"""
verify.py — ground-truth vs. Dilation-prediction comparison.

Ground truth : actual_bound으로 완전 언롤 → LRUProfiler
Prediction   : Dilation Equation (_predict_loop_block)
"""
import sys
import os
sys.path.insert(0, os.path.dirname(__file__))

from parser import parse_trace, LoopBlockNode
from lru_sim import LRUProfiler, ReuseProfile
from main import _predict_loop_block, _apply_sim_bounds

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

# ── Ground Truth ──────────────────────────────────────────────

def _ground_truth(raw_node: dict) -> ReuseProfile:
    nodes = parse_trace([raw_node], sim_bound=2)
    loop = nodes[0]
    # 실제 바운드를 깊이 순서대로 수집
    actual_bounds = []
    n = loop
    while isinstance(n, LoopBlockNode):
        actual_bounds.append(n.actual_bound)
        n = next((c for c in n.body if isinstance(c, LoopBlockNode)), None)
    _apply_sim_bounds(loop, actual_bounds)
    return LRUProfiler.calculate(loop.unroll({}))

# ── 출력 ──────────────────────────────────────────────────────

def _print_comparison(name: str, gt: ReuseProfile, pred: ReuseProfile) -> None:
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
    print(f"  {'MATCH ✓' if gt.histogram == pred.histogram else 'MISMATCH ✗'}")

# ── 실행 ──────────────────────────────────────────────────────

if __name__ == "__main__":
    cases = [
        ("2D loop  j=8, k=8  (A[j][k], B[k][j])", LOOP_2D),
        ("matmul   i=3, j=3, k=3",                 MATMUL_3),
        ("matmul   i=4, j=4, k=4",                 MATMUL_4),
        ("matmul   i=8, j=8, k=8",                 MATMUL_8),
    ]
    for name, raw in cases:
        gt         = _ground_truth(raw)
        pred, _    = _predict_loop_block(raw)
        _print_comparison(name, gt, pred)