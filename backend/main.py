import json
from collections import Counter
from fractions import Fraction
from itertools import product
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
def _collect_bounds(raw_node: dict, bounds: Dict[str, int] | None = None) -> Dict[str, int]:
    bounds = {} if bounds is None else bounds
    if raw_node["type"] == "Loop":
        bounds[raw_node["var"]] = raw_node["bound"]
        for child in raw_node["body"]:
            _collect_bounds(child, bounds)
    return bounds
def _predict_cold_misses(raw_node: dict) -> set[str]:
    bounds = _collect_bounds(raw_node)
    cold: set[str] = set()
    def visit(node: dict) -> None:
        if node["type"] == "Loop":
            for child in node["body"]:
                visit(child)
        elif node["type"] == "Scalar":
            cold.add(node["name"])
        elif node["type"] == "Array":
            vars_seen = []
            for idx in node["indices"]:
                if idx in bounds and idx not in vars_seen:
                    vars_seen.append(idx)
            ranges = [range(bounds[var]) for var in vars_seen]
            for values in product(*ranges) if ranges else [()]:
                env = dict(zip(vars_seen, values))
                indices = [str(env[idx]) if idx in env else idx for idx in node["indices"]]
                cold.add(node["name"] + "-" + "-".join(indices))

    visit(raw_node)
    return cold
def _consecutive_groups(values: List[int]) -> List[List[int]]:
    groups: List[List[int]] = []
    for value in sorted(values):
        if not groups or value != groups[-1][-1] + 1:
            groups.append([value])
        else:
            groups[-1].append(value)
    return groups
def _interp(points: List[Tuple[int, int]], target: int) -> int:
    if not points:
        return 0
    total = Fraction(0)
    for i, (xi, yi) in enumerate(points):
        term = Fraction(yi)
        for j, (xj, _) in enumerate(points):
            if i != j:
                term *= Fraction(target - xj, xi - xj)
        total += term
    return round(total.numerator / total.denominator)
def _predict_series(points: List[Tuple[int, int]], target: int) -> int:
    for start in range(len(points) - 2):
        suffix = points[start:]
        values = [y for _, y in suffix]
        if len(suffix) >= 3:
            diffs = [b - a for a, b in zip(values, values[1:])]
            if len(set(diffs)) == 1:
                return _interp(suffix, target)
        if len(suffix) >= 4:
            diffs = [b - a for a, b in zip(values, values[1:])]
            diff2 = [b - a for a, b in zip(diffs, diffs[1:])]
            if len(set(diff2)) == 1:
                return _interp(suffix, target)
    return _interp(points, target)
def _group_mode(row: List[int]) -> int:
    counts = Counter(row)
    max_count = max(counts.values())
    return min(value for value, count in counts.items() if count == max_count)
def _sample_volatile_groups(
    raw_node: dict,
    sample_ns: List[int],
    stable_rds: set[int],
) -> List[Tuple[int, List[List[int]]]]:
    sampled = []
    for n in sample_ns:
        profile, _ = _run_sim(raw_node, [n, n, n])
        volatile = [rd for rd in profile.histogram if rd not in stable_rds]
        sampled.append((n, _consecutive_groups(volatile)))
    return sampled
def _predict_group_freq(
    rows: List[Tuple[int, List[int]]],
    target_n: int,
    target_size: int,
) -> List[int]:
    modes = [(n, _group_mode(row)) for n, row in rows]
    fill = max(0, _predict_series(modes, target_n))
    result = [fill] * target_size
    max_sample_size = max(len(row) for _, row in rows)
    for offset in range(min(2, max_sample_size)):
        points = [(n, row[offset]) for n, row in rows if offset < len(row)]
        mode_points = [(n, _group_mode(row)) for n, row in rows if offset < len(row)]
        if len(points) >= 2 and any(y != m for (_, y), (_, m) in zip(points[-2:], mode_points[-2:])):
            if offset < target_size:
                result[offset] = max(0, _predict_series(points, target_n))
    for tail in range(min(3, max_sample_size)):
        points = [(n, row[-1 - tail]) for n, row in rows if tail < len(row)]
        mode_points = [(n, _group_mode(row)) for n, row in rows if tail < len(row)]
        if len(points) >= 2 and any(y != m for (_, y), (_, m) in zip(points[-2:], mode_points[-2:])):
            if tail < target_size:
                result[-1 - tail] = max(0, _predict_series(points, target_n))
    return result
def _predict_volatile_diagonal(
    raw_node: dict,
    stable_rds: set[int],
    target_n: int,
) -> ReuseProfile | None:
    sample_ns = [3, 4, 5, 6, 7]
    sampled = _sample_volatile_groups(raw_node, sample_ns, stable_rds)
    group_count = len(sampled[0][1])
    if group_count == 0 or any(len(groups) != group_count for _, groups in sampled):
        return None
    profile = ReuseProfile()
    for index in range(group_count):
        if target_n in sample_ns:
            groups = dict(sampled)[target_n]
            hist = _run_sim(raw_node, [target_n, target_n, target_n])[0].histogram
            for rd in groups[index]:
                profile.histogram[rd] = hist[rd]
            continue

        starts = [(n, groups[index][0]) for n, groups in sampled]
        sizes = [(n, len(groups[index])) for n, groups in sampled]
        start = _predict_series(starts, target_n)
        size = _predict_series(sizes, target_n)
        if size <= 0:
            return None
        rows = []
        for n, groups in sampled:
            hist = _run_sim(raw_node, [n, n, n])[0].histogram
            rows.append((n, [hist[rd] for rd in groups[index]]))
        freqs = _predict_group_freq(rows, target_n, size)
        for offset, freq in enumerate(freqs):
            profile.histogram[start + offset] = freq
    return profile
def _predict_1d(raw_node: dict) -> Tuple[ReuseProfile, List[str]]:
    predicted, trace = _run_sim(raw_node, [2])
    predicted.cold_misses = _predict_cold_misses(raw_node)
    return predicted, trace

def _predict_2d(raw_node: dict) -> Tuple[ReuseProfile, List[str]]:
    b22, trace = _run_sim(raw_node, [2, 2])
    b32, _     = _run_sim(raw_node, [3, 2])
    b23, _     = _run_sim(raw_node, [2, 3])
    b33, _     = _run_sim(raw_node, [3, 3])

    stable_rds = (set(b22.histogram) & set(b32.histogram)
                  & set(b23.histogram) & set(b33.histogram))
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
            volatile = _predict_volatile_diagonal(raw_node, stable_rds, outer.actual_bound)
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
