"""
plot_timing.py — GT unroll baseline vs. 예측 실행시간 비교 차트.
"""

import os
from pathlib import Path
from typing import List, Tuple

from _plot_utils import (
    _add_break_band, _add_break_marks, _broken_axis_limits,
    _save_figure, _setup_theme, _time_ylim,
)

_SECONDS_TO_MILLISECONDS = 1000.0


def _timing_results_ms(
    results: List[Tuple[str, float, float]],
) -> List[Tuple[str, float, float]]:
    return [
        (label, gt_time * _SECONDS_TO_MILLISECONDS, pred_time * _SECONDS_TO_MILLISECONDS)
        for label, gt_time, pred_time in results
    ]


def _plot_timing_bars(ax, gt_time: float, pred_time: float):
    w = 0.38
    bars = ax.bar(
        [-w / 2, w / 2], [gt_time, pred_time],
        color=["#4C72B0", "#DD8452"], edgecolor="black", linewidth=0.5, width=w,
        label=["Unrolling", "Predicted"],
    )
    ax.set_xticks([0])
    ax.set_xticklabels(["Runtime"], fontsize=8)
    return list(bars)


def _add_timing_reduction_label(ax, unroll_time: float, pred_time: float) -> None:
    if unroll_time <= 0:
        return
    reduction = (unroll_time - pred_time) / unroll_time * 100
    text = f"{reduction:.1f}% reduction" if reduction >= 0 else f"{abs(reduction):.1f}% slower"
    ax.text(0.98, 0.92, text, transform=ax.transAxes, ha="right", va="top", fontsize=9)


def plot_timing_comparison(
    results: List[Tuple[str, float, float]],
    save_path: Path | None,
) -> None:
    """GT unroll baseline과 prediction 시간을 블록별로 비교한다."""
    os.environ.setdefault("MPLBACKEND", "Agg")
    import matplotlib.pyplot as plt
    import seaborn as sns
    from matplotlib.ticker import MaxNLocator

    _setup_theme()
    n = len(results)
    series = []
    has_break = False
    for label, gt_time, pred_time in _timing_results_ms(results):
        limits = _broken_axis_limits([gt_time, pred_time])
        has_break = has_break or limits is not None
        series.append((label, gt_time, pred_time, limits))

    if has_break:
        fig = plt.figure(figsize=(3.2 * n, 4.2))
        gs = fig.add_gridspec(2, n, height_ratios=[3, 1], hspace=0.0)
        top_axes = [fig.add_subplot(gs[0, i]) for i in range(n)]
        bot_axes = [fig.add_subplot(gs[1, i]) for i in range(n)]
    else:
        fig = plt.figure(figsize=(3.2 * n, 3.2))
        gs = fig.add_gridspec(1, n)
        top_axes = [fig.add_subplot(gs[0, i]) for i in range(n)]
        bot_axes = [None] * n

    break_axes = []
    for i, (label, gt_time, pred_time, limits) in enumerate(series):
        ax, ax_bot = top_axes[i], bot_axes[i]
        values = [gt_time, pred_time]

        _plot_timing_bars(ax, gt_time, pred_time)
        ax.set_ylabel("Time (ms)", labelpad=12)
        ax.yaxis.set_major_locator(MaxNLocator(nbins=5))
        ax.set_title(label, fontsize=13, pad=14)
        _add_timing_reduction_label(ax, gt_time, pred_time)

        if ax_bot is not None and limits is not None:
            low_max, high_min = limits
            _plot_timing_bars(ax_bot, gt_time, pred_time)
            ax.set_ylim(high_min, max(values) * 1.18)
            ax_bot.set_ylim(0, low_max)
            ax.tick_params(labelbottom=False, bottom=False)
            ax.yaxis.set_major_locator(MaxNLocator(nbins=5, prune="lower"))
            ax_bot.yaxis.set_major_locator(MaxNLocator(nbins=2, prune="upper"))
            ax_bot.set_xlabel("Time Comparison")
            _add_break_marks(ax, ax_bot)
            break_axes.append((ax, ax_bot))
            sns.despine(ax=ax)
            sns.despine(ax=ax_bot)
        else:
            _time_ylim(ax, values)
            ax.set_xlabel("Time Comparison")
            sns.despine(ax=ax)
            if ax_bot is not None:
                ax_bot.set_visible(False)

    handles, leg_labels = top_axes[0].get_legend_handles_labels()
    fig.subplots_adjust(left=0.14 if n == 1 else 0.08, right=0.98,
                        bottom=0.30 if handles else 0.26, top=0.84, hspace=0.0)
    if handles:
        axes_left = min(ax.get_position().x0 for ax in top_axes)
        axes_right = max(ax.get_position().x1 for ax in top_axes)
        fig.legend(handles, leg_labels, loc="lower center", ncol=2,
                   fontsize=9, frameon=False,
                   bbox_to_anchor=((axes_left + axes_right) / 2, 0.035))
    for ax, ax_bot in break_axes:
        _add_break_band(ax, ax_bot)
    if save_path:
        _save_figure(fig, save_path)
        print(f"  플롯 저장 → {save_path}")


def aggregate_timing_as_program(
    results: List[Tuple[str, float, float]],
    label: str = "program",
) -> List[Tuple[str, float, float]]:
    """블록별 timing을 program-level로 합산한다."""
    if not results:
        return []
    return [(label, sum(r[1] for r in results), sum(r[2] for r in results))]
