"""
plot.py — 재사용 거리 히스토그램 시각화 유틸리티.
"""

import math
import os
from collections import defaultdict
from pathlib import Path
from typing import List, Tuple

from lru_sim import ReuseProfile


def _bin_histogram(hist: dict) -> Tuple[List[str], List[int]]:
    """RD 히스토그램을 2의 제곱수 단위로 binning."""
    binned: dict = defaultdict(int)
    for rd, count in hist.items():
        if rd <= 1:
            binned[rd] += count
        else:
            bin_idx = int(math.log2(rd))
            binned[2**bin_idx] += count
    labels, counts = [], []
    for k in sorted(binned.keys()):
        if k == 0:
            labels.append("0")
        elif k == 1:
            labels.append("1")
        else:
            labels.append(f"{k}-{k*2-1}")
        counts.append(binned[k])
    return labels, counts


def _setup_theme():
    import seaborn as sns
    import matplotlib.pyplot as plt

    sns.set_theme(style="ticks", context="paper")
    plt.rcParams.update({
        "font.family": "serif",
        "font.serif": ["DejaVu Serif", "Liberation Serif", "Times New Roman"],
        "pdf.fonttype": 42,
        "axes.linewidth": 1.0,
        "xtick.direction": "in",
        "ytick.direction": "in",
    })


def _add_break_marks(ax_top, ax_bot) -> None:
    """두 축 사이에 절단 표시(//)를 그린다."""
    d = 0.012
    kw = dict(color="k", clip_on=False, linewidth=1.2, transform=ax_top.transAxes)
    ax_top.plot((-d, +d), (-d * 2, +d * 2), **kw)
    ax_top.plot((1 - d, 1 + d), (-d * 2, +d * 2), **kw)
    kw["transform"] = ax_bot.transAxes
    ax_bot.plot((-d, +d), (1 - d * 2, 1 + d * 2), **kw)
    ax_bot.plot((1 - d, 1 + d), (1 - d * 2, 1 + d * 2), **kw)
    ax_top.spines["bottom"].set_visible(False)
    ax_bot.spines["top"].set_visible(False)
    ax_top.tick_params(bottom=False, labelbottom=False)


def plot_histograms(
    results: List[Tuple[str, ReuseProfile]],
    save_path: Path | None,
) -> None:
    """예측 RDH를 파일별로 subplot에 그린다. cold miss는 broken axis로 분리."""
    os.environ.setdefault("MPLBACKEND", "Agg")
    import matplotlib.pyplot as plt
    import seaborn as sns
    from matplotlib.ticker import MaxNLocator

    _setup_theme()

    n = len(results)
    has_cold = any(p.cold_misses for _, p in results)

    if has_cold:
        fig = plt.figure(figsize=(4.0 * n, 3.8))
        gs = fig.add_gridspec(2, n, height_ratios=[3, 1], hspace=0.05)
        top_axes = [fig.add_subplot(gs[0, i]) for i in range(n)]
        bot_axes = [fig.add_subplot(gs[1, i]) for i in range(n)]
    else:
        fig = plt.figure(figsize=(4.0 * n, 3.0))
        gs = fig.add_gridspec(1, n)
        top_axes = [fig.add_subplot(gs[0, i]) for i in range(n)]
        bot_axes = [None] * n

    for i, (label, profile) in enumerate(results):
        ax = top_axes[i]
        ax_cold = bot_axes[i]
        cold = len(profile.cold_misses)

        if not profile.histogram and not cold:
            ax.text(0.5, 0.5, "No reuse", ha="center", va="center")
            ax.set_title(label)
            if ax_cold:
                ax_cold.set_visible(False)
            continue

        if profile.histogram:
            labels, counts = _bin_histogram(profile.histogram)
            x_pos = range(len(labels))
            ax.bar(x_pos, counts, color="#4C72B0", edgecolor="black", linewidth=0.5, width=0.8)
            ax.set_xticks(x_pos)
            ax.set_xticklabels(labels, rotation=45, ha="right", fontsize=9)
            ax.set_ylabel("Frequency")
            ax.yaxis.set_major_locator(MaxNLocator(integer=True))
            ax.set_title(label, fontsize=11)
            sns.despine(ax=ax)

        if ax_cold is not None:
            if cold:
                ax_cold.bar([0], [cold], color="#4C72B0", edgecolor="black",
                            linewidth=0.5, width=0.5)
                ax_cold.set_xticks([0])
                ax_cold.set_xticklabels(["-1\n(cold)"], fontsize=9)
                ax_cold.set_xlabel("Reuse Distance")
                ax_cold.yaxis.set_major_locator(MaxNLocator(integer=True, nbins=2))
                sns.despine(ax=ax_cold)
                if profile.histogram:
                    _add_break_marks(ax, ax_cold)
            else:
                ax_cold.set_visible(False)
                ax.set_xlabel("Reuse Distance")
        else:
            ax.set_xlabel("Reuse Distance")

    plt.tight_layout()
    if save_path:
        fig.savefig(save_path, dpi=300, bbox_inches="tight", transparent=True)


def plot_verify_comparison(
    results: List[Tuple[str, ReuseProfile, ReuseProfile]],
    save_path: Path | None,
) -> None:
    """블록별 GT vs. 예측 RDH를 grouped bar chart로 비교한다. cold miss는 broken axis로 분리.

    @param results  (label, gt_profile, pred_profile) 리스트
    @param save_path  저장 경로. None이면 저장 안 함.
    """
    os.environ.setdefault("MPLBACKEND", "Agg")
    import matplotlib.pyplot as plt
    import numpy as np
    import seaborn as sns
    from matplotlib.ticker import MaxNLocator

    _setup_theme()

    n = len(results)
    has_cold = any(gt.cold_misses or pred.cold_misses for _, gt, pred in results)

    if has_cold:
        fig = plt.figure(figsize=(4.5 * n, 4.2))
        gs = fig.add_gridspec(2, n, height_ratios=[3, 1], hspace=0.05)
        top_axes = [fig.add_subplot(gs[0, i]) for i in range(n)]
        bot_axes = [fig.add_subplot(gs[1, i]) for i in range(n)]
    else:
        fig = plt.figure(figsize=(4.5 * n, 3.2))
        gs = fig.add_gridspec(1, n)
        top_axes = [fig.add_subplot(gs[0, i]) for i in range(n)]
        bot_axes = [None] * n

    for i, (label, gt, pred) in enumerate(results):
        ax = top_axes[i]
        ax_cold = bot_axes[i]
        gt_cold = len(gt.cold_misses)
        pred_cold = len(pred.cold_misses)
        all_rds = sorted(set(gt.histogram) | set(pred.histogram))

        if not all_rds and not gt_cold and not pred_cold:
            ax.text(0.5, 0.5, "No reuse", ha="center", va="center")
            ax.set_title(label, fontsize=9)
            if ax_cold:
                ax_cold.set_visible(False)
            continue

        if all_rds:
            merged_gt: dict = defaultdict(int)
            merged_pred: dict = defaultdict(int)
            for rd in all_rds:
                key = rd if rd <= 1 else 2 ** int(math.log2(rd))
                merged_gt[key] += gt.histogram.get(rd, 0)
                merged_pred[key] += pred.histogram.get(rd, 0)

            bin_keys = sorted(set(merged_gt) | set(merged_pred))
            bin_labels = []
            for k in bin_keys:
                if k == 0:
                    bin_labels.append("0")
                elif k == 1:
                    bin_labels.append("1")
                else:
                    bin_labels.append(f"{k}-{k*2-1}")

            x = np.arange(len(bin_labels))
            w = 0.38
            ax.bar(x - w/2, [merged_gt[k] for k in bin_keys], width=w,
                   label="Ground Truth", color="#4C72B0", edgecolor="black", linewidth=0.5)
            ax.bar(x + w/2, [merged_pred[k] for k in bin_keys], width=w,
                   label="Predicted", color="#DD8452", edgecolor="black", linewidth=0.5)
            ax.set_xticks(x)
            ax.set_xticklabels(bin_labels, rotation=45, ha="right", fontsize=8)
            ax.set_ylabel("Frequency")
            ax.yaxis.set_major_locator(MaxNLocator(integer=True))
            ax.set_title(label, fontsize=9)
            sns.despine(ax=ax)

        if ax_cold is not None:
            if gt_cold or pred_cold:
                w = 0.38
                ax_cold.bar([-w/2], [gt_cold], width=w, color="#4C72B0",
                            edgecolor="black", linewidth=0.5)
                ax_cold.bar([+w/2], [pred_cold], width=w, color="#DD8452",
                            edgecolor="black", linewidth=0.5)
                ax_cold.set_xticks([0])
                ax_cold.set_xticklabels(["-1\n(cold)"], fontsize=8)
                ax_cold.set_xlabel("Reuse Distance")
                ax_cold.yaxis.set_major_locator(MaxNLocator(integer=True, nbins=2))
                sns.despine(ax=ax_cold)
                if all_rds:
                    _add_break_marks(ax, ax_cold)
            else:
                ax_cold.set_visible(False)
                ax.set_xlabel("Reuse Distance")
        else:
            ax.set_xlabel("Reuse Distance")

    handles, leg_labels = top_axes[0].get_legend_handles_labels()
    if handles:
        fig.legend(handles, leg_labels, loc="lower center", ncol=2,
                   fontsize=9, frameon=False, bbox_to_anchor=(0.5, 0))
    plt.tight_layout(rect=[0, 0.06, 1, 1])
    if save_path:
        fig.savefig(save_path, dpi=300, bbox_inches="tight", transparent=True)
        print(f"  플롯 저장 → {save_path}")


def aggregate_by_function(results: list) -> list:
    """블록별 결과를 함수명으로 묶어 ReuseProfile을 합산한다.

    @param results  (name, p1[, p2, ...]) 튜플 리스트.
                    name은 "func  ..." 형태여야 한다.
    @return         (func_name, merged_p1[, merged_p2, ...]) 리스트.
    """
    groups: dict = {}
    order: List[str] = []
    for entry in results:
        name, profiles = entry[0], entry[1:]
        func_name = name.split("  ")[0]
        if func_name not in groups:
            groups[func_name] = tuple(ReuseProfile() for _ in profiles)
            order.append(func_name)
        for merged, src in zip(groups[func_name], profiles):
            for rd, cnt in src.histogram.items():
                merged.histogram[rd] = merged.histogram.get(rd, 0) + cnt
            merged.cold_misses |= src.cold_misses
    return [(fn,) + groups[fn] for fn in order]
