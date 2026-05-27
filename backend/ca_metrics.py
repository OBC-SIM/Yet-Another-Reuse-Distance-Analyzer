from dataclasses import dataclass

from lru_sim import ReuseProfile


@dataclass(frozen=True)
class CAMetrics:
    mean_rd: float | None
    ca_score: float | None
    reuse_count: int
    cold_misses: int


def calculate_ca_metrics(profile: ReuseProfile) -> CAMetrics:
    reuse_count = sum(profile.histogram.values())
    cold_count = len(profile.cold_misses)
    weighted_sum = sum(rd * count for rd, count in profile.histogram.items())
    if reuse_count == 0:
        return CAMetrics(None, None, reuse_count, cold_count)
    mean_rd = weighted_sum / reuse_count
    ca_score = reuse_count / (reuse_count + weighted_sum)
    return CAMetrics(mean_rd, ca_score, reuse_count, cold_count)


def format_ca_metrics(profile: ReuseProfile) -> str:
    metrics = calculate_ca_metrics(profile)
    if metrics.mean_rd is None or metrics.ca_score is None:
        return (
            "  mean_rd: N/A  ca_score: N/A  "
            f"reuses={metrics.reuse_count} cold_misses={metrics.cold_misses}"
        )
    return (
        f"  mean_rd: {metrics.mean_rd:.2f}  "
        f"ca_score: {metrics.ca_score:.6f}  "
        f"reuses={metrics.reuse_count} cold_misses={metrics.cold_misses}"
    )
