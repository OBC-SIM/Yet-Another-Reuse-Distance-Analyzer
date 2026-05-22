from typing import List, Set, Tuple

from lru_sim import ReuseProfile
from volatile import RunSim, _consecutive_groups, _group_mode, _predict_bilinear


def _sample_groups(raw_node: dict, sample_js: List[int], sample_ks: List[int],
                   stable_rds: Set[int], run_sim: RunSim):
    sampled = []
    for j in sample_js:
        for k in sample_ks:
            profile, _ = run_sim(raw_node, [j, k])
            volatile = [rd for rd in profile.histogram if rd not in stable_rds]
            sampled.append((j, k, _consecutive_groups(volatile)))
    return sampled


def _sample_hist(raw_node: dict, sample_js: List[int], sample_ks: List[int], run_sim: RunSim):
    return {
        (j, k): run_sim(raw_node, [j, k])[0].histogram
        for j in sample_js for k in sample_ks
    }


def _predict_group_freq(rows: List[Tuple[int, int, List[int]]],
                        target_j: int, target_k: int, target_size: int) -> List[int]:
    plateaus = [(j, k, max(row)) for j, k, row in rows]
    fill = max(0, _predict_bilinear(plateaus, target_j, target_k))
    result = [fill] * target_size
    max_sample_size = max(len(row) for _, _, row in rows)
    for offset in range(min(2, max_sample_size)):
        points = [(j, k, row[offset]) for j, k, row in rows if offset < len(row)]
        if len(points) >= 4 and offset < target_size:
            result[offset] = max(0, _predict_bilinear(points, target_j, target_k))
    for tail in range(min(3, max_sample_size)):
        points = [(j, k, row[-1 - tail]) for j, k, row in rows if tail < len(row)]
        if len(points) >= 4 and tail < target_size:
            result[-1 - tail] = max(0, _predict_bilinear(points, target_j, target_k))
    return result


def predict_volatile_2d_rectangular(raw_node: dict, stable_rds: Set[int],
                                    target_j: int, target_k: int,
                                    run_sim: RunSim) -> ReuseProfile | None:
    sample_js = [3, 4, 5]
    sample_ks = [3, 4, 5]
    sampled = _sample_groups(raw_node, sample_js, sample_ks, stable_rds, run_sim)
    group_count = len(sampled[0][2])
    if group_count == 0 or any(len(groups) != group_count for _, _, groups in sampled):
        return None

    hist_by_bound = _sample_hist(raw_node, sample_js, sample_ks, run_sim)
    profile = ReuseProfile()
    for index in range(group_count):
        starts = [(j, k, groups[index][0]) for j, k, groups in sampled]
        sizes = [(j, k, len(groups[index])) for j, k, groups in sampled]
        start = _predict_bilinear(starts, target_j, target_k)
        size = _predict_bilinear(sizes, target_j, target_k)
        if size <= 0:
            return None

        rows = [
            (j, k, [hist_by_bound[(j, k)][rd] for rd in groups[index]])
            for j, k, groups in sampled
        ]
        for offset, freq in enumerate(_predict_group_freq(rows, target_j, target_k, size)):
            profile.histogram[start + offset] = freq
    return profile
