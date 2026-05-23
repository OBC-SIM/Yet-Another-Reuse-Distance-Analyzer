import hashlib
import json
import os
from time import perf_counter
from typing import Dict, List, Set, Tuple

from lru_sim import LRUProfiler, ReuseProfile
from predictor import _apply_sim_bounds
from parser import LoopBlockNode, index_variable, parse_trace, resolve_index


GT_CACHE_VERSION = 3
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


def _collect_starts(raw_node: dict) -> Dict[str, int]:
    starts: Dict[str, int] = {}
    if raw_node["type"] == "Loop":
        starts[raw_node["var"]] = raw_node.get("start", 0)
        for child in raw_node["body"]:
            starts.update(_collect_starts(child))
    return starts


def _collect_arrays(raw_node: dict, seen: Dict[str, List[List[str]]] | None = None) -> Dict[str, List[List[str]]]:
    if seen is None:
        seen = {}
    if raw_node["type"] == "Array":
        name = raw_node["name"]
        seen.setdefault(name, []).append(raw_node["indices"])
    elif raw_node["type"] == "Loop":
        for child in raw_node["body"]:
            _collect_arrays(child, seen)
    return seen


def _index_extent(index: str, var_bounds: Dict[str, int], starts: Dict[str, int]) -> Tuple[int, int]:
    var = index_variable(index)
    if var in var_bounds:
        start = starts.get(var, 0)
        end = var_bounds[var] - 1
        lo = int(resolve_index(index, {var: start}))
        hi = int(resolve_index(index, {var: end}))
        return min(lo, hi), max(lo, hi)
    value = int(resolve_index(index, {}))
    return value, value


def _compute_addr_maps(raw_node: dict) -> Tuple[Dict[str, int], Dict[str, int], Dict[str, Tuple[int, int]]]:
    """bases[name] = 누적 원소 수 오프셋, cols[name] = 두 번째 차원 크기."""
    var_bounds = _collect_var_bounds(raw_node)
    starts = _collect_starts(raw_node)
    arrays = _collect_arrays(raw_node)
    bases: Dict[str, int] = {}
    cols: Dict[str, int] = {}
    lows: Dict[str, Tuple[int, int]] = {}
    offset = 0
    for name, accesses in arrays.items():
        bases[name] = offset
        dims = max((len(indices) for indices in accesses), default=0)
        extents = []
        for dim in range(dims):
            bounds = [
                _index_extent(indices[dim], var_bounds, starts)
                for indices in accesses
                if dim < len(indices)
            ]
            lo = min(bound[0] for bound in bounds)
            hi = max(bound[1] for bound in bounds)
            extents.append(hi - lo + 1)
            if dim == 0:
                lows[name] = (lo, 0)
            elif dim == 1:
                lows[name] = (lows.get(name, (0, 0))[0], lo)
        dim0 = extents[0] if len(extents) > 0 else 1
        dim1 = extents[1] if len(extents) > 1 else 1
        lows.setdefault(name, (0, 0))
        cols[name] = dim1
        offset += dim0 * dim1
    return bases, cols, lows


def _to_int_trace(
    sym_trace: List[str],
    bases: Dict[str, int],
    cols: Dict[str, int],
    lows: Dict[str, Tuple[int, int]],
) -> List[str]:
    """symbolic 트레이스("A-1-2")를 정수 선형 주소 문자열로 변환."""
    def parse_coords(rest: str) -> Tuple[int, int] | None:
        for pos, char in enumerate(rest):
            if char != "-" or pos == 0:
                continue
            left, right = rest[:pos], rest[pos + 1:]
            try:
                row, col = int(left), int(right)
            except ValueError:
                continue
            if f"{row}-{col}" == rest:
                return row, col
        return None

    result = []
    for sym in sym_trace:
        mapped = False
        for name in bases:
            prefix = f"{name}-"
            if not sym.startswith(prefix):
                continue
            coords = parse_coords(sym[len(prefix):])
            if coords is None:
                continue
            row, col = coords
            row_lo, col_lo = lows.get(name, (0, 0))
            result.append(str(bases[name] + (row - row_lo) * cols[name] + (col - col_lo)))
            mapped = True
            break
        if not mapped:
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
    bases, cols, lows = _compute_addr_maps(raw_node)
    return LRUProfiler.calculate(_to_int_trace(sym_trace, bases, cols, lows))


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
