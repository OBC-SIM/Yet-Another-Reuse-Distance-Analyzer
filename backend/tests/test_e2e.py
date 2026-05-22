import json
import pytest
from predictor import analyze
from predictor import _predict_loop_block
from lru_sim import ReuseProfile


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
