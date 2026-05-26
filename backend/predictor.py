import json
from itertools import product
from typing import Dict, List, Tuple

from block_trace import unroll_node_actual
from calls import expand_calls
from dilation import DilationContextBuilder, DilationPredictor
from lru_sim import LRUProfiler, ReuseProfile
from merger import BlockMerger
from parser import LoopBlockNode, parse_trace, index_variable, resolve_index
from sequence_summary import summarize_sequence
from stability import validated_stable_rds_2d
from volatile import predict_volatile_3d_rectangular, predict_volatile_diagonal
from volatile2d import predict_volatile_2d_rectangular


def _get_loop_depth(raw_node: dict) -> int:
    if raw_node["type"] != "Loop":
        return 0
    max_depth = 0
    for child in raw_node["body"]:
        if child["type"] == "Loop":
            max_depth = max(max_depth, _get_loop_depth(child))
    return 1 + max_depth


def _apply_sim_bounds(loop: LoopBlockNode, sim_bounds: List[int], level: int = 0) -> None:
    loop.sim_bound = min(sim_bounds[level], loop.actual_bound)
    if level + 1 < len(sim_bounds):
        for child in loop.body:
            if isinstance(child, LoopBlockNode):
                _apply_sim_bounds(child, sim_bounds, level + 1)


def _run_sim(raw_node: dict, sim_bounds: List[int]) -> Tuple[ReuseProfile, List[str]]:
    nodes = parse_trace([raw_node], sim_bound=2)
    _apply_sim_bounds(nodes[0], sim_bounds)
    trace = nodes[0].unroll({})
    return LRUProfiler.calculate(trace), trace


def _unroll_with_actual_bounds(raw_node: dict) -> List[str]:
    return unroll_node_actual(raw_node)


def _diff(a: Dict[int, int], b: Dict[int, int], rds) -> Dict[int, int]:
    return {rd: a.get(rd, 0) - b.get(rd, 0) for rd in rds}


def _collect_bounds(raw_node: dict, bounds: Dict[str, int] | None = None) -> Dict[str, int]:
    bounds = {} if bounds is None else bounds
    if raw_node["type"] == "Loop":
        bounds[raw_node["var"]] = raw_node["bound"]
        for child in raw_node["body"]:
            _collect_bounds(child, bounds)
    return bounds


def _collect_starts(raw_node: dict, starts: Dict[str, int] | None = None) -> Dict[str, int]:
    starts = {} if starts is None else starts
    if raw_node["type"] == "Loop":
        starts[raw_node["var"]] = raw_node.get("start", 0)
        for child in raw_node["body"]:
            _collect_starts(child, starts)
    return starts


def _predict_cold_misses(raw_node: dict) -> set[str]:
    bounds = _collect_bounds(raw_node)
    starts = _collect_starts(raw_node)
    cold: set[str] = set()
    def visit(node: dict) -> None:
        if node["type"] == "Loop":
            if node["bound"] <= 0:
                return
            for child in node["body"]:
                visit(child)
        elif node["type"] == "Scalar":
            cold.add(node["name"])
        elif node["type"] == "Array":
            vars_seen = []
            for idx in node["indices"]:
                var = index_variable(idx)
                if var in bounds and var not in vars_seen:
                    vars_seen.append(var)
            ranges = [range(starts.get(var, 0), bounds[var]) for var in vars_seen]
            for values in product(*ranges) if ranges else [()]:
                env = dict(zip(vars_seen, values))
                indices = [resolve_index(idx, env) for idx in node["indices"]]
                cold.add(node["name"] + "-" + "-".join(indices))

    visit(raw_node)
    return cold


def _predict_1d(raw_node: dict) -> Tuple[ReuseProfile, List[str]]:
    actual_bound = max(0, raw_node["bound"] - raw_node.get("start", 0))
    if actual_bound <= 1:
        predicted, trace = _run_sim(raw_node, [actual_bound])
        predicted.cold_misses = _predict_cold_misses(raw_node)
        return predicted, trace

    base, _ = _run_sim(raw_node, [1])
    b2, _ = _run_sim(raw_node, [2])
    rds = set(base.histogram) | set(b2.histogram)
    trace = _unroll_with_actual_bounds(raw_node)

    predicted = ReuseProfile()
    for rd in rds:
        increment = b2.histogram.get(rd, 0) - base.histogram.get(rd, 0)
        freq = base.histogram.get(rd, 0) + (actual_bound - 1) * increment
        if freq > 0:
            predicted.histogram[rd] = freq
    predicted.cold_misses = _predict_cold_misses(raw_node)
    return predicted, trace


def _predict_2d(raw_node: dict) -> Tuple[ReuseProfile, List[str]]:
    sims = {
        (j, k): _run_sim(raw_node, [j, k])
        for j, k in ((2, 2), (3, 2), (2, 3), (3, 3), (4, 4))
    }
    b22, trace = sims[(2, 2)]
    b32, _     = sims[(3, 2)]
    b23, _     = sims[(2, 3)]
    b33, _     = sims[(3, 3)]
    hist_by_bound = {bound: profile.histogram for bound, (profile, _) in sims.items()}

    stable_rds = validated_stable_rds_2d(hist_by_bound)
    stable_b22 = ReuseProfile()
    stable_b22.histogram = {rd: v for rd, v in b22.histogram.items() if rd in stable_rds}
    stable_b22.cold_misses = b22.cold_misses

    rds = set(b22.histogram) | set(b32.histogram) | set(b23.histogram) | set(b33.histogram)
    incr_j  = _diff(b32.histogram, b22.histogram, rds)
    incr_k  = _diff(b23.histogram, b22.histogram, rds)
    coff_jk = {
        rd: b33.histogram.get(rd, 0) - b22.histogram.get(rd, 0)
            - incr_j.get(rd, 0) - incr_k.get(rd, 0)
        for rd in rds
    }
    nodes = parse_trace([raw_node], sim_bound=2)
    outer = nodes[0]
    inner = next(n for n in outer.body if isinstance(n, LoopBlockNode))

    predicted = DilationPredictor(2).execute(
        DilationContextBuilder()
        .set_target_bounds({"j": outer.actual_bound, "k": inner.actual_bound})
        .set_base_profile(stable_b22)
        .add_coefficient("Incr_J", incr_j)
        .add_coefficient("Incr_K", incr_k)
        .add_coefficient("Coff_JK", coff_jk)
    )
    if stable_rds != rds:
        volatile = predict_volatile_2d_rectangular(
            raw_node,
            stable_rds,
            outer.actual_bound,
            inner.actual_bound,
            _run_sim,
        )
        if volatile is not None:
            predicted.histogram.update(volatile.histogram)
    predicted.cold_misses = _predict_cold_misses(raw_node)
    return predicted, trace


def _predict_3d(raw_node: dict) -> Tuple[ReuseProfile, List[str]]:
    sims = {
        (i, j, k): _run_sim(raw_node, [i, j, k])
        for i in (2, 3) for j in (2, 3) for k in (2, 3)
    }
    def h(key: tuple) -> Dict[int, int]:
        return sims[key][0].histogram
    b222  = sims[(2, 2, 2)][0]
    trace = sims[(2, 2, 2)][1]
    rds   = set().union(*(set(p.histogram) for p, _ in sims.values()))
    stable_rds = set.intersection(*(set(p.histogram) for p, _ in sims.values()))
    stable_b222 = ReuseProfile()
    stable_b222.histogram = {rd: v for rd, v in b222.histogram.items() if rd in stable_rds}
    stable_b222.cold_misses = b222.cold_misses

    incr_i  = _diff(h((3,2,2)), h((2,2,2)), rds)
    incr_j  = _diff(h((2,3,2)), h((2,2,2)), rds)
    incr_k  = _diff(h((2,2,3)), h((2,2,2)), rds)
    coff_ij = {rd: h((3,3,2)).get(rd,0) - h((2,2,2)).get(rd,0) - incr_i.get(rd,0) - incr_j.get(rd,0) for rd in rds}
    coff_jk = {rd: h((2,3,3)).get(rd,0) - h((2,2,2)).get(rd,0) - incr_j.get(rd,0) - incr_k.get(rd,0) for rd in rds}
    coff_ik = {rd: h((3,2,3)).get(rd,0) - h((2,2,2)).get(rd,0) - incr_i.get(rd,0) - incr_k.get(rd,0) for rd in rds}
    coff_ijk = {
        rd: h((3,3,3)).get(rd,0) - h((2,2,2)).get(rd,0)
            - incr_i.get(rd,0) - incr_j.get(rd,0) - incr_k.get(rd,0)
            - coff_ij.get(rd,0) - coff_jk.get(rd,0) - coff_ik.get(rd,0)
        for rd in rds
    }
    nodes = parse_trace([raw_node], sim_bound=2)
    outer = nodes[0]
    mid   = next(n for n in outer.body if isinstance(n, LoopBlockNode))
    inner = next(n for n in mid.body   if isinstance(n, LoopBlockNode))

    predicted = DilationPredictor(3).execute(
        DilationContextBuilder()
        .set_target_bounds({"i": outer.actual_bound, "j": mid.actual_bound, "k": inner.actual_bound})
        .set_base_profile(stable_b222)
        .add_coefficient("Incr_I",   incr_i)
        .add_coefficient("Incr_J",   incr_j)
        .add_coefficient("Incr_K",   incr_k)
        .add_coefficient("Coff_IJ",  coff_ij)
        .add_coefficient("Coff_JK",  coff_jk)
        .add_coefficient("Coff_IK",  coff_ik)
        .add_coefficient("Coff_IJK", coff_ijk)
    )
    if stable_rds != rds:
        bounds = {outer.actual_bound, mid.actual_bound, inner.actual_bound}
        volatile = None
        if len(bounds) == 1:
            volatile = predict_volatile_diagonal(raw_node, stable_rds, outer.actual_bound, _run_sim)
        if volatile is None:
            volatile = predict_volatile_3d_rectangular(
                raw_node,
                stable_rds,
                outer.actual_bound,
                mid.actual_bound,
                inner.actual_bound,
                _run_sim,
            )
        if volatile is not None:
            predicted.histogram.update(volatile.histogram)

    predicted.cold_misses = _predict_cold_misses(raw_node)
    return predicted, trace


def _predict_loop_block(raw_node: dict) -> Tuple[ReuseProfile, List[str]]:
    depth = _get_loop_depth(raw_node)
    if depth == 1:
        return _predict_1d(raw_node)
    if depth == 2:
        return _predict_2d(raw_node)
    if depth == 3:
        return _predict_3d(raw_node)
    raise NotImplementedError(f"{depth}D loop not supported")


def analyze_blocks(json_path: str) -> List[Tuple[str, ReuseProfile]]:
    """LAT JSON의 블록별 예측 ReuseProfile을 (블록명, 프로파일) 리스트로 반환.

    @param json_path  _lat.json 경로
    @return           블록명은 "func  var-loop (bound=N)" 또는 "func  (flat, N accesses)" 형태
    """
    with open(json_path) as f:
        raw = expand_calls(json.load(f))

    results: List[Tuple[str, ReuseProfile]] = []
    def flush_flat(func_name: str, trace: List[str]) -> None:
        if trace:
            profile = LRUProfiler.calculate(trace)
            name = f"{func_name}  (flat, {len(trace)} accesses)"
            results.append((name, profile))
            trace.clear()

    for func_entry in raw:
        func_name = func_entry["function"]
        flat_trace: List[str] = []
        for node in func_entry["body"]:
            if node["type"] == "Loop":
                flush_flat(func_name, flat_trace)
                profile, _ = _predict_loop_block(node)
                name = f"{func_name}  {node['var']}-loop (bound={node['bound']})"
                results.append((name, profile))
            else:
                flat_trace.extend(unroll_node_actual(node))
        flush_flat(func_name, flat_trace)

    return results


def analyze(json_path: str) -> ReuseProfile:
    with open(json_path) as f:
        raw = expand_calls(json.load(f))

    merger = BlockMerger()
    for func_entry in raw:
        for node in func_entry["body"]:
            if node["type"] == "Loop":
                block_profile, block_trace = _predict_loop_block(node)
                sequence = summarize_sequence(node)
                if sequence:
                    merger.merge_sequence(block_profile, sequence, block_trace)
                else:
                    merger.merge_block(block_profile, block_trace)
            else:
                block_trace = parse_trace([node])[0].unroll({})
                block_profile = LRUProfiler.calculate(block_trace)
                merger.merge_block(block_profile, block_trace)

    return merger.global_profile
