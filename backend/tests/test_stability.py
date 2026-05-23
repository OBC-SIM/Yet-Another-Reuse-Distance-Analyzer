from stability import validated_stable_rds_3d


def _stable_freq(i: int, j: int, k: int) -> int:
    return 5 + 2 * (i - 2) + 3 * (j - 2) + 4 * (k - 2)


def test_validated_stable_rds_3d_keeps_holdout_match_only():
    hist = {}
    for i in (2, 3):
        for j in (2, 3):
            for k in (2, 3):
                hist[(i, j, k)] = {
                    7: _stable_freq(i, j, k),
                    11: i + j + k,
                }

    hist[(4, 4, 4)] = {
        7: _stable_freq(4, 4, 4),
        11: 999,
    }

    assert validated_stable_rds_3d(hist) == {7}
