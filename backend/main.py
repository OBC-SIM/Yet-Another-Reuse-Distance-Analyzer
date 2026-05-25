"""
main.py — C/LLVM IR → 재사용 거리 히스토그램 출력.

Usage:
    python backend/main.py [--plugin PATH] [--mode predict|unroll]
                           [--plot] [--save PATH] FILE [FILE ...]

    FILE: .c 또는 .ll 파일. .c 는 clang-14 로 컴파일 후 파이프라인 실행.

Options:
    --plugin PATH          libLoopAnnotatedTrace.so 경로
    --mode predict|unroll  predict(기본): Dilation 예측 (빠름)
                           unroll: 실제 loop unroll + LRU 시뮬 (정확)
    --plot                 seaborn 히스토그램 저장
    --save PATH            플롯 저장 경로 (PNG/PDF/SVG 등)
"""

import argparse
import json
import subprocess
import sys
from pathlib import Path
from typing import List, Tuple

sys.path.insert(0, str(Path(__file__).parent))

from block_trace import block_trace_results
from lru_sim import LRUProfiler, ReuseProfile
from plot import aggregate_as_program, plot_histograms
from predictor import analyze, analyze_blocks

_REPO_ROOT = Path(__file__).parent.parent.resolve()
_DEFAULT_PLUGIN = _REPO_ROOT / "build" / "libLoopAnnotatedTrace.so"


def compile_c(c_path: Path) -> Path:
    """clang-14로 .c → _g.ll 컴파일 후 .ll 경로 반환."""
    abs_c = c_path.resolve()
    out_ll = abs_c.parent / (abs_c.stem + "_g.ll")
    subprocess.run(
        [
            "clang-14",
            "-O0",
            "-Xclang",
            "-disable-O0-optnone",
            "-g",
            "-emit-llvm",
            "-S",
            "-o",
            str(out_ll),
            str(abs_c),
        ],
        check=True,
        stderr=subprocess.PIPE,
        cwd=abs_c.parent,
    )
    return out_ll


def _to_ll(path: Path) -> Path:
    """입력이 .c면 컴파일, .ll이면 그대로 반환."""
    if path.suffix == ".c":
        print(f"  [0/2] clang-14 컴파일 중...", end=" ", flush=True)
        ll = compile_c(path)
        print(f"완료 → {ll.name}")
        return ll
    return path


def _lat_path(ll_path: Path) -> Path:
    """foo_g.ll → foo_g_lat.json"""
    return ll_path.with_suffix("").parent / (ll_path.stem + "_lat.json")


def run_llvm_pass(ll_path: Path, plugin_path: Path) -> Path:
    """opt-14 실행하여 _lat.json을 생성하고 그 경로를 반환."""
    abs_ll = ll_path.resolve()
    out_json = _lat_path(abs_ll)
    subprocess.run(
        [
            "opt-14",
            f"-load-pass-plugin={plugin_path.resolve()}",
            "-passes=function(mem2reg),loop-simplify,loop-annotated-trace",
            str(abs_ll),
            "-o", "/dev/null",
        ],
        check=True,
        stderr=subprocess.PIPE,
        cwd=abs_ll.parent,
    )
    if not out_json.exists():
        raise FileNotFoundError(f"opt-14 실행 후 {out_json} 가 생성되지 않았습니다.")
    return out_json


def _unroll_block_traces(lat_json: Path) -> List[Tuple[str, ReuseProfile, List[str]]]:
    """LAT JSON body 순서를 보존해 block별 actual trace를 계산."""
    with open(lat_json) as f:
        return block_trace_results(json.load(f))


def _unroll_blocks(lat_json: Path) -> List[Tuple[str, ReuseProfile]]:
    return [(name, profile) for name, profile, _ in _unroll_block_traces(lat_json)]


def _unroll_file(lat_json: Path) -> Tuple[ReuseProfile, List[Tuple[str, ReuseProfile]]]:
    block_traces = _unroll_block_traces(lat_json)
    full_trace: List[str] = []
    blocks: List[Tuple[str, ReuseProfile]] = []
    for name, profile, trace in block_traces:
        blocks.append((name, profile))
        full_trace.extend(trace)
    return LRUProfiler.calculate(full_trace), blocks


def _print_histogram(profile: ReuseProfile) -> None:
    hist = profile.histogram
    if not hist:
        print("  (재사용 없음 — 모든 접근이 cold miss)")
        return

    all_rds = sorted(hist)
    total = sum(hist.values())
    max_count = max(hist.values())
    bar_width = 30

    print(f"  {'RD':>6}  {'count':>10}  {'%':>6}  bar")
    print(f"  {'-'*6}  {'-'*10}  {'-'*6}  {'-'*bar_width}")
    for rd in all_rds:
        count = hist[rd]
        pct = count / total * 100
        bar_len = int(count / max_count * bar_width)
        bar = "█" * bar_len
        print(f"  {rd:>6}  {count:>10}  {pct:>5.1f}%  {bar}")
    print(f"  {'total':>6}  {total:>10}")
    print(f"  cold misses: {len(profile.cold_misses)}")



def analyze_file(
    path: Path, plugin_path: Path, mode: str = "predict"
) -> Tuple[ReuseProfile, List[Tuple[str, ReuseProfile]]]:
    """파이프라인 실행 후 (합산 프로파일, 블록별 프로파일 리스트) 반환."""
    print(f"\n{'='*62}")
    print(f"  {path.name}  [mode={mode}]")
    print(f"{'='*62}")

    ll_path = _to_ll(path)

    print("  [1/2] opt-14 실행 중...", end=" ", flush=True)
    lat_json = run_llvm_pass(ll_path, plugin_path)
    print(f"완료 → {lat_json.name}")

    action = "예측" if mode == "predict" else "unrolling"
    print(f"  [2/2] 재사용 거리 {action} 중...", end=" ", flush=True)
    if mode == "predict":
        profile = analyze(str(lat_json))
        blocks = analyze_blocks(str(lat_json))
    else:
        profile, blocks = _unroll_file(lat_json)
    print("완료")

    print()
    _print_histogram(profile)
    return profile, blocks


def main() -> None:
    parser = argparse.ArgumentParser(
        description="LLVM IR → 재사용 거리 히스토그램"
    )
    parser.add_argument("files", nargs="+", metavar="FILE", help=".c 또는 .ll 파일")
    parser.add_argument(
        "--plugin",
        default=str(_DEFAULT_PLUGIN),
        metavar="PATH",
        help=f"플러그인 .so 경로 (기본값: {_DEFAULT_PLUGIN})",
    )
    parser.add_argument(
        "--mode",
        choices=["predict", "unroll"],
        default="predict",
        help="predict: Dilation 예측 (기본), unroll: 실제 LRU 시뮬레이션",
    )
    parser.add_argument(
        "--plot",
        action="store_true",
        help="seaborn 히스토그램 저장 (figs/<stem>.png). --save와 함께 쓰면 --save 경로 우선",
    )
    parser.add_argument(
        "--save",
        metavar="PATH",
        help="플롯을 지정 경로에 저장 (PNG/PDF/SVG 등)",
    )
    args = parser.parse_args()

    plugin = Path(args.plugin)
    if not plugin.exists():
        print(f"오류: 플러그인을 찾을 수 없습니다: {plugin}", file=sys.stderr)
        print("  빌드 후 다시 시도하거나 --plugin 으로 경로를 지정하세요.", file=sys.stderr)
        sys.exit(1)

    block_results: List[Tuple[str, ReuseProfile]] = []
    for file_str in args.files:
        path = Path(file_str)
        if not path.exists():
            print(f"오류: 파일 없음: {path}", file=sys.stderr)
            continue
        if path.suffix not in (".c", ".ll"):
            print(f"오류: .c 또는 .ll 파일만 지원합니다: {path}", file=sys.stderr)
            continue
        try:
            _, blocks = analyze_file(path, plugin, args.mode)
            block_results.extend(blocks)
        except subprocess.CalledProcessError as e:
            stderr = e.stderr.decode(errors="replace") if e.stderr else ""
            print(f"\n오류: {e.args[0][0]} 실패\n{stderr}", file=sys.stderr)
        except Exception as e:
            print(f"\n오류: {e}", file=sys.stderr)

    if block_results and (args.plot or args.save):
        if args.save:
            base = Path(args.save)
        else:
            figs_dir = _REPO_ROOT / "figs"
            figs_dir.mkdir(exist_ok=True)
            stems = "_".join(Path(f).stem for f in args.files)
            base = figs_dir / f"{stems}.png"

        plot_histograms(block_results, base.with_stem(base.stem + "_blocks"))
        reusable_results = [
            row for row in block_results
            if any(profile.histogram for profile in row[1:])
        ]
        program_label = (
            Path(args.files[0]).name
            if len(args.files) == 1
            else ", ".join(Path(f).name for f in args.files)
        )
        program_results = aggregate_as_program(reusable_results, label=program_label)
        if program_results:
            plot_histograms(program_results, base.with_stem(base.stem + "_program"))


if __name__ == "__main__":
    main()
