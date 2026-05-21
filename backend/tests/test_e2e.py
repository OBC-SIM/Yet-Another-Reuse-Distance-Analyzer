import json
import pytest
from predictor import analyze
from lru_sim import ReuseProfile


LOOP_1D = [{"function": "test_1d", "body": [
    {"type": "Loop", "var": "i", "bound": 100, "depth": 1, "body": [
        {"type": "Array", "name": "arr", "indices": ["i"]}
    ]}
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