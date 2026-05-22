"""
plot.py — 재사용 거리 히스토그램 시각화 유틸리티.
"""

import math
import os
from collections import defaultdict
from pathlib import Path
from typing import List, Tuple

from lru_sim import ReuseProfile
from _plot_utils import (
    _add_break_band, _add_break_marks, _bar_ylim, _bin_histogram,
    _broken_axis_limits, _plot_grouped_bars, _plot_single_bars,
    _save_figure, _setup_theme,
)


def plot_histograms(
    results: List[Tuple[str, ReuseProfile]],
    save_path: Path | None,
) -> None:
    """예측 RDH를 블록별 subplot에 그린다. scale gap이 크면 broken axis로 분리."""
    os.environ.setdefault("MPLBACKEND", "Agg")
    import matplotlib.pyplot as plt
    import seaborn as sns
    from matplotlib.ticker import MaxNLocator

    _setup_theme()
    n = len(results)
    series = []
    has_break = False
    for label, profile in results:
        labels, counts = _bin_histogram(profile.histogram)
        cold = len(profile.cold_misses)
        if cold:
            labels, counts = ["-1\n(cold)"] + labels, [cold] + counts
        limits = _broken_axis_limits(counts)
        has_break = has_break or limits is not None
        series.append((label, labels, counts, limits))

    if has_break:
        fig = plt.figure(figsize=(4.0 * n, 3.8))
        gs = fig.add_gridspec(2, n, height_ratios=[3, 1], hspace=0.0)
        top_axes = [fig.add_subplot(gs[0, i]) for i in range(n)]
        bot_axes = [fig.add_subplot(gs[1, i]) for i in range(n)]
    else:
        fig = plt.figure(figsize=(4.0 * n, 3.0))
        gs = fig.add_gridspec(1, n)
        top_axes = [fig.add_subplot(gs[0, i]) for i in range(n)]
        bot_axes = [None] * n

    break_axes = []
    for i, (label, labels, counts, limits) in enumerate(series):
        ax, ax_cold = top_axes[i], bot_axes[i]
        if not counts:
            ax.text(0.5, 0.5, "No reuse", ha="center", va="center")
            ax.set_title(label, fontsize=13, pad=14)
            if ax_cold:
                ax_cold.set_visible(False)
            continue

        _plot_single_bars(ax, labels, counts)
        ax.set_ylabel("Frequency", labelpad=12)
        ax.yaxis.set_major_locator(MaxNLocator(integer=True))
        ax.set_title(label, fontsize=13, pad=14)

        if ax_cold is not None and limits is not None:
            low_max, high_min = limits
            _plot_single_bars(ax_cold, labels, counts)
            ax.set_ylim(high_min, max(counts) * 1.18)
            ax_cold.set_ylim(0, low_max)
            ax.tick_params(labelbottom=False, bottom=False)
            ax.yaxis.set_major_locator(MaxNLocator(integer=True, prune="lower"))
            ax_cold.yaxis.set_major_locator(MaxNLocator(integer=True, nbins=2, prune="upper"))
            ax_cold.set_xlabel("Reuse Distance")
            _add_break_marks(ax, ax_cold)
            break_axes.append((ax, ax_cold))
            sns.despine(ax=ax)
            sns.despine(ax=ax_cold)
        else:
            _bar_ylim(ax, counts)
            ax.set_xlabel("Reuse Distance")
            sns.despine(ax=ax)
            if ax_cold is not None:
                ax_cold.set_visible(False)

    fig.subplots_adjust(left=0.14 if n == 1 else 0.08, right=0.98,
                        bottom=0.28, top=0.84, hspace=0.0)
    for ax, ax_cold in break_axes:
        _add_break_band(ax, ax_cold)
    if save_path:
        _save_figure(fig, save_path)


def plot_verify_comparison(
    results: List[Tuple[str, ReuseProfile, ReuseProfile]],
    save_path: Path | None,
) -> None:
    """블록별 GT vs. 예측 RDH를 grouped bar chart로 비교한다.

    @param results  (label, gt_profile, pred_profile) 리스트
    @param save_path  저장 경로. None이면 저장 안 함.
    """
    os.environ.setdefault("MPLBACKEND", "Agg")
    import matplotlib.pyplot as plt
    import seaborn as sns
    from matplotlib.ticker import MaxNLocator

    _setup_theme()
    n = len(results)
    series = []
    has_break = False
    for label, gt, pred in results:
        merged_gt: dict = defaultdict(int)
        merged_pred: dict = defaultdict(int)
        all_rds = sorted(set(gt.histogram) | set(pred.histogram))
        for rd in all_rds:
            key = rd if rd <= 1 else 2 ** int(math.log2(rd))
            merged_gt[key] += gt.histogram.get(rd, 0)
            merged_pred[key] += pred.histogram.get(rd, 0)

        labels, gt_counts, pred_counts = [], [], []
        gt_cold, pred_cold = len(gt.cold_misses), len(pred.cold_misses)
        if gt_cold or pred_cold:
            labels.append("-1\n(cold)")
            gt_counts.append(gt_cold)
            pred_counts.append(pred_cold)
        for k in sorted(set(merged_gt) | set(merged_pred)):
            labels.append("0" if k == 0 else "1" if k == 1 else f"{k}-{k*2-1}")
            gt_counts.append(merged_gt[k])
            pred_counts.append(merged_pred[k])

        limits = _broken_axis_limits(gt_counts + pred_counts)
        has_break = has_break or limits is not None
        series.append((label, labels, gt_counts, pred_counts, limits))

    if has_break:
        fig = plt.figure(figsize=(4.5 * n, 4.2))
        gs = fig.add_gridspec(2, n, height_ratios=[3, 1], hspace=0.0)
        top_axes = [fig.add_subplot(gs[0, i]) for i in range(n)]
        bot_axes = [fig.add_subplot(gs[1, i]) for i in range(n)]
    else:
        fig = plt.figure(figsize=(4.5 * n, 3.2))
        gs = fig.add_gridspec(1, n)
        top_axes = [fig.add_subplot(gs[0, i]) for i in range(n)]
        bot_axes = [None] * n

    break_axes = []
    for i, (label, labels, gt_counts, pred_counts, limits) in enumerate(series):
        ax, ax_cold = top_axes[i], bot_axes[i]
        if not labels:
            ax.text(0.5, 0.5, "No reuse", ha="center", va="center")
            ax.set_title(label, fontsize=13, pad=14)
            if ax_cold:
                ax_cold.set_visible(False)
            continue

        _plot_grouped_bars(ax, labels, gt_counts, pred_counts)
        ax.set_ylabel("Frequency", labelpad=12)
        ax.yaxis.set_major_locator(MaxNLocator(integer=True))
        ax.set_title(label, fontsize=13, pad=14)

        counts = gt_counts + pred_counts
        if ax_cold is not None and limits is not None:
            low_max, high_min = limits
            _plot_grouped_bars(ax_cold, labels, gt_counts, pred_counts)
            ax.set_ylim(high_min, max(counts) * 1.18)
            ax_cold.set_ylim(0, low_max)
            ax.tick_params(labelbottom=False, bottom=False)
            ax.yaxis.set_major_locator(MaxNLocator(integer=True, prune="lower"))
            ax_cold.yaxis.set_major_locator(MaxNLocator(integer=True, nbins=2, prune="upper"))
            ax_cold.set_xlabel("Reuse Distance")
            _add_break_marks(ax, ax_cold)
            break_axes.append((ax, ax_cold))
            sns.despine(ax=ax)
            sns.despine(ax=ax_cold)
        else:
            _bar_ylim(ax, counts)
            ax.set_xlabel("Reuse Distance")
            sns.despine(ax=ax)
            if ax_cold is not None:
                ax_cold.set_visible(False)

    handles, leg_labels = top_axes[0].get_legend_handles_labels()
    fig.subplots_adjust(left=0.14 if n == 1 else 0.08, right=0.98,
                        bottom=0.30 if handles else 0.26, top=0.84, hspace=0.0)
    if handles:
        axes_left = min(ax.get_position().x0 for ax in top_axes)
        axes_right = max(ax.get_position().x1 for ax in top_axes)
        fig.legend(handles, leg_labels, loc="lower center", ncol=2,
                   fontsize=9, frameon=False,
                   bbox_to_anchor=((axes_left + axes_right) / 2, 0.035))
    for ax, ax_cold in break_axes:
        _add_break_band(ax, ax_cold)
    if save_path:
        _save_figure(fig, save_path)
        print(f"  플롯 저장 → {save_path}")


def aggregate_as_program(results: list, label: str = "program") -> list:
    """여러 블록 결과를 program-level ReuseProfile 하나로 합산한다."""
    if not results:
        return []
    profile_count = len(results[0]) - 1
    merged = tuple(ReuseProfile() for _ in range(profile_count))
    for block_idx, entry in enumerate(results):
        for profile_idx, (dst, src) in enumerate(zip(merged, entry[1:])):
            for rd, cnt in src.histogram.items():
                dst.histogram[rd] = dst.histogram.get(rd, 0) + cnt
            dst.cold_misses |= {
                f"{block_idx}:{profile_idx}:{i}" for i in range(len(src.cold_misses))
            }
    return [(label,) + merged]
