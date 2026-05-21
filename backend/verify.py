"""
verify.py — ground-truth vs. Dilation-prediction comparison.

Ground truth : actual_bound으로 완전 언롤 → 정수 메모리 주소 기반 LRUProfiler
Prediction   : Dilation Equation (_predict_loop_block)
"""
import sys
import os
sys.path.insert(0, os.path.dirname(__file__))

from gt_cache import ground_truth_cached
from lru_sim import ReuseProfile
from main import _predict_loop_block
from report import print_comparison, timed

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
        (gt, gt_cached, unroll_time), gt_time = timed(lambda: ground_truth_cached(raw))
        (pred, _), pred_time = timed(lambda: _predict_loop_block(raw))
        print_comparison(name, gt, pred, gt_time, pred_time, gt_cached, unroll_time)
