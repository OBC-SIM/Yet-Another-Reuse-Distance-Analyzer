from typing import Dict, Set, Tuple


def _predict_2d_freq(hist: Dict[Tuple[int, int], Dict[int, int]],
                     rd: int, target_j: int, target_k: int) -> int:
    b22 = hist[(2, 2)].get(rd, 0)
    incr_j = hist[(3, 2)].get(rd, 0) - b22
    incr_k = hist[(2, 3)].get(rd, 0) - b22
    coff_jk = hist[(3, 3)].get(rd, 0) - b22 - incr_j - incr_k
    return b22 + (target_j - 2) * incr_j + (target_k - 2) * incr_k + (
        target_j - 2
    ) * (target_k - 2) * coff_jk


def validated_stable_rds_2d(hist: Dict[Tuple[int, int], Dict[int, int]]) -> Set[int]:
    base_keys = ((2, 2), (3, 2), (2, 3), (3, 3))
    candidates = set.intersection(*(set(hist[key]) for key in base_keys))
    validated: Set[int] = set()
    for rd in candidates:
        expected = _predict_2d_freq(hist, rd, 4, 4)
        if hist[(4, 4)].get(rd, 0) == expected:
            validated.add(rd)
    return validated
