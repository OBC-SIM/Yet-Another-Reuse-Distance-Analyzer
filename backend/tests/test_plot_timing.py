import pytest

from plot_timing import _timing_results_ms, aggregate_timing_as_program


def test_timing_results_ms_converts_seconds_to_milliseconds():
    results = [("loop", 0.125, 0.0025)]

    assert _timing_results_ms(results) == [("loop", 125.0, 2.5)]


def test_aggregate_timing_keeps_seconds():
    results = [("a", 0.1, 0.01), ("b", 0.2, 0.02)]

    label, unroll_time, pred_time = aggregate_timing_as_program(results)[0]

    assert label == "program"
    assert unroll_time == pytest.approx(0.3)
    assert pred_time == pytest.approx(0.03)
