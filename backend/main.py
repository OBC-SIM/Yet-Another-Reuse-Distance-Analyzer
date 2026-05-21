import json
from typing import Dict, List, Tuple

from dilation import DilationContextBuilder, DilationPredictor
from lru_sim import LRUProfiler, ReuseProfile
from merger import BlockMerger
from parser import LoopBlockNode, parse_trace


def _get_loop_depth(raw_node: dict) -> int:
    if raw_node["type"] != "Loop":
        return 0
    max_depth = 0
    for child in raw_node["body"]:
        if child["type"] == "Loop":
            max_depth = max(max_depth, _get_loop_depth(child))
    return 1 + max_depth


def _apply_sim_bounds(loop: LoopBlockNode, sim_bounds: List[int], level: int = 0) -> None:
    loop.sim_bound = sim_bounds[level]
    if level + 1 < len(sim_bounds):
        for child in loop.body:
            if isinstance(child, LoopBlockNode):
                _apply_sim_bounds(child, sim_bounds, level + 1)
                break


def _run_sim(raw_node: dict, sim_bounds: List[int]) -> Tuple[ReuseProfile, List[str]]:
    nodes = parse_trace([raw_node], sim_bound=2)
    _apply_sim_bounds(nodes[0], sim_bounds)
    trace = nodes[0].unroll({})
    return LRUProfiler.calculate(trace), trace


def _diff(a: Dict[int, int], b: Dict[int, int], rds) -> Dict[int, int]:
    return {rd: a.get(rd, 0) - b.get(rd, 0) for rd in rds}


def _predict_1d(raw_node: dict) -> Tuple[ReuseProfile, List[str]]:
    return _run_sim(raw_node, [2])


def _predict_2d(raw_node: dict) -> Tuple[ReuseProfile, List[str]]:
    b22, trace = _run_sim(raw_node, [2, 2])
    b32, _     = _run_sim(raw_node, [3, 2])
    b23, _     = _run_sim(raw_node, [2, 3])
    b33, _     = _run_sim(raw_node, [3, 3])

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
        .set_base_profile(b22)
        .add_coefficient("Incr_J", incr_j)
        .add_coefficient("Incr_K", incr_k)
        .add_coefficient("Coff_JK", coff_jk)
    )
    predicted.cold_misses = b22.cold_misses
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
        .set_base_profile(b222)
        .add_coefficient("Incr_I",   incr_i)
        .add_coefficient("Incr_J",   incr_j)
        .add_coefficient("Incr_K",   incr_k)
        .add_coefficient("Coff_IJ",  coff_ij)
        .add_coefficient("Coff_JK",  coff_jk)
        .add_coefficient("Coff_IK",  coff_ik)
        .add_coefficient("Coff_IJK", coff_ijk)
    )
    predicted.cold_misses = b222.cold_misses
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


def analyze(json_path: str) -> ReuseProfile:
    with open(json_path) as f:
        raw = json.load(f)

    merger = BlockMerger()
    for func_entry in raw:
        for node in func_entry["body"]:
            if node["type"] == "Loop":
                block_profile, block_trace = _predict_loop_block(node)
                merger.merge_block(block_profile, block_trace)
            else:
                merger.adjust_cold_misses({node["name"]})

    return merger.global_profile
