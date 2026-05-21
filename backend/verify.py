"""
verify.py — ground-truth vs. Dilation-prediction comparison.

인자 없이 실행: 내장 테스트 케이스 5개 검증
파일 지정:      .c → clang-14 → .ll → opt-14 → _lat.json → 루프 블록별 검증

Usage:
    python backend/verify.py
    python backend/verify.py [--plugin PATH] FILE [FILE ...]

    FILE: .c 또는 .ll 파일
"""
import argparse
import json
import subprocess
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))

from gt_cache import ground_truth_cached
from lru_sim import LRUProfiler
from main import _DEFAULT_PLUGIN, _to_ll, run_llvm_pass
from predictor import _predict_loop_block
from report import print_comparison, timed

# ── 내장 테스트 케이스 ────────────────────────────────────────

_BUILTIN_CASES = [
    ("2D loop  j=8, k=8  (A[j][k], B[k][j])", {
        "type": "Loop", "var": "j", "bound": 8, "depth": 1, "body": [
            {"type": "Loop", "var": "k", "bound": 8, "depth": 2, "body": [
                {"type": "Array", "name": "A", "indices": ["j", "k"]},
                {"type": "Array", "name": "B", "indices": ["k", "j"]},
            ]}
        ]
    }),
    ("matmul   i=3, j=3, k=3", {
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
    }),
    ("matmul   i=4, j=4, k=4", {
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
    }),
    ("matmul   i=8, j=8, k=8", {
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
    }),
    ("ATAX     i=100, j=100, k=100  (A[i][j], x[j], y[i])", {
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
    }),
]

# ── 공통 검증 로직 ────────────────────────────────────────────

def _verify_node(name: str, raw: dict) -> None:
    (gt, gt_cached, unroll_time), gt_time = timed(lambda: ground_truth_cached(raw))
    (pred, _), pred_time = timed(lambda: _predict_loop_block(raw))
    print_comparison(name, gt, pred, gt_time, pred_time, gt_cached, unroll_time)


def _verify_flat(func_name: str, body: list) -> None:
    """루프 없는 flat 접근 시퀀스를 LRU로 직접 검증."""
    trace = []
    for node in body:
        if node["type"] == "Array":
            trace.append(node["name"] + "-" + "-".join(node["indices"]))
        elif node["type"] == "Scalar":
            trace.append(node["name"])
    if not trace:
        return
    profile = LRUProfiler.calculate(trace)
    name = f"{func_name}  (flat, {len(trace)} accesses)"
    print_comparison(name, profile, profile, 0.0, 0.0, False, 0.0)


def verify_json(json_path: Path) -> None:
    """_lat.json 내 모든 블록을 ground truth와 비교."""
    with open(json_path) as f:
        raw = json.load(f)
    for func_entry in raw:
        loops = [n for n in func_entry["body"] if n["type"] == "Loop"]
        non_loops = [n for n in func_entry["body"] if n["type"] != "Loop"]

        for node in loops:
            name = (
                f"{func_entry['function']}  "
                f"{node['var']}-loop (bound={node['bound']})"
            )
            _verify_node(name, node)

        if not loops and non_loops:
            _verify_flat(func_entry["function"], non_loops)


# ── 진입점 ───────────────────────────────────────────────────

def main() -> None:
    parser = argparse.ArgumentParser(
        description="ground-truth vs. Dilation-prediction 비교"
    )
    parser.add_argument(
        "files", nargs="*", metavar="FILE",
        help=".c 또는 .ll 파일. 미지정 시 내장 테스트 케이스 실행",
    )
    parser.add_argument(
        "--plugin",
        default=str(_DEFAULT_PLUGIN),
        metavar="PATH",
        help=f"플러그인 .so 경로 (기본값: {_DEFAULT_PLUGIN})",
    )
    args = parser.parse_args()

    if not args.files:
        for name, raw in _BUILTIN_CASES:
            _verify_node(name, raw)
        return

    plugin = Path(args.plugin)
    if not plugin.exists():
        print(f"오류: 플러그인을 찾을 수 없습니다: {plugin}", file=sys.stderr)
        sys.exit(1)

    for file_str in args.files:
        path = Path(file_str)
        if not path.exists():
            print(f"오류: 파일 없음: {path}", file=sys.stderr)
            continue
        if path.suffix not in (".c", ".ll"):
            print(f"오류: .c 또는 .ll 파일만 지원합니다: {path}", file=sys.stderr)
            continue
        try:
            print(f"\n[pipeline] {path.name} ...", flush=True)
            ll_path = _to_ll(path)
            lat_json = run_llvm_pass(ll_path, plugin)
            print(f"  opt-14 완료 → {lat_json.name}")
            verify_json(lat_json)
        except subprocess.CalledProcessError as e:
            stderr = e.stderr.decode(errors="replace") if e.stderr else ""
            print(f"\n오류: {e.args[0][0]} 실패\n{stderr}", file=sys.stderr)
        except Exception as e:
            print(f"\n오류: {e}", file=sys.stderr)


if __name__ == "__main__":
    main()