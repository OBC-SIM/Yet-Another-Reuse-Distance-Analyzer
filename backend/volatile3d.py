from typing import List, Set, Tuple

from lru_sim import ReuseProfile
from volatile import (
    RunSim,
    _consecutive_groups,
    _group_mode,
)
from volatile3d_variable import predict_volatile_3d_variable


def _predict_trilinear(points: List[Tuple[int, int, int, int]],
                       target_i: int, target_j: int, target_k: int) -> int:
    values = {(i, j, k): value for i, j, k, value in points}
    i0 = min(i for i, _, _, _ in points)
    j0 = min(j for _, j, _, _ in points)
    k0 = min(k for _, _, k, _ in points)
    i1 = min(i for i, _, _, _ in points if i > i0)
    j1 = min(j for _, j, _, _ in points if j > j0)
    k1 = min(k for _, _, k, _ in points if k > k0)
    b = values[(i0, j0, k0)]
    di, dj, dk = target_i - i0, target_j - j0, target_k - k0
    incr_i = values[(i1, j0, k0)] - b
    incr_j = values[(i0, j1, k0)] - b
    incr_k = values[(i0, j0, k1)] - b
    coff_ij = values[(i1, j1, k0)] - b - incr_i - incr_j
    coff_ik = values[(i1, j0, k1)] - b - incr_i - incr_k
    coff_jk = values[(i0, j1, k1)] - b - incr_j - incr_k
    coff_ijk = (
        values[(i1, j1, k1)]
        - b
        - incr_i
        - incr_j
        - incr_k
        - coff_ij
        - coff_ik
        - coff_jk
    )
    return (
        b
        + di * incr_i
        + dj * incr_j
        + dk * incr_k
        + di * dj * coff_ij
        + di * dk * coff_ik
        + dj * dk * coff_jk
        + di * dj * dk * coff_ijk
    )


def _sample_volatile_groups_3d(
    raw_node: dict,
    sample_ns: List[int],
    stable_rds: Set[int],
    run_sim: RunSim,
) -> List[Tuple[int, int, int, List[List[int]]]]:
    sampled = []
    for i in sample_ns:
        for j in sample_ns:
            for k in sample_ns:
                profile, _ = run_sim(raw_node, [i, j, k])
                volatile = [rd for rd in profile.histogram if rd not in stable_rds]
                sampled.append((i, j, k, _consecutive_groups(volatile)))
    return sampled


def _sample_hist_3d(raw_node: dict, sample_ns: List[int], run_sim: RunSim):
    return {
        (i, j, k): run_sim(raw_node, [i, j, k])[0].histogram
        for i in sample_ns for j in sample_ns for k in sample_ns
    }


def _predict_group_freq_3d(rows: List[Tuple[int, int, int, List[int]]],
                           target_i: int, target_j: int,
                           target_k: int, target_size: int) -> List[int]:
    modes = [(i, j, k, _group_mode(row)) for i, j, k, row in rows]
    fill = max(0, _predict_trilinear(modes, target_i, target_j, target_k))
    result = [fill] * target_size
    max_sample_size = max(len(row) for _, _, _, row in rows)
    for offset in range(min(2, max_sample_size)):
        points = [(i, j, k, row[offset]) for i, j, k, row in rows if offset < len(row)]
        if len(points) >= 8 and offset < target_size:
            result[offset] = max(0, _predict_trilinear(points, target_i, target_j, target_k))
    for tail in range(min(3, max_sample_size)):
        points = [(i, j, k, row[-1 - tail]) for i, j, k, row in rows if tail < len(row)]
        if len(points) >= 8 and tail < target_size:
            result[-1 - tail] = max(0, _predict_trilinear(points, target_i, target_j, target_k))
    return result


def predict_volatile_3d_rectangular(raw_node: dict, stable_rds: Set[int],
                                    target_i: int, target_j: int,
                                    target_k: int, run_sim: RunSim) -> ReuseProfile | None:
    sample_ns = [3, 4]
    sampled = _sample_volatile_groups_3d(raw_node, sample_ns, stable_rds, run_sim)
    target_min = min(target_i, target_j, target_k)
    if target_min > 0 and max(target_i, target_j, target_k) / target_min > 4:
        variable = predict_volatile_3d_variable(
            raw_node,
            stable_rds,
            target_i,
            target_j,
            target_k,
            run_sim,
        )
        if variable is not None:
            return variable

    group_count = len(sampled[0][3])
    if group_count == 0 or any(len(groups) != group_count for _, _, _, groups in sampled):
        return predict_volatile_3d_variable(
            raw_node,
            stable_rds,
            target_i,
            target_j,
            target_k,
            run_sim,
        )

    hist_by_bound = _sample_hist_3d(raw_node, sample_ns, run_sim)
    profile = ReuseProfile()
    for index in range(group_count):
        starts = [(i, j, k, groups[index][0]) for i, j, k, groups in sampled]
        sizes = [(i, j, k, len(groups[index])) for i, j, k, groups in sampled]
        start = _predict_trilinear(starts, target_i, target_j, target_k)
        size = _predict_trilinear(sizes, target_i, target_j, target_k)
        if size <= 0:
            return None

        rows = []
        for i, j, k, groups in sampled:
            rows.append((i, j, k, [hist_by_bound[(i, j, k)][rd] for rd in groups[index]]))
        freqs = _predict_group_freq_3d(rows, target_i, target_j, target_k, size)
        for offset, freq in enumerate(freqs):
            profile.histogram[start + offset] = freq
    return profile
