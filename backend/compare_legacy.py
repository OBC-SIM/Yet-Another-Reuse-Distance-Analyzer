"""Compare ground truth, legacy analyzer output, and current prediction.

Usage:
    python backend/compare_legacy.py tasks/polybench_atax.c
    python backend/compare_legacy.py tasks/polybench_*.c --save figs/legacy_compare
"""

import argparse
import json
import math
import os
import subprocess
import sys
from collections import defaultdict
from pathlib import Path
from typing import List, Tuple

os.environ.setdefault("MPLBACKEND", "Agg")
os.environ.setdefault("MPLCONFIGDIR", "/tmp/matplotlib")

sys.path.insert(0, str(Path(__file__).parent))

from _plot_utils import (  # noqa: E402
    _save_figure,
    _setup_theme,
)
from lru_sim import ReuseProfile  # noqa: E402
from main import _DEFAULT_PLUGIN, _REPO_ROOT, _to_ll, run_llvm_pass  # noqa: E402
from verify import verify_json  # noqa: E402

_DEFAULT_LEGACY_DIR = (
    Path("/workspace/caas/MEM_RD_IR/Static-Memory-Reuse-Distance-on-LLVM/results")
)


def _add_to_hist(dst: dict, src: dict) -> None:
    for rd, count in src.items():
        dst[rd] = dst.get(rd, 0) + count


def _merge_profiles(results: list, gt_idx: int, pred_idx: int) -> Tuple[ReuseProfile, ReuseProfile]:
    gt, pred = ReuseProfile(), ReuseProfile()
    for block_idx, entry in enumerate(results):
        gt_src, pred_src = entry[gt_idx], entry[pred_idx]
        _add_to_hist(gt.histogram, gt_src.histogram)
        _add_to_hist(pred.histogram, pred_src.histogram)
        gt.cold_misses |= {f"{block_idx}:gt:{i}" for i in range(len(gt_src.cold_misses))}
        pred.cold_misses |= {f"{block_idx}:pred:{i}" for i in range(len(pred_src.cold_misses))}
    return gt, pred


def _legacy_profile(json_path: Path) -> ReuseProfile:
    profile = ReuseProfile()
    data = json.loads(json_path.read_text())
    cold_idx = 0
    for entry in data:
        for address_map in entry.get("functions", {}).values():
            for rd_values in address_map.values():
                for rd in rd_values:
                    rd = int(rd)
                    if rd < 0:
                        profile.cold_misses.add(f"legacy-cold:{cold_idx}")
                        cold_idx += 1
                    else:
                        profile.histogram[rd] = profile.histogram.get(rd, 0) + 1
    return profile


def _bin_counts(profiles: list[ReuseProfile]) -> Tuple[list[str], list[list[int]]]:
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


def _plot_three_bars(ax, labels: list[str], counts_by_series: list[list[int]]) -> list:
    import numpy as np

    names = ["Ground Truth", "Legacy", "Current"]
    colors = ["#4C72B0", "#55A868", "#DD8452"]
    x_pos = np.arange(len(labels))
    width = 0.26
    bar_groups = []
    for idx, (name, color, counts) in enumerate(zip(names, colors, counts_by_series)):
        bars = ax.bar(
            x_pos + (idx - 1) * width,
            counts,
            width=width,
            label=name,
            color=color,
            edgecolor="black",
            linewidth=0.5,
        )
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


def _normalize_counts(counts_by_series: list[list[int]]) -> list[list[float]]:
    normalized = []
    for counts in counts_by_series:
        total = sum(counts)
        if total == 0:
            normalized.append([0.0 for _ in counts])
        else:
            normalized.append([count * 100.0 / total for count in counts])
    return normalized


def plot_comparison(
    label: str,
    gt: ReuseProfile,
    legacy: ReuseProfile,
    current: ReuseProfile,
    save_path: Path,
    normalize: bool = False,
) -> None:
    os.environ.setdefault("MPLBACKEND", "Agg")
    import matplotlib.pyplot as plt
    import seaborn as sns
    from matplotlib.ticker import MaxNLocator

    _setup_theme()
    labels, counts_by_series = _bin_counts([gt, legacy, current])
    if normalize:
        counts_by_series = _normalize_counts(counts_by_series)
    counts = [value for series in counts_by_series for value in series]
    fig = plt.figure(figsize=(6.0, 3.4))
    ax = fig.add_subplot(111)

    if not labels:
        ax.text(0.5, 0.5, "No reuse", ha="center", va="center")
    else:
        bar_groups = _plot_three_bars(ax, labels, counts_by_series)
        if normalize:
            ax.set_ylabel("Percentage (%)", labelpad=12)
            ax.yaxis.set_major_locator(MaxNLocator(integer=True))
        else:
            ax.set_ylabel("Frequency (symlog scale)", labelpad=12)
            ax.set_yscale("symlog", linthresh=1)
            _annotate_legacy(ax, bar_groups[1], counts_by_series[1])

    ax.set_title(label, fontsize=13, pad=14)
    ax.set_ylim(0, 100 if normalize else max(1, max(counts, default=0) * 2.0))
    ax.set_xlabel("Reuse Distance")
    sns.despine(ax=ax)

    handles, names = ax.get_legend_handles_labels()
    fig.subplots_adjust(left=0.12, right=0.98, bottom=0.30, top=0.84)
    fig.legend(handles, names, loc="lower center", ncol=3, fontsize=9, frameon=False)
    _save_figure(fig, save_path)
    print(f"  플롯 저장 → {save_path}")


def _legacy_path_for(c_path: Path, legacy_dir: Path) -> Path:
    return legacy_dir / f"{c_path.stem}.json"


def _save_path_for(c_path: Path, save_arg: str | None, file_count: int) -> Path:
    if save_arg is None:
        figs_dir = _REPO_ROOT / "figs"
        figs_dir.mkdir(exist_ok=True)
        return figs_dir / f"legacy_compare_{c_path.stem}.png"

    save_path = Path(save_arg)
    if file_count == 1 and save_path.suffix:
        save_path.parent.mkdir(parents=True, exist_ok=True)
        return save_path
    save_path.mkdir(parents=True, exist_ok=True)
    return save_path / f"legacy_compare_{c_path.stem}.png"


def _variant_path(base: Path, suffix: str) -> Path:
    return base.with_stem(f"{base.stem}_{suffix}")


def compare_file(c_path: Path, legacy_path: Path, plugin: Path, save_path: Path) -> None:
    print(f"\n[pipeline] {c_path.name} ...", flush=True)
    ll_path = _to_ll(c_path)
    lat_json = run_llvm_pass(ll_path, plugin)
    print(f"  opt-14 완료 → {lat_json.name}")
    results, _ = verify_json(lat_json)
    gt, current = _merge_profiles(results, 1, 2)
    legacy = _legacy_profile(legacy_path)
    plot_comparison(c_path.name, gt, legacy, current, _variant_path(save_path, "count"))
    plot_comparison(
        f"{c_path.name} (normalized)",
        gt,
        legacy,
        current,
        _variant_path(save_path, "pct"),
        normalize=True,
    )


def main() -> None:
    parser = argparse.ArgumentParser(
        description="RDH 비교 플롯 생성: Ground Truth vs Legacy vs Current"
    )
    parser.add_argument("files", nargs="+", help=".c 파일")
    parser.add_argument("--legacy-dir", default=str(_DEFAULT_LEGACY_DIR))
    parser.add_argument("--legacy-json", help="단일 파일 비교용 legacy JSON")
    parser.add_argument("--plugin", default=str(_DEFAULT_PLUGIN), metavar="PATH")
    parser.add_argument("--save", help="출력 PNG 경로 또는 디렉토리")
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

        legacy_path = Path(args.legacy_json) if args.legacy_json else _legacy_path_for(
            c_path, Path(args.legacy_dir)
        )
        if not legacy_path.exists():
            print(f"오류: legacy JSON 없음: {legacy_path}", file=sys.stderr)
            continue

        save_path = _save_path_for(c_path, args.save, len(files))
        try:
            compare_file(c_path, legacy_path, plugin, save_path)
        except subprocess.CalledProcessError as exc:
            stderr = exc.stderr.decode(errors="replace") if exc.stderr else ""
            print(f"\n오류: {exc.args[0][0]} 실패\n{stderr}", file=sys.stderr)
        except Exception as exc:
            print(f"\n오류: {exc}", file=sys.stderr)


if __name__ == "__main__":
    main()
