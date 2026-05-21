from collections import Counter
from fractions import Fraction
from typing import Callable, List, Set, Tuple

from lru_sim import ReuseProfile

# run_sim 시그니처: (raw_node, sim_bounds) -> (ReuseProfile, trace)
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
) -> List[Tuple[int, List[List[int]]]]:
    sampled = []
    for n in sample_ns:
        profile, _ = run_sim(raw_node, [n, n, n])
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


def predict_volatile_diagonal(
    raw_node: dict,
    stable_rds: Set[int],
    target_n: int,
    run_sim: RunSim,
) -> ReuseProfile | None:
    sample_ns = [3, 4, 5, 6, 7]
    sampled = _sample_volatile_groups(raw_node, sample_ns, stable_rds, run_sim)
    group_count = len(sampled[0][1])
    if group_count == 0 or any(len(groups) != group_count for _, groups in sampled):
        return None
    profile = ReuseProfile()
    for index in range(group_count):
        if target_n in sample_ns:
            groups = dict(sampled)[target_n]
            hist = run_sim(raw_node, [target_n, target_n, target_n])[0].histogram
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
            hist = run_sim(raw_node, [n, n, n])[0].histogram
            rows.append((n, [hist[rd] for rd in groups[index]]))
        freqs = _predict_group_freq(rows, target_n, size)
        for offset, freq in enumerate(freqs):
            profile.histogram[start + offset] = freq
    return profile
