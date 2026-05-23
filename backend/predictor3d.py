from typing import Callable, Dict, List, Tuple

from dilation import DilationContextBuilder, DilationPredictor
from lru_sim import ReuseProfile
from parser import LoopBlockNode, parse_trace
from volatile import predict_volatile_diagonal
from volatile3d import predict_volatile_3d_rectangular

RunSim = Callable[[dict, List[int]], Tuple[ReuseProfile, List[str]]]
ColdPredictor = Callable[[dict], set[str]]


def _diff(a: Dict[int, int], b: Dict[int, int], rds) -> Dict[int, int]:
    return {rd: a.get(rd, 0) - b.get(rd, 0) for rd in rds}


def predict_3d(
    raw_node: dict,
    run_sim: RunSim,
    predict_cold_misses: ColdPredictor,
) -> Tuple[ReuseProfile, List[str]]:
    sims = {
        (i, j, k): run_sim(raw_node, [i, j, k])
        for i in (2, 3) for j in (2, 3) for k in (2, 3)
    }

    def h(key: tuple) -> Dict[int, int]:
        return sims[key][0].histogram

    b222 = sims[(2, 2, 2)][0]
    trace = sims[(2, 2, 2)][1]
    rds = set().union(*(set(p.histogram) for p, _ in sims.values()))
    stable_rds = set.intersection(*(set(p.histogram) for p, _ in sims.values()))
    stable_b222 = ReuseProfile()
    stable_b222.histogram = {rd: v for rd, v in b222.histogram.items() if rd in stable_rds}
    stable_b222.cold_misses = b222.cold_misses

    incr_i = _diff(h((3, 2, 2)), h((2, 2, 2)), rds)
    incr_j = _diff(h((2, 3, 2)), h((2, 2, 2)), rds)
    incr_k = _diff(h((2, 2, 3)), h((2, 2, 2)), rds)
    coff_ij = {
        rd: h((3, 3, 2)).get(rd, 0)
        - h((2, 2, 2)).get(rd, 0)
        - incr_i.get(rd, 0)
        - incr_j.get(rd, 0)
        for rd in rds
    }
    coff_jk = {
        rd: h((2, 3, 3)).get(rd, 0)
        - h((2, 2, 2)).get(rd, 0)
        - incr_j.get(rd, 0)
        - incr_k.get(rd, 0)
        for rd in rds
    }
    coff_ik = {
        rd: h((3, 2, 3)).get(rd, 0)
        - h((2, 2, 2)).get(rd, 0)
        - incr_i.get(rd, 0)
        - incr_k.get(rd, 0)
        for rd in rds
    }
    coff_ijk = {
        rd: h((3, 3, 3)).get(rd, 0)
        - h((2, 2, 2)).get(rd, 0)
        - incr_i.get(rd, 0)
        - incr_j.get(rd, 0)
        - incr_k.get(rd, 0)
        - coff_ij.get(rd, 0)
        - coff_jk.get(rd, 0)
        - coff_ik.get(rd, 0)
        for rd in rds
    }
    nodes = parse_trace([raw_node], sim_bound=2)
    outer = nodes[0]
    mid = next(n for n in outer.body if isinstance(n, LoopBlockNode))
    inner = next(n for n in mid.body if isinstance(n, LoopBlockNode))

    predicted = DilationPredictor(3).execute(
        DilationContextBuilder()
        .set_target_bounds({"i": outer.actual_bound, "j": mid.actual_bound, "k": inner.actual_bound})
        .set_base_profile(stable_b222)
        .add_coefficient("Incr_I", incr_i)
        .add_coefficient("Incr_J", incr_j)
        .add_coefficient("Incr_K", incr_k)
        .add_coefficient("Coff_IJ", coff_ij)
        .add_coefficient("Coff_JK", coff_jk)
        .add_coefficient("Coff_IK", coff_ik)
        .add_coefficient("Coff_IJK", coff_ijk)
    )
    if stable_rds != rds:
        bounds = {outer.actual_bound, mid.actual_bound, inner.actual_bound}
        volatile = None
        if len(bounds) == 1:
            volatile = predict_volatile_diagonal(raw_node, stable_rds, outer.actual_bound, run_sim)
        if volatile is None:
            volatile = predict_volatile_3d_rectangular(
                raw_node,
                stable_rds,
                outer.actual_bound,
                mid.actual_bound,
                inner.actual_bound,
                run_sim,
            )
        if volatile is not None:
            predicted.histogram.update(volatile.histogram)

    predicted.cold_misses = predict_cold_misses(raw_node)
    return predicted, trace
