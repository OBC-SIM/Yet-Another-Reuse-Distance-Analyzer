"""Compare unrolling and legacy analyzer output."""

import argparse
import json
import math
import os
import subprocess
import sys
from collections import defaultdict
from pathlib import Path

os.environ.setdefault("MPLBACKEND", "Agg")
os.environ.setdefault("MPLCONFIGDIR", "/tmp/matplotlib")

sys.path.insert(0, str(Path(__file__).parent))

from _plot_utils import (  # noqa: E402
    _save_figure,
    _setup_theme,
)
from block_trace import function_trace  # noqa: E402
from calls import expand_calls  # noqa: E402
from lru_sim import LRUProfiler, ReuseProfile  # noqa: E402
from main import _DEFAULT_PLUGIN, _REPO_ROOT, _to_ll, run_llvm_pass  # noqa: E402

_DEFAULT_LEGACY_DIR = (
    Path("/workspace/caas/MEM_RD_IR/Static-Memory-Reuse-Distance-on-LLVM/results")
)


def _add_to_hist(dst: dict, src: dict) -> None:
    for rd, count in src.items():
        dst[rd] = dst.get(rd, 0) + count


def _expanded_lat(lat_json: Path) -> list[dict]:
    return expand_calls(json.loads(lat_json.read_text()))


def _ground_truth_profile(raw: list[dict]) -> ReuseProfile:
    profile = ReuseProfile()
    for func_idx, func_entry in enumerate(raw):
        func_profile = LRUProfiler.calculate(function_trace(func_entry))
        _add_to_hist(profile.histogram, func_profile.histogram)
        profile.cold_misses |= {
            f"{func_idx}:gt:{i}" for i in range(len(func_profile.cold_misses))
        }
    return profile


def _legacy_profile(json_path: Path, include_funcs: set[str]) -> ReuseProfile:
    profile = ReuseProfile()
    data = json.loads(json_path.read_text())
    cold_idx = 0
    for entry in data:
        for func_name, address_map in entry.get("functions", {}).items():
            if func_name not in include_funcs:
                continue
            for rd_values in address_map.values():
                for rd in rd_values:
                    rd = int(rd)
                    if rd < 0:
                        profile.cold_misses.add(f"legacy-cold:{cold_idx}")
                        cold_idx += 1
                    else:
                        profile.histogram[rd] = profile.histogram.get(rd, 0) + 1
    return profile


def _bin_counts(profiles: list[ReuseProfile]) -> tuple[list[str], list[list[int]]]:
    merged = [defaultdict(int) for _ in profiles]
    for i, profile in enumerate(profiles):
        for rd, count in profile.histogram.items():
            key = rd if rd <= 1 else 2 ** int(math.log2(rd))
            merged[i][key] += count

    labels, counts_by_series = [], [[] for _ in profiles]
    cold_counts = [len(profile.cold_misses) for profile in profiles]
    if any(cold_counts):
        labels.append("-1\n(cold)")
        for counts, cold in zip(counts_by_series, cold_counts):
            counts.append(cold)

    for key in sorted(set().union(*(set(hist) for hist in merged))):
        labels.append("0" if key == 0 else "1" if key == 1 else f"{key}-{key * 2 - 1}")
        for counts, hist in zip(counts_by_series, merged):
            counts.append(hist.get(key, 0))
    return labels, counts_by_series


def _plot_two_bars(ax, labels: list[str], counts_by_series: list[list[int]]) -> list:
    import numpy as np

    names = ["Unrolling", "Legacy"]
    colors = ["#4C72B0", "#55A868"]
    x_pos = np.arange(len(labels))
    width = 0.34
    bar_groups = []
    for idx, (name, color, counts) in enumerate(zip(names, colors, counts_by_series)):
        bars = ax.bar(x_pos + (idx - 0.5) * width, counts, width=width,
                      label=name, color=color, edgecolor="black", linewidth=0.5)
        bar_groups.append(bars)
    ax.set_xticks(x_pos)
    ax.set_xticklabels(labels, rotation=45, ha="right", fontsize=8)
    return bar_groups


def _annotate_legacy(ax, legacy_bars, legacy_counts: list[int]) -> None:
    for bar, count in zip(legacy_bars, legacy_counts):
        if count <= 0:
            continue
        ax.text(
            bar.get_x() + bar.get_width() / 2,
            count,
            str(int(count)),
            ha="center",
            va="bottom",
            fontsize=6,
            rotation=90,
        )


def _weighted_mean_rd(profile: ReuseProfile) -> float:
    total = sum(profile.histogram.values())
    if total == 0:
        return 0.0
    return sum(rd * count for rd, count in profile.histogram.items()) / total


def _ca_score(mean_rd: float) -> float:
    return 1.0 / (1.0 + mean_rd)


def _comparison_metrics(gt: ReuseProfile, legacy: ReuseProfile) -> dict:
    gt_mean, legacy_mean = _weighted_mean_rd(gt), _weighted_mean_rd(legacy)
    gt_score, legacy_score = _ca_score(gt_mean), _ca_score(legacy_mean)
    gt_reuses, legacy_reuses = sum(gt.histogram.values()), sum(legacy.histogram.values())
    score_error = legacy_score - gt_score
    return {
        "mean_rd_unrolling": gt_mean,
        "mean_rd_legacy": legacy_mean,
        "ca_score_unrolling": gt_score,
        "ca_score_legacy": legacy_score,
        "ca_score_error": score_error,
        "ca_score_relative_error": score_error / gt_score if gt_score else 0.0,
        "gt_reuses": gt_reuses,
        "legacy_reuses": legacy_reuses,
        "reuse_count_error": legacy_reuses - gt_reuses,
        "cold_miss_error": len(legacy.cold_misses) - len(gt.cold_misses),
    }


def plot_comparison(
    label: str,
    gt: ReuseProfile,
    legacy: ReuseProfile,
    save_path: Path,
) -> None:
    os.environ.setdefault("MPLBACKEND", "Agg")
    import matplotlib.pyplot as plt
    import seaborn as sns
    _setup_theme()
    labels, counts_by_series = _bin_counts([gt, legacy])
    counts = [value for series in counts_by_series for value in series]
    fig = plt.figure(figsize=(6.0, 3.4))
    ax = fig.add_subplot(111)

    if not labels:
        ax.text(0.5, 0.5, "No reuse", ha="center", va="center")
    else:
        bar_groups = _plot_two_bars(ax, labels, counts_by_series)
        ax.set_ylabel("Frequency (symlog scale)", labelpad=12)
        ax.set_yscale("symlog", linthresh=1)
        _annotate_legacy(ax, bar_groups[1], counts_by_series[1])

    ax.set_title(label, fontsize=13, pad=14)
    ax.set_ylim(0, max(1, max(counts, default=0) * 2.0))
    ax.set_xlabel("Reuse Distance")
    sns.despine(ax=ax)

    handles, names = ax.get_legend_handles_labels()
    metrics = _comparison_metrics(gt, legacy)
    score_text = (
        "CA score  Unrolling: {ca_score_unrolling:.6f}   "
        "Legacy: {ca_score_legacy:.6f}   "
        "Delta: {ca_score_error:+.6f}"
    ).format(**metrics)
    fig.subplots_adjust(left=0.12, right=0.98, bottom=0.42, top=0.84)
    axes_center = (ax.get_position().x0 + ax.get_position().x1) / 2
    fig.legend(handles, names, loc="lower center", ncol=2, fontsize=9,
               frameon=False, bbox_to_anchor=(axes_center, 0.09))
    fig.text(axes_center, 0.025, score_text, ha="center", va="bottom", fontsize=8)
    _save_figure(fig, save_path)
    print(f"  플롯 저장 → {save_path}")


def _legacy_path_for(c_path: Path, legacy_dir: Path) -> Path:
    return legacy_dir / f"{c_path.stem}.json"


def _plot_path_for(c_path: Path) -> Path:
    figs_dir = _REPO_ROOT / "figs" / "compare"
    figs_dir.mkdir(exist_ok=True)
    return figs_dir / f"legacy_compare_{c_path.stem}.png"


def _export_path_for(c_path: Path, export_arg: str, file_count: int) -> Path:
    export_path = Path(export_arg)
    if file_count == 1 and export_path.suffix:
        export_path.parent.mkdir(parents=True, exist_ok=True)
        return export_path
    export_path.mkdir(parents=True, exist_ok=True)
    return export_path / f"legacy_compare_{c_path.stem}.json"


def _profile_json(profile: ReuseProfile) -> dict:
    return {
        "histogram": {str(rd): count for rd, count in sorted(profile.histogram.items())},
        "cold_misses": len(profile.cold_misses),
        "total_reuses": sum(profile.histogram.values()),
    }


def export_comparison(label: str, gt: ReuseProfile, legacy: ReuseProfile, path: Path) -> None:
    payload = {
        "label": label,
        "unrolling": _profile_json(gt),
        "legacy": _profile_json(legacy),
        "metrics": _comparison_metrics(gt, legacy),
    }
    path.write_text(json.dumps(payload, indent=2, sort_keys=True), encoding="utf-8")
    print(f"  JSON 저장 → {path}")


def compare_file(c_path: Path, legacy_path: Path, plugin: Path,
                 save_path: Path | None, export_path: Path | None) -> None:
    print(f"\n[pipeline] {c_path.name} ...", flush=True)
    ll_path = _to_ll(c_path)
    lat_json = run_llvm_pass(ll_path, plugin)
    print(f"  opt-14 완료 → {lat_json.name}")
    raw = _expanded_lat(lat_json)
    include_funcs = {entry["function"] for entry in raw}
    print(f"  legacy 필터 → {', '.join(sorted(include_funcs))}")
    gt = _ground_truth_profile(raw)
    legacy = _legacy_profile(legacy_path, include_funcs)
    metrics = _comparison_metrics(gt, legacy)
    print("  mean_rd: unrolling={:.2f} legacy={:.2f}".format(
        metrics["mean_rd_unrolling"], metrics["mean_rd_legacy"]))
    print("  ca_score: unrolling={:.6f} legacy={:.6f} error={:+.6f} ({:+.2f}%)".format(
        metrics["ca_score_unrolling"], metrics["ca_score_legacy"],
        metrics["ca_score_error"], metrics["ca_score_relative_error"] * 100))
    print("  reuse_count: unrolling={} legacy={} error={:+d} cold_error={:+d}".format(
        metrics["gt_reuses"], metrics["legacy_reuses"],
        metrics["reuse_count_error"], metrics["cold_miss_error"]))
    if save_path:
        plot_comparison(c_path.name, gt, legacy, save_path)
    if export_path:
        export_comparison(c_path.name, gt, legacy, export_path)


def main() -> None:
    parser = argparse.ArgumentParser(description="RDH 비교 플롯 생성: Unrolling vs Legacy")
    parser.add_argument("files", nargs="+", help=".c 파일")
    parser.add_argument("--legacy-dir", default=str(_DEFAULT_LEGACY_DIR))
    parser.add_argument("--legacy-json", help="단일 파일 비교용 legacy JSON")
    parser.add_argument("--plugin", default=str(_DEFAULT_PLUGIN), metavar="PATH")
    parser.add_argument("--plot", action="store_true", help="비교 플롯을 figs/에 저장")
    parser.add_argument("--export", help="비교 결과 JSON 경로 또는 디렉토리")
    args = parser.parse_args()

    plugin = Path(args.plugin)
    if not plugin.exists():
        print(f"오류: 플러그인을 찾을 수 없습니다: {plugin}", file=sys.stderr)
        sys.exit(1)

    files = [Path(file_str) for file_str in args.files]
    for c_path in files:
        if not c_path.exists() or c_path.suffix != ".c":
            print(f"오류: .c 파일만 지원합니다: {c_path}", file=sys.stderr)
            continue

        legacy_path = Path(args.legacy_json) if args.legacy_json else _legacy_path_for(c_path, Path(args.legacy_dir))
        if not legacy_path.exists():
            print(f"오류: legacy JSON 없음: {legacy_path}", file=sys.stderr)
            continue

        save_path = _plot_path_for(c_path) if args.plot else None
        export_path = _export_path_for(c_path, args.export, len(files)) if args.export else None
        try:
            compare_file(c_path, legacy_path, plugin, save_path, export_path)
        except subprocess.CalledProcessError as exc:
            stderr = exc.stderr.decode(errors="replace") if exc.stderr else ""
            print(f"\n오류: {exc.args[0][0]} 실패\n{stderr}", file=sys.stderr)
        except Exception as exc:
            print(f"\n오류: {exc}", file=sys.stderr)


if __name__ == "__main__":
    main()
