"""_plot_utils.py — 공유 시각화 인프라."""

import math
from collections import defaultdict
from pathlib import Path
from typing import List, Tuple

_SAVE_DPI = 600


def _bin_histogram(hist: dict) -> Tuple[List[str], List[int]]:
    """RD 히스토그램을 2의 제곱수 단위로 binning."""
    binned: dict = defaultdict(int)
    for rd, count in hist.items():
        if rd <= 1:
            binned[rd] += count
        else:
            binned[2 ** int(math.log2(rd))] += count
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


def _broken_axis_limits(values: List) -> Tuple[float, float] | None:
    """scale gap이 4배 이상일 때 broken y-axis 범위 (low_max, high_min)을 반환한다."""
    positive = sorted(v for v in values if v > 0)
    if len(positive) < 2:
        return None
    gap_idx, best_ratio = None, 1.0
    for idx, (lo, hi) in enumerate(zip(positive, positive[1:])):
        if lo == 0:
            continue
        r = hi / lo
        if r > best_ratio:
            best_ratio, gap_idx = r, idx
    if gap_idx is None or best_ratio < 4:
        return None
    low_max = positive[gap_idx] * 1.25
    high_min = positive[gap_idx + 1] * 0.85
    return (low_max, high_min) if high_min > low_max else None


def _bar_ylim(ax, values: List) -> None:
    ax.set_ylim(0, max(1, max(values, default=0) * 1.20))


def _time_ylim(ax, values: List) -> None:
    ax.set_ylim(0, max(1e-6, max(values, default=0.0) * 1.20))


def _plot_single_bars(ax, labels: List[str], counts: List):
    x_pos = range(len(labels))
    bars = ax.bar(x_pos, counts, color="#4C72B0", edgecolor="black", linewidth=0.5, width=0.8)
    ax.set_xticks(list(x_pos))
    ax.set_xticklabels(labels, rotation=45, ha="right", fontsize=9)
    return list(bars)


def _plot_grouped_bars(ax, labels: List[str], gt_counts: List, pred_counts: List):
    import numpy as np
    x = np.arange(len(labels))
    w = 0.38
    bars_gt = ax.bar(x - w/2, gt_counts, width=w, label="Ground Truth",
                     color="#4C72B0", edgecolor="black", linewidth=0.5)
    bars_pred = ax.bar(x + w/2, pred_counts, width=w, label="Predicted",
                       color="#DD8452", edgecolor="black", linewidth=0.5)
    ax.set_xticks(x)
    ax.set_xticklabels(labels, rotation=45, ha="right", fontsize=8)
    return list(bars_gt) + list(bars_pred)


def _setup_theme():
    import seaborn as sns
    import matplotlib.pyplot as plt
    sns.set_theme(style="ticks", context="paper")
    plt.rcParams.update({
        "font.family": "serif",
        "font.serif": ["DejaVu Serif", "Liberation Serif", "Times New Roman"],
        "pdf.fonttype": 42,
        "ps.fonttype": 42,
        "svg.fonttype": "none",
        "figure.facecolor": "white",
        "axes.facecolor": "white",
        "axes.linewidth": 1.0,
        "xtick.direction": "in",
        "ytick.direction": "in",
    })


def _add_break_marks(ax_top, ax_bot) -> None:
    """두 축 사이 spine을 열어 broken axis임을 나타낸다."""
    ax_top.spines["bottom"].set_visible(False)
    ax_bot.spines["top"].set_visible(False)
    ax_top.tick_params(bottom=False)


def _add_break_band(ax_top, ax_bot) -> None:
    """두 축 경계에 흰색 물결 리본을 그린다."""
    import numpy as np
    from matplotlib.lines import Line2D
    from matplotlib.patches import Polygon

    xmin, xmax = ax_bot.get_xlim()
    pad = (xmax - xmin) * 0.01
    xs = np.linspace(xmin - pad, xmax + pad, 500)
    phase = np.linspace(0, max(2.0, (xmax - xmin) / 1.8) * 2 * math.pi, xs.size)
    fig = ax_bot.figure
    inv = fig.transFigure.inverted()
    x_fig = inv.transform(
        ax_bot.transData.transform(np.column_stack([xs, np.zeros_like(xs)]))
    )[:, 0]

    y_mid = ax_bot.get_position().y1
    wave = 0.006 * np.sin(phase)
    upper = y_mid + wave + 0.010
    lower = y_mid + wave - 0.010

    verts = list(zip(x_fig, upper)) + list(zip(x_fig[::-1], lower[::-1]))
    fig.add_artist(Polygon(verts, closed=True, facecolor="white", edgecolor="none",
                           transform=fig.transFigure, zorder=1000, clip_on=False))
    for ys in (upper, lower):
        fig.add_artist(Line2D(x_fig, ys, color="#7F7F7F", linewidth=0.8,
                              transform=fig.transFigure, zorder=1001,
                              solid_capstyle="round", clip_on=False))


def _save_figure(fig, save_path: Path) -> None:
    fig.savefig(save_path, dpi=_SAVE_DPI, bbox_inches="tight",
                facecolor="white", transparent=False, pad_inches=0.04)
