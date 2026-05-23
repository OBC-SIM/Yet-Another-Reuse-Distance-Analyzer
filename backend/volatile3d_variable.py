from typing import List, Set, Tuple

from lru_sim import ReuseProfile
from series import predict_tail_series
from volatile import RunSim, _consecutive_groups

MIN_AXIS_SAMPLE = 5


def _ray_sample_bounds(target_i: int, target_j: int, target_k: int) -> List[Tuple[int, int, int]]:
    target_min = min(target_i, target_j, target_k)
    target_max = max(target_i, target_j, target_k)
    sample_caps = tuple(min(dim, max(dim // 2, MIN_AXIS_SAMPLE)) for dim in (target_i, target_j, target_k))
    if min(sample_caps) < 2:
        return []
    max_sample_volume = target_i * target_j * target_k // 2
    dims = (target_i, target_j, target_k)
    if target_max / target_min > 4:
        variable = [idx for idx, dim in enumerate(dims) if dim > target_min]
        max_n = min(sample_caps[idx] for idx in variable)
        return [
            tuple(n if idx in variable else sample_caps[idx] for idx in range(3))
            for n in range(max(2, max_n - 2), max_n + 1)
        ]
    offsets = (target_i - target_min, target_j - target_min, target_k - target_min)
    samples = []
    for n in range(2, min(14, target_min - 1) + 1):
        if max(offsets) <= 8:
            bounds = tuple(n + offset for offset in offsets)
        else:
            bounds = (
                max(2, round(n * target_i / target_min)),
                max(2, round(n * target_j / target_min)),
                max(2, round(n * target_k / target_min)),
            )
        if any(bound > cap for bound, cap in zip(bounds, sample_caps)):
            continue
        if bounds not in samples and bounds[0] * bounds[1] * bounds[2] <= max_sample_volume:
            samples.append(bounds)
    return samples


def _shape_key(i: int, j: int, k: int, use_max: bool) -> int:
    return max(i, j, k) if use_max else min(i, j, k)


def _sample_groups(raw_node: dict, sample_bounds: List[Tuple[int, int, int]],
                   stable_rds: Set[int], run_sim: RunSim):
    sampled = []
    hist_by_bound = {}
    for bounds in sample_bounds:
        profile, _ = run_sim(raw_node, list(bounds))
        hist_by_bound[bounds] = profile.histogram
        volatile = [rd for rd in profile.histogram if rd not in stable_rds]
        sampled.append((*bounds, _consecutive_groups(volatile)))
    return sampled, hist_by_bound


def _leading_run(row: List[int]) -> int:
    if not row:
        return 0
    size = 1
    while size < len(row) and row[size] == row[0]:
        size += 1
    return size if size < len(row) else 0


def _row_value_at_rank(row: List[int], rank: float, prefix_size: int) -> int:
    if len(row) == 1:
        return row[0]
    if prefix_size and rank < 0:
        index = min(prefix_size - 1, len(row) - 1)
    elif prefix_size and prefix_size < len(row):
        index = prefix_size + int(rank * (len(row) - 1 - prefix_size))
    else:
        index = int(rank * (len(row) - 1))
    return row[max(0, min(index, len(row) - 1))]


def _predict_freqs(rows: List[Tuple[int, List[int]]], target_n: int,
                   target_size: int) -> List[int]:
    for n, row in rows:
        if n == target_n and len(row) == target_size:
            return row[:]
    result = []
    prefix_size = min((_leading_run(row) for _, row in rows if row), default=0)
    suffix_size = min(4, min((len(row) for _, row in rows if row), default=0))
    common_mid = min((len(row) - suffix_size for _, row in rows if row), default=0)
    period2 = common_mid > 4 and all(
        row[pos] == row[pos + 2]
        for _, row in rows for pos in range(2, max(2, min(len(row) - suffix_size - 2, common_mid - 2)))
    )
    for offset in range(target_size):
        if suffix_size and offset >= target_size - suffix_size:
            tail_offset = target_size - offset
            points = [(n, row[-tail_offset]) for n, row in rows if tail_offset <= len(row)]
        elif prefix_size and offset < prefix_size:
            points = [(n, row[offset]) for n, row in rows if offset < len(row)]
        elif period2 and offset >= common_mid:
            ref = 2 + (offset - 2) % 2
            points = [(n, row[ref]) for n, row in rows if ref < len(row)]
        elif all(offset < len(row) - suffix_size for _, row in rows[-3:]):
            points = [(n, row[offset]) for n, row in rows if offset < len(row)]
        elif offset >= 2 and all(offset - 2 < len(row) - suffix_size for _, row in rows[-3:]):
            points = [(n, row[offset - 2]) for n, row in rows if offset - 2 < len(row)]
        else:
            rank_size = target_size - prefix_size
            rank = 0.0 if rank_size <= 1 else (offset - prefix_size) / (rank_size - 1)
            points = [
                (n, _row_value_at_rank(row, rank, prefix_size))
                for n, row in rows
                if row
            ]
        result.append(max(0, predict_tail_series(points, target_n)))
    return result


def _split_groups(groups: List[List[int]]):
    if not groups:
        return [], [], [], []
    index = 1
    body = []
    while index < len(groups) and len(groups[index]) > 2:
        body.append(groups[index])
        index += 1
    pivot = []
    if index < len(groups) and len(groups[index]) == 1:
        pivot = [groups[index]]
        index += 1
    return groups[:1], body, pivot, groups[index:]


def _predict_group(rows, hist_by_bound, target_n: int):
    starts = [(n, group[0]) for n, _, group in rows]
    sizes = [(n, len(group)) for n, _, group in rows]
    start = predict_tail_series(starts, target_n)
    size = max(0, predict_tail_series(sizes, target_n))
    freq_rows = [
        (n, [hist_by_bound[bounds][rd] for rd in group])
        for n, bounds, group in rows
    ]
    return start, _predict_freqs(freq_rows, target_n, size)


def _emit(profile: ReuseProfile, start: int, freqs: List[int]) -> None:
    for offset, freq in enumerate(freqs):
        if freq > 0:
            profile.histogram[start + offset] = freq


def _sparse_freqs(size: int) -> List[int]:
    if size <= 0:
        return []
    if size == 1:
        return [1]
    return [1] + [2] * (size - 2) + [1]


def predict_volatile_3d_variable(
    raw_node: dict,
    stable_rds: Set[int],
    target_i: int,
    target_j: int,
    target_k: int,
    run_sim: RunSim,
) -> ReuseProfile | None:
    sample_bounds = _ray_sample_bounds(target_i, target_j, target_k)
    if len(sample_bounds) < 3:
        return None

    sampled, hist_by_bound = _sample_groups(raw_node, sample_bounds, stable_rds, run_sim)
    target_skew = max(target_i, target_j, target_k) / min(target_i, target_j, target_k) > 4
    target_n = _shape_key(target_i, target_j, target_k, target_skew)
    parts = [(*bounds, _split_groups(groups)) for *bounds, groups in sampled]
    body_counts = [(_shape_key(i, j, k, target_skew), len(part[1])) for i, j, k, part in parts]
    target_body_count = max(0, predict_tail_series(body_counts, target_n))

    profile = ReuseProfile()
    prefix_rows = [
        (_shape_key(i, j, k, target_skew), (i, j, k), part[0][0])
        for i, j, k, part in parts
        if part[0]
    ]
    _emit(profile, *_predict_group(prefix_rows, hist_by_bound, target_n))

    last_body_start = 0
    last_body_size = 0
    emitted_bodies = {}
    for index in range(target_body_count):
        body_rows = [
            (_shape_key(i, j, k, target_skew), (i, j, k), part[1][index])
            for i, j, k, part in parts
            if index < len(part[1])
        ]
        if len(body_rows) >= 4:
            start, freqs = _predict_group(body_rows, hist_by_bound, target_n)
            last_body_start, last_body_size = start, len(freqs)
            emitted_bodies[index] = (start, len(freqs))
        elif len(body_rows) >= 2:
            known = [(idx, body) for idx, body in emitted_bodies.items() if idx > 0]
            if known:
                starts = [(idx, body[0]) for idx, body in known]
                sizes = [(idx, body[1]) for idx, body in known]
                start = predict_tail_series(starts, index)
                last_body_size = max(0, predict_tail_series(sizes, index))
            else:
                starts = [(n, group[0]) for n, _, group in body_rows]
                sizes = [(n, len(group)) for n, _, group in body_rows]
                start = predict_tail_series(starts, target_n)
                last_body_size = max(0, predict_tail_series(sizes, target_n))
            freqs = _sparse_freqs(last_body_size)
            last_body_start = start
            emitted_bodies[index] = (start, last_body_size)
        else:
            known = [(idx, body) for idx, body in emitted_bodies.items() if idx > 0]
            if known:
                starts = [(idx, body[0]) for idx, body in known]
                sizes = [(idx, body[1]) for idx, body in known]
                start = predict_tail_series(starts, index)
                last_body_size = max(0, predict_tail_series(sizes, index))
            else:
                start = last_body_start + last_body_size + 1
            freqs = _sparse_freqs(last_body_size)
            last_body_start = start
            emitted_bodies[index] = (start, last_body_size)
        _emit(profile, start, freqs)

    last_body_rows = [
        (_shape_key(i, j, k, target_skew), (i, j, k), part[1][-1])
        for i, j, k, part in parts
        if part[1]
    ]
    if len(last_body_rows) >= 4:
        _emit(profile, *_predict_group(last_body_rows, hist_by_bound, target_n))

    pivot_rows = [
        (_shape_key(i, j, k, target_skew), (i, j, k), part[2][0])
        for i, j, k, part in parts
        if part[2]
    ]
    if len(pivot_rows) >= 2:
        _emit(profile, *_predict_group(pivot_rows, hist_by_bound, target_n))

    tail_counts = [(_shape_key(i, j, k, target_skew), len(part[3])) for i, j, k, part in parts]
    target_tail_count = max(0, predict_tail_series(tail_counts, target_n))
    emitted_tails = {}
    for tail in range(target_tail_count):
        tail_rows = [
            (_shape_key(i, j, k, target_skew), (i, j, k), part[3][-1 - tail])
            for i, j, k, part in parts
            if tail < len(part[3])
        ]
        if len(tail_rows) >= 3:
            start, freqs = _predict_group(tail_rows, hist_by_bound, target_n)
            emitted_tails[tail] = (start, freqs)
            _emit(profile, start, freqs)

    for tail in range(target_tail_count):
        if tail in emitted_tails or not emitted_tails:
            continue
        known = sorted(emitted_tails)
        starts = [(index, emitted_tails[index][0]) for index in known]
        sizes = [(index, len(emitted_tails[index][1])) for index in known]
        freqs_by_offset = []
        size = max(0, predict_tail_series(sizes, tail))
        for offset in range(size):
            points = [
                (index, freqs[offset])
                for index, (_, freqs) in emitted_tails.items()
                if offset < len(freqs)
            ]
            freqs_by_offset.append(max(0, predict_tail_series(points, tail)))
        start = predict_tail_series(starts, tail)
        _emit(profile, start, freqs_by_offset)

    return profile if profile.histogram else None
