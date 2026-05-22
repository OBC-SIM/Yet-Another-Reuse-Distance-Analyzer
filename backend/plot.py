"""
plot.py — 재사용 거리 히스토그램 시각화 유틸리티.
"""

import math
import os
from collections import defaultdict
from pathlib import Path
from typing import List, Tuple

from lru_sim import ReuseProfile


def _bin_histogram(hist: dict, cold_miss_count: int = 0) -> Tuple[List[str], List[int]]:
    """RD 히스토그램을 2의 제곱수 단위로 binning. cold miss는 RD=-1 bin으로 맨 앞에 추가."""
    binned: dict = defaultdict(int)
    for rd, count in hist.items():
        if rd <= 1:
            binned[rd] += count
        else:
            bin_idx = int(math.log2(rd))
            binned[2**bin_idx] += count

    labels, counts = [], []
    if cold_miss_count > 0:
        labels.append("-1\n(cold)")
        counts.append(cold_miss_count)
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


def plot_histograms(
    results: List[Tuple[str, ReuseProfile]],
    save_path: Path | None,
) -> None:
    """예측 RDH를 파일별로 subplot에 그린다."""
    os.environ.setdefault("MPLBACKEND", "Agg")
    import matplotlib.pyplot as plt
    import seaborn as sns
    from matplotlib.ticker import MaxNLocator

    _setup_theme()

    n = len(results)
    fig, axes = plt.subplots(1, n, figsize=(4.0 * n, 3.0), squeeze=False)

    for ax, (label, profile) in zip(axes[0], results):
        hist = profile.histogram
        if not hist:
            ax.text(0.5, 0.5, "No reuse", ha="center", va="center")
            ax.set_title(label)
            continue

        labels, counts = _bin_histogram(hist, len(profile.cold_misses))
        x_pos = range(len(labels))

        ax.bar(x_pos, counts, color="#4C72B0", edgecolor="black", linewidth=0.5, width=0.8)
        ax.set_xticks(x_pos)
        ax.set_xticklabels(labels, rotation=45, ha="right", fontsize=9)
        ax.set_xlabel("Reuse Distance")
        ax.set_ylabel("Frequency")
        ax.yaxis.set_major_locator(MaxNLocator(integer=True))
        ax.set_title(label, fontsize=11)
        sns.despine(ax=ax)

    plt.tight_layout()
    if save_path:
        fig.savefig(save_path, dpi=300, bbox_inches="tight", transparent=True)


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
    import numpy as np
    import seaborn as sns
    from matplotlib.ticker import MaxNLocator

    _setup_theme()

    n = len(results)
    fig, axes = plt.subplots(1, n, figsize=(4.5 * n, 3.2), squeeze=False)

    for ax, (label, gt, pred) in zip(axes[0], results):
        all_rds = sorted(set(gt.histogram) | set(pred.histogram))
        if not all_rds:
            ax.text(0.5, 0.5, "No reuse", ha="center", va="center")
            ax.set_title(label, fontsize=9)
            continue

        # 두 히스토그램을 같은 bin 기준으로 병합
        merged_gt: dict = defaultdict(int)
        merged_pred: dict = defaultdict(int)
        for rd in all_rds:
            if rd <= 1:
                key = rd
            else:
                key = 2 ** int(math.log2(rd))
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

        # cold miss를 RD=-1 bin으로 맨 앞에 추가
        gt_cold = len(gt.cold_misses)
        pred_cold = len(pred.cold_misses)
        if gt_cold or pred_cold:
            bin_labels = ["-1\n(cold)"] + bin_labels
            gt_vals = [gt_cold] + [merged_gt[k] for k in bin_keys]
            pred_vals = [pred_cold] + [merged_pred[k] for k in bin_keys]
        else:
            gt_vals = [merged_gt[k] for k in bin_keys]
            pred_vals = [merged_pred[k] for k in bin_keys]

        x = np.arange(len(bin_labels))
        w = 0.38

        ax.bar(x - w/2, gt_vals, width=w, label="Ground Truth",
               color="#4C72B0", edgecolor="black", linewidth=0.5)
        ax.bar(x + w/2, pred_vals, width=w, label="Predicted",
               color="#DD8452", edgecolor="black", linewidth=0.5)

        ax.set_xticks(x)
        ax.set_xticklabels(bin_labels, rotation=45, ha="right", fontsize=8)
        ax.set_xlabel("Reuse Distance")
        ax.set_ylabel("Frequency")
        ax.yaxis.set_major_locator(MaxNLocator(integer=True))
        ax.set_title(label, fontsize=9)
        ax.legend(fontsize=8)
        sns.despine(ax=ax)

    plt.tight_layout()
    if save_path:
        fig.savefig(save_path, dpi=300, bbox_inches="tight", transparent=True)
        print(f"  플롯 저장 → {save_path}")