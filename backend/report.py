from time import perf_counter
from typing import Callable, Tuple, TypeVar

from lru_sim import ReuseProfile

T = TypeVar("T")


def timed(fn: Callable[[], T]) -> Tuple[T, float]:
    """fn()을 실행하고 (결과, 경과_초)를 반환."""
    start = perf_counter()
    result = fn()
    return result, perf_counter() - start


def fmt_seconds(seconds: float) -> str:
    if seconds < 0.001:
        return f"{seconds * 1_000_000:.1f}us"
    if seconds < 1:
        return f"{seconds * 1000:.2f}ms"
    return f"{seconds:.3f}s"


def print_timing(
    gt_time: float,
    pred_time: float,
    gt_cached: bool,
    unroll_time: float,
) -> None:
    total = gt_time + pred_time
    speedup = unroll_time / pred_time if pred_time > 0 else float("inf")
    print("\n  timing")
    if gt_cached:
        print(
            "    gt_source=cache"
            f"  cache_load={fmt_seconds(gt_time)}"
            f"  gt_unroll_baseline={fmt_seconds(unroll_time)}"
        )
    else:
        print(f"    gt_source=unroll  gt_unroll={fmt_seconds(gt_time)}")
    print(
        f"    pred={fmt_seconds(pred_time)}"
        f"  verify_total={fmt_seconds(total)}"
        f"  pred_vs_unroll={speedup:.1f}x"
    )


def print_comparison(
    name: str,
    gt: ReuseProfile,
    pred: ReuseProfile,
    gt_time: float,
    pred_time: float,
    gt_cached: bool,
    unroll_time: float,
) -> None:
    all_rds = sorted(set(gt.histogram) | set(pred.histogram))

    print(f"\n{'='*62}")
    print(f"  {name}")
    print(f"{'='*62}")
    print(f"  {'RD':>6}  {'ground truth':>14}  {'predicted':>12}  {'diff':>8}")
    print(f"  {'-'*6}  {'-'*14}  {'-'*12}  {'-'*8}")

    for rd in all_rds:
        g = gt.histogram.get(rd, 0)
        p = pred.histogram.get(rd, 0)
        flag = "" if g == p else "  ✗"
        print(f"  {rd:>6}  {g:>14}  {p:>12}  {p-g:>+8}{flag}")

    gt_total   = sum(gt.histogram.values())
    pred_total = sum(pred.histogram.values())
    print(f"  {'total':>6}  {gt_total:>14}  {pred_total:>12}  {pred_total-gt_total:>+8}")
    print(f"\n  cold misses  gt={len(gt.cold_misses)}  pred={len(pred.cold_misses)}")
    print_timing(gt_time, pred_time, gt_cached, unroll_time)
    print(f"  {'MATCH ✓' if gt.histogram == pred.histogram else 'MISMATCH ✗'}")
