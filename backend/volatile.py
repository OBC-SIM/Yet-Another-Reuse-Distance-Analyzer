from collections import Counter
from fractions import Fraction
from typing import Callable, List, Set, Tuple

from lru_sim import ReuseProfile

RunSim = Callable[[dict, List[int]], Tuple[ReuseProfile, List[str]]]


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
    stable_rds: Set[int],
    run_sim: RunSim,
    depth: int,
) -> List[Tuple[int, List[List[int]]]]:
    sampled = []
    for n in sample_ns:
        profile, _ = run_sim(raw_node, [n] * depth)
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


def _predict_bilinear(points: List[Tuple[int, int, int]], target_j: int, target_k: int) -> int:
    values = {(j, k): value for j, k, value in points}
    j0 = min(j for j, _, _ in points)
    k0 = min(k for _, k, _ in points)
    j1 = min(j for j, _, _ in points if j > j0)
    k1 = min(k for _, k, _ in points if k > k0)
    b = values[(j0, k0)]
    incr_j = values[(j1, k0)] - b
    incr_k = values[(j0, k1)] - b
    coff_jk = values[(j1, k1)] - b - incr_j - incr_k
    dj = target_j - j0
    dk = target_k - k0
    return b + dj * incr_j + dk * incr_k + dj * dk * coff_jk


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
    coff_ijk = values[(i1, j1, k1)] - b - incr_i - incr_j - incr_k - coff_ij - coff_ik - coff_jk
    return (b + di * incr_i + dj * incr_j + dk * incr_k
            + di * dj * coff_ij + di * dk * coff_ik + dj * dk * coff_jk
            + di * dj * dk * coff_ijk)


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
    group_count = len(sampled[0][3])
    if group_count == 0 or any(len(groups) != group_count for _, _, _, groups in sampled):
        return None

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


def predict_volatile_diagonal(
    raw_node: dict,
    stable_rds: Set[int],
    target_n: int,
    run_sim: RunSim,
    depth: int = 3,
) -> ReuseProfile | None:
    sample_ns = [3, 4, 5, 6, 7]
    sampled = _sample_volatile_groups(raw_node, sample_ns, stable_rds, run_sim, depth)
    group_count = len(sampled[0][1])
    if group_count == 0 or any(len(groups) != group_count for _, groups in sampled):
        return None
    profile = ReuseProfile()
    for index in range(group_count):
        if target_n in sample_ns:
            groups = dict(sampled)[target_n]
            hist = run_sim(raw_node, [target_n] * depth)[0].histogram
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
            hist = run_sim(raw_node, [n] * depth)[0].histogram
            rows.append((n, [hist[rd] for rd in groups[index]]))
        freqs = _predict_group_freq(rows, target_n, size)
        for offset, freq in enumerate(freqs):
            profile.histogram[start + offset] = freq
    return profile
