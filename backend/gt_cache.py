import hashlib
import json
import os
from time import perf_counter
from typing import Dict, List, Set, Tuple

from lru_sim import LRUProfiler, ReuseProfile
from block_trace import function_trace
from predictor import _apply_sim_bounds
from parser import LoopBlockNode, parse_trace


GT_CACHE_VERSION = 2
GT_CACHE_PATH = os.environ.get(
    "VERIFY_GT_CACHE",
    os.path.join(os.path.dirname(__file__), ".verify_gt_cache.json"),
)
_GT_CACHE: Dict[str, dict] | None = None


def _load_gt_cache() -> Dict[str, dict]:
    global _GT_CACHE
    if _GT_CACHE is not None:
        return _GT_CACHE
    try:
        with open(GT_CACHE_PATH, "r", encoding="utf-8") as f:
            _GT_CACHE = json.load(f)
    except (FileNotFoundError, json.JSONDecodeError):
        _GT_CACHE = {}
    return _GT_CACHE


def _save_gt_cache() -> None:
    if _GT_CACHE is None:
        return
    tmp_path = f"{GT_CACHE_PATH}.{os.getpid()}.tmp"
    with open(tmp_path, "w", encoding="utf-8") as f:
        json.dump(_GT_CACHE, f, sort_keys=True)
    os.replace(tmp_path, GT_CACHE_PATH)


def _gt_cache_key(raw_node: dict) -> str:
    payload = {"version": GT_CACHE_VERSION, "raw": raw_node}
    encoded = json.dumps(payload, sort_keys=True, separators=(",", ":"))
    return hashlib.sha256(encoded.encode("utf-8")).hexdigest()


def _function_gt_cache_key(func_entry: dict) -> str:
    payload = {"version": GT_CACHE_VERSION, "function": func_entry}
    encoded = json.dumps(payload, sort_keys=True, separators=(",", ":"))
    return hashlib.sha256(encoded.encode("utf-8")).hexdigest()


def _profile_from_cache(data: dict) -> ReuseProfile:
    profile = ReuseProfile()
    profile.histogram = {int(k): v for k, v in data["histogram"].items()}
    profile.cold_misses = set(data["cold_misses"])
    return profile


def _profile_to_cache(profile: ReuseProfile, unroll_seconds: float) -> dict:
    return {
        "histogram": {str(k): v for k, v in profile.histogram.items()},
        "cold_misses": sorted(profile.cold_misses),
        "unroll_seconds": unroll_seconds,
    }


def _collect_var_bounds(raw_node: dict) -> Dict[str, int]:
    bounds: Dict[str, int] = {}
    if raw_node["type"] == "Loop":
        bounds[raw_node["var"]] = raw_node["bound"]
        for child in raw_node["body"]:
            bounds.update(_collect_var_bounds(child))
    return bounds


def _collect_arrays(raw_node: dict, seen: Dict[str, List[str]] | None = None) -> Dict[str, List[str]]:
    if seen is None:
        seen = {}
    if raw_node["type"] == "Array":
        name = raw_node["name"]
        if name not in seen:
            seen[name] = raw_node["indices"]
    elif raw_node["type"] == "Loop":
        for child in raw_node["body"]:
            _collect_arrays(child, seen)
    return seen


def _compute_addr_maps(raw_node: dict) -> Tuple[Dict[str, int], Dict[str, int]]:
    """bases[name] = 누적 원소 수 오프셋, cols[name] = 두 번째 차원 크기."""
    var_bounds = _collect_var_bounds(raw_node)
    arrays = _collect_arrays(raw_node)
    bases: Dict[str, int] = {}
    cols: Dict[str, int] = {}
    offset = 0
    for name, indices in arrays.items():
        bases[name] = offset
        dim0 = var_bounds.get(indices[0], 1) if len(indices) > 0 else 1
        dim1 = var_bounds.get(indices[1], 1) if len(indices) > 1 else 1
        cols[name] = dim1
        offset += dim0 * dim1
    return bases, cols


def _to_int_trace(
    sym_trace: List[str],
    bases: Dict[str, int],
    cols: Dict[str, int],
) -> List[str]:
    """symbolic 트레이스("A-1-2")를 정수 선형 주소 문자열로 변환."""
    result = []
    for sym in sym_trace:
        parts = sym.split("-")
        name = parts[0]
        if name in bases and len(parts) >= 3:
            row, col = int(parts[1]), int(parts[2])
            result.append(str(bases[name] + row * cols[name] + col))
        else:
            result.append(sym)
    return result


def ground_truth(raw_node: dict) -> ReuseProfile:
    nodes = parse_trace([raw_node], sim_bound=2)
    loop = nodes[0]
    actual_bounds: List[int] = []
    n = loop
    while isinstance(n, LoopBlockNode):
        actual_bounds.append(n.actual_bound)
        n = next((c for c in n.body if isinstance(c, LoopBlockNode)), None)
    _apply_sim_bounds(loop, actual_bounds)
    sym_trace = loop.unroll({})
    bases, cols = _compute_addr_maps(raw_node)
    return LRUProfiler.calculate(_to_int_trace(sym_trace, bases, cols))


def ground_truth_cached(raw_node: dict) -> Tuple[ReuseProfile, bool, float]:
    """
    @return (profile, cache_hit, unroll_seconds)
    """
    cache = _load_gt_cache()
    key = _gt_cache_key(raw_node)
    if key in cache:
        data = cache[key]
        return _profile_from_cache(data), True, data.get("unroll_seconds", 0.0)

    start = perf_counter()
    profile = ground_truth(raw_node)
    unroll_seconds = perf_counter() - start
    cache[key] = _profile_to_cache(profile, unroll_seconds)
    _save_gt_cache()
    return profile, False, unroll_seconds


def function_ground_truth(func_entry: dict) -> ReuseProfile:
    return LRUProfiler.calculate(function_trace(func_entry))


def function_ground_truth_cached(func_entry: dict) -> Tuple[ReuseProfile, bool, float]:
    """
    @return (profile, cache_hit, unroll_seconds)
    """
    cache = _load_gt_cache()
    key = _function_gt_cache_key(func_entry)
    if key in cache:
        data = cache[key]
        return _profile_from_cache(data), True, data.get("unroll_seconds", 0.0)

    start = perf_counter()
    profile = function_ground_truth(func_entry)
    unroll_seconds = perf_counter() - start
    cache[key] = _profile_to_cache(profile, unroll_seconds)
    _save_gt_cache()
    return profile, False, unroll_seconds
