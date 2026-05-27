from ca_metrics import calculate_ca_metrics, format_ca_metrics
from lru_sim import ReuseProfile


def test_calculate_ca_metrics_excludes_cold_misses_from_score():
    profile = ReuseProfile()
    profile.histogram = {0: 2, 3: 1}
    profile.cold_misses = {"A", "B"}

    metrics = calculate_ca_metrics(profile)

    assert metrics.reuse_count == 3
    assert metrics.cold_misses == 2
    assert metrics.mean_rd == 1.0
    assert metrics.ca_score == 1 / 2


def test_format_ca_metrics_handles_no_reuse():
    profile = ReuseProfile()
    profile.cold_misses = {"A"}

    assert format_ca_metrics(profile) == (
        "  mean_rd: N/A  ca_score: N/A  reuses=0 cold_misses=1"
    )
