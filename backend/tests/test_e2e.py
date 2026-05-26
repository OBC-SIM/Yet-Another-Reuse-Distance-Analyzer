import json
import pytest
from pathlib import Path
from main import _unroll_file
from predictor import analyze
from predictor import _predict_loop_block, analyze_blocks
from verify import verify_json
from lru_sim import LRUProfiler, ReuseProfile


LOOP_1D = [{"function": "test_1d", "body": [
    {"type": "Loop", "var": "i", "bound": 100, "depth": 1, "body": [
        {"type": "Array", "name": "arr", "indices": ["i"]}
    ]}
]}]

REPEATED_1D = [{"function": "repeated_1d", "body": [
    {"type": "Loop", "var": "i", "bound": 64, "depth": 1, "body": [
        {"type": "Array", "name": "arr", "indices": ["i"]},
        {"type": "Array", "name": "arr", "indices": ["i"]},
    ]}
]}]

CACHE_LINE_1D = [{"function": "cache_line_1d", "body": [
    {"type": "Loop", "var": "i", "bound": 16, "depth": 1, "body": [
        {"type": "Array", "name": "A", "indices": ["i"], "shape": [16], "elem_size": 8}
    ]}
]}]

STENCIL_LIKE_1D = [{"function": "stencil_like_1d", "body": [
    {"type": "Loop", "var": "i", "bound": 99, "depth": 1, "body": [
        {"type": "Array", "name": "in", "indices": ["i"]},
        {"type": "Array", "name": "in", "indices": ["i"]},
        {"type": "Array", "name": "in", "indices": ["i"]},
        {"type": "Array", "name": "out", "indices": ["i"]},
    ]}
]}]

REGULAR_BLOCK = [{"function": "regular_block", "body": [
    {"type": "Array", "name": "A", "indices": ["0"]},
    {"type": "Loop", "var": "i", "bound": 100, "depth": 1, "body": [
        {"type": "Array", "name": "A", "indices": ["i"]},
        {"type": "Array", "name": "A", "indices": ["i"]},
    ]},
    {"type": "Array", "name": "A", "indices": ["0"]},
    {"type": "Array", "name": "A", "indices": ["0"]},
]}]

ORDERED_MIXED_BLOCKS = [{"function": "ordered", "body": [
    {"type": "Array", "name": "A", "indices": ["0"]},
    {"type": "Array", "name": "B", "indices": ["0"]},
    {"type": "Loop", "var": "i", "bound": 3, "depth": 1, "body": [
        {"type": "Array", "name": "A", "indices": ["i"]},
        {"type": "Array", "name": "A", "indices": ["i"]},
        {"type": "Array", "name": "B", "indices": ["i"]},
    ]},
    {"type": "Array", "name": "A", "indices": ["2"]},
    {"type": "Loop", "var": "j", "bound": 2, "depth": 1, "body": [
        {"type": "Loop", "var": "k", "bound": 3, "depth": 2, "body": [
            {"type": "Array", "name": "C", "indices": ["j", "k"]},
            {"type": "Array", "name": "C", "indices": ["j", "k"]},
            {"type": "Array", "name": "D", "indices": ["k", "j"]},
        ]},
    ]},
    {"type": "Array", "name": "C", "indices": ["1", "2"]},
    {"type": "Loop", "var": "x", "bound": 2, "depth": 1, "body": [
        {"type": "Loop", "var": "y", "bound": 2, "depth": 2, "body": [
            {"type": "Loop", "var": "z", "bound": 2, "depth": 3, "body": [
                {"type": "Array", "name": "E", "indices": ["x", "y", "z"]},
                {"type": "Array", "name": "F", "indices": ["z", "y"]},
                {"type": "Array", "name": "E", "indices": ["x", "y", "z"]},
            ]},
        ]},
    ]},
    {"type": "Array", "name": "E", "indices": ["1", "1", "1"]},
    {"type": "Array", "name": "B", "indices": ["0"]},
]}]

ORDERED_MIXED_NAMES = [
    "ordered  (flat, 2 accesses)",
    "ordered  i-loop (bound=3)",
    "ordered  (flat, 1 accesses)",
    "ordered  j-loop (bound=2)",
    "ordered  (flat, 1 accesses)",
    "ordered  x-loop (bound=2)",
    "ordered  (flat, 2 accesses)",
]

ORDERED_MIXED_TRACE = [
    "A-0", "B-0",
    "A-0", "A-0", "B-0",
    "A-1", "A-1", "B-1",
    "A-2", "A-2", "B-2",
    "A-2",
    "C-0-0", "C-0-0", "D-0-0",
    "C-0-1", "C-0-1", "D-1-0",
    "C-0-2", "C-0-2", "D-2-0",
    "C-1-0", "C-1-0", "D-0-1",
    "C-1-1", "C-1-1", "D-1-1",
    "C-1-2", "C-1-2", "D-2-1",
    "C-1-2",
    "E-0-0-0", "F-0-0", "E-0-0-0",
    "E-0-0-1", "F-1-0", "E-0-0-1",
    "E-0-1-0", "F-0-1", "E-0-1-0",
    "E-0-1-1", "F-1-1", "E-0-1-1",
    "E-1-0-0", "F-0-0", "E-1-0-0",
    "E-1-0-1", "F-1-0", "E-1-0-1",
    "E-1-1-0", "F-0-1", "E-1-1-0",
    "E-1-1-1", "F-1-1", "E-1-1-1",
    "E-1-1-1", "B-0",
]

LOOP_2D = [{"function": "test_2d", "body": [
    {"type": "Loop", "var": "j", "bound": 8, "depth": 1, "body": [
        {"type": "Loop", "var": "k", "bound": 8, "depth": 2, "body": [
            {"type": "Array", "name": "A", "indices": ["j", "k"]}
        ]}
    ]}
]}]

MATMUL_3D = [{"function": "matmul", "body": [
    {"type": "Loop", "var": "i", "bound": 32, "depth": 1, "body": [
        {"type": "Loop", "var": "j", "bound": 32, "depth": 2, "body": [
            {"type": "Loop", "var": "k", "bound": 64, "depth": 3, "body": [
                {"type": "Array", "name": "A", "indices": ["i", "k"]},
                {"type": "Array", "name": "B", "indices": ["k", "j"]},
                {"type": "Array", "name": "C", "indices": ["i", "j"]},
                {"type": "Array", "name": "C", "indices": ["i", "j"]},
            ]}
        ]}
    ]}
]}]


def write_json(tmp_path, data, name="trace.json"):
    path = tmp_path / name
    path.write_text(json.dumps(data))
    return str(path)


class TestAnalyze1D:
    def test_returns_reuse_profile(self, tmp_path):
        path = write_json(tmp_path, LOOP_1D)
        assert isinstance(analyze(path), ReuseProfile)

    def test_unique_accesses_histogram_empty(self, tmp_path):
        path = write_json(tmp_path, LOOP_1D)
        assert analyze(path).histogram == {}

    def test_repeated_access_scales_to_actual_bound(self):
        profile, _ = _predict_loop_block(REPEATED_1D[0]["body"][0])
        assert profile.histogram == {0: 64}

    def test_multiple_reuses_scale_to_actual_bound(self):
        profile, _ = _predict_loop_block(STENCIL_LIKE_1D[0]["body"][0])
        assert profile.histogram == {0: 198}

    def test_function_level_cross_block_reuse(self, tmp_path):
        path = write_json(tmp_path, REGULAR_BLOCK)
        profile = analyze(path)
        assert profile.histogram == {0: 102, 99: 1}
        assert len(profile.cold_misses) == 100

    def test_analyze_blocks_preserves_body_order(self, tmp_path):
        path = write_json(tmp_path, ORDERED_MIXED_BLOCKS)

        names = [name for name, _ in analyze_blocks(path)]

        assert names == ORDERED_MIXED_NAMES

    def test_unroll_mode_uses_ordered_full_trace_for_program_profile(self, tmp_path):
        path = Path(write_json(tmp_path, ORDERED_MIXED_BLOCKS))

        profile, blocks = _unroll_file(path)
        expected = LRUProfiler.calculate(ORDERED_MIXED_TRACE)

        assert [name for name, _ in blocks] == ORDERED_MIXED_NAMES
        assert profile.histogram == expected.histogram
        assert profile.cold_misses == expected.cold_misses

    def test_unroll_mode_can_use_cache_line_granularity(self, tmp_path):
        path = Path(write_json(tmp_path, CACHE_LINE_1D))

        profile, _ = _unroll_file(path, granularity="cache-line", cache_line_size=64)

        assert profile.histogram == {0: 14}
        assert profile.cold_misses == {"A-line-0", "A-line-1"}

    def test_verify_json_preserves_body_order(self, tmp_path, capsys):
        path = Path(write_json(tmp_path, ORDERED_MIXED_BLOCKS))

        results, timings, function_results, function_timings = verify_json(path)

        assert [name for name, _, _ in results] == ORDERED_MIXED_NAMES
        assert [name for name, _, _ in timings] == ORDERED_MIXED_NAMES
        assert [name for name, _, _ in function_results] == ["ordered  (function)"]
        assert [name for name, _, _ in function_timings] == ["ordered  (function)"]
        expected = LRUProfiler.calculate(ORDERED_MIXED_TRACE)
        assert function_results[0][1].histogram == expected.histogram
        assert function_results[0][1].cold_misses == expected.cold_misses
        capsys.readouterr()


class TestAnalyze2D:
    def test_returns_reuse_profile(self, tmp_path):
        path = write_json(tmp_path, LOOP_2D)
        assert isinstance(analyze(path), ReuseProfile)

    def test_no_reuse_histogram_empty(self, tmp_path):
        path = write_json(tmp_path, LOOP_2D)
        assert analyze(path).histogram == {}


class TestAnalyze3DMatmul:
    def test_returns_reuse_profile(self, tmp_path):
        path = write_json(tmp_path, MATMUL_3D)
        assert isinstance(analyze(path), ReuseProfile)

    def test_histogram_nonempty(self, tmp_path):
        path = write_json(tmp_path, MATMUL_3D)
        assert analyze(path).histogram != {}


class TestAnalyzeScalarTopLevel:
    def test_scalar_recorded_as_cold_miss(self, tmp_path):
        data = [{"function": "test_scalar", "body": [{"type": "Scalar", "name": "n"}]}]
        path = write_json(tmp_path, data)
        assert "n" in analyze(path).cold_misses
