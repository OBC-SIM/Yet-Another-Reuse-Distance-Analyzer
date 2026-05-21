"""
verify.py — ground-truth vs. Dilation-prediction comparison.

Ground truth : actual_bound으로 완전 언롤 → 정수 메모리 주소 기반 LRUProfiler
Prediction   : Dilation Equation (_predict_loop_block)
"""
import sys
import os
sys.path.insert(0, os.path.dirname(__file__))

from time import perf_counter

from gt_cache import ground_truth_cached
from lru_sim import ReuseProfile
from main import _predict_loop_block

# ── 테스트 케이스 ─────────────────────────────────────────────

LOOP_2D = {
    "type": "Loop", "var": "j", "bound": 8, "depth": 1, "body": [
        {"type": "Loop", "var": "k", "bound": 8, "depth": 2, "body": [
            {"type": "Array", "name": "A", "indices": ["j", "k"]},
            {"type": "Array", "name": "B", "indices": ["k", "j"]},
        ]}
    ]
}

MATMUL_3 = {
    "type": "Loop", "var": "i", "bound": 3, "depth": 1, "body": [
        {"type": "Loop", "var": "j", "bound": 3, "depth": 2, "body": [
            {"type": "Loop", "var": "k", "bound": 3, "depth": 3, "body": [
                {"type": "Array", "name": "A", "indices": ["i", "k"]},
                {"type": "Array", "name": "B", "indices": ["k", "j"]},
                {"type": "Array", "name": "C", "indices": ["i", "j"]},
                {"type": "Array", "name": "C", "indices": ["i", "j"]},
            ]}
        ]}
    ]
}

MATMUL_4 = {
    "type": "Loop", "var": "i", "bound": 4, "depth": 1, "body": [
        {"type": "Loop", "var": "j", "bound": 4, "depth": 2, "body": [
            {"type": "Loop", "var": "k", "bound": 4, "depth": 3, "body": [
                {"type": "Array", "name": "A", "indices": ["i", "k"]},
                {"type": "Array", "name": "B", "indices": ["k", "j"]},
                {"type": "Array", "name": "C", "indices": ["i", "j"]},
                {"type": "Array", "name": "C", "indices": ["i", "j"]},
            ]}
        ]}
    ]
}

MATMUL_8 = {
    "type": "Loop", "var": "i", "bound": 8, "depth": 1, "body": [
        {"type": "Loop", "var": "j", "bound": 8, "depth": 2, "body": [
            {"type": "Loop", "var": "k", "bound": 8, "depth": 3, "body": [
                {"type": "Array", "name": "A", "indices": ["i", "k"]},
                {"type": "Array", "name": "B", "indices": ["k", "j"]},
                {"type": "Array", "name": "C", "indices": ["i", "j"]},
                {"type": "Array", "name": "C", "indices": ["i", "j"]},
            ]}
        ]}
    ]
}

ATAX = {
    "type": "Loop", "var": "i", "bound": 100, "depth": 1, "body": [
        {"type": "Loop", "var": "j", "bound": 100, "depth": 2, "body": [
            {"type": "Loop", "var": "k", "bound": 100, "depth": 3, "body": [
                {"type": "Array", "name": "A", "indices": ["i", "j"]},
                {"type": "Array", "name": "x", "indices": ["j"]},
                {"type": "Array", "name": "y", "indices": ["i"]},
                {"type": "Array", "name": "y", "indices": ["i"]},
            ]}
        ]}
     ]
}

# ── 출력 ──────────────────────────────────────────────────────

def _timed(callable_):
    start = perf_counter()
    result = callable_()
    return result, perf_counter() - start


def _fmt_seconds(seconds: float) -> str:
    if seconds < 0.001:
        return f"{seconds * 1_000_000:.1f}us"
    if seconds < 1:
        return f"{seconds * 1000:.2f}ms"
    return f"{seconds:.3f}s"


def _print_timing(
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
            f"  cache_load={_fmt_seconds(gt_time)}"
            f"  gt_unroll_baseline={_fmt_seconds(unroll_time)}"
        )
    else:
        print(
            "    gt_source=unroll"
            f"  gt_unroll={_fmt_seconds(gt_time)}"
        )
    print(
        f"    pred={_fmt_seconds(pred_time)}"
        f"  verify_total={_fmt_seconds(total)}"
        f"  pred_vs_unroll={speedup:.1f}x"
    )


def _print_comparison(
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

    gt_total  = sum(gt.histogram.values())
    pred_total = sum(pred.histogram.values())
    print(f"  {'total':>6}  {gt_total:>14}  {pred_total:>12}  {pred_total-gt_total:>+8}")
    print(f"\n  cold misses  gt={len(gt.cold_misses)}  pred={len(pred.cold_misses)}")
    _print_timing(gt_time, pred_time, gt_cached, unroll_time)
    print(f"  {'MATCH ✓' if gt.histogram == pred.histogram else 'MISMATCH ✗'}")

# ── 실행 ──────────────────────────────────────────────────────

if __name__ == "__main__":
    cases = [
        ("2D loop  j=8, k=8  (A[j][k], B[k][j])", LOOP_2D),
        ("matmul   i=3, j=3, k=3",                 MATMUL_3),
        ("matmul   i=4, j=4, k=4",                 MATMUL_4),
        ("matmul   i=8, j=8, k=8",                 MATMUL_8),
        ("ATAX     i=100, j=100, k=100  (A[i][j], x[j], y[i])", ATAX),
    ]
    for name, raw in cases:
        (gt, gt_cached, unroll_time), gt_time = _timed(lambda: ground_truth_cached(raw))
        (pred, _), pred_time = _timed(lambda: _predict_loop_block(raw))
        _print_comparison(name, gt, pred, gt_time, pred_time, gt_cached, unroll_time)
