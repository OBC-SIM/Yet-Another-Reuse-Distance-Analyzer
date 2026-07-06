import json
from pathlib import Path
import pytest
from parser import (
    ScalarNode,
    ArrayNode,
    CallNode,
    LoopBlockNode,
    loop_iteration_count,
    parse_trace,
)

TASKS_DIR = Path(__file__).resolve().parent.parent.parent / "tasks"


class TestScalarNode:
    def test_unroll_returns_name(self):
        assert ScalarNode("x").unroll({}) == ["x"]

    def test_unroll_ignores_env(self):
        assert ScalarNode("x").unroll({"x": 42}) == ["x"]


class TestArrayNode:
    def test_unroll_substitutes_indices(self):
        assert ArrayNode("A", ["i", "j"]).unroll({"i": 1, "j": 2}) == ["A-1-2"]

    def test_unroll_constant_index(self):
        assert ArrayNode("arr", ["0"]).unroll({}) == ["arr-0"]

    def test_unroll_mixed_constant_and_var(self):
        assert ArrayNode("A", ["i", "0"]).unroll({"i": 3}) == ["A-3-0"]

    def test_unroll_affine_indices(self):
        assert ArrayNode("A", ["i-1", "i", "i+1"]).unroll({"i": 3}) == ["A-2-3-4"]

    def test_unroll_cache_line_1d(self):
        node = ArrayNode("A", ["i"], elem_size=8)
        assert node.unroll({"i": 7}, "cache-line", 64) == ["A-line-0"]
        assert node.unroll({"i": 8}, "cache-line", 64) == ["A-line-1"]

    def test_unroll_cache_line_default_is_32_bytes(self):
        node = ArrayNode("A", ["i"], elem_size=8)
        assert node.unroll({"i": 3}, "cache-line") == ["A-line-0"]
        assert node.unroll({"i": 4}, "cache-line") == ["A-line-1"]

    def test_unroll_cache_line_2d_with_shape(self):
        node = ArrayNode("A", ["i", "j"], shape=[4, 16], elem_size=8)
        assert node.unroll({"i": 0, "j": 7}, "cache-line", 64) == ["A-line-0"]
        assert node.unroll({"i": 0, "j": 8}, "cache-line", 64) == ["A-line-1"]
        assert node.unroll({"i": 1, "j": 0}, "cache-line", 64) == ["A-line-2"]

    def test_unroll_cache_line_2d_with_trailing_shape(self):
        node = ArrayNode("A", ["i", "j"], shape=[16], elem_size=8)
        assert node.unroll({"i": 1, "j": 0}, "cache-line", 64) == ["A-line-2"]

    def test_cache_line_falls_back_without_shape(self):
        node = ArrayNode("A", ["i", "j"])
        assert node.unroll({"i": 0, "j": 1}, "cache-line", 64) == ["A-0-1"]


class TestCallNode:
    def test_unroll_requires_expansion(self):
        with pytest.raises(RuntimeError):
            CallNode("helper", ["A", "i"]).unroll({})


class TestLoopBlockNode:
    def test_unroll_iterates_sim_bound(self):
        loop = LoopBlockNode("i", actual_bound=100, sim_bound=2, depth=1,
                             body=[ArrayNode("A", ["i"])])
        assert loop.unroll({}) == ["A-0", "A-1"]

    def test_unroll_respects_start(self):
        loop = LoopBlockNode("i", actual_bound=98, sim_bound=2, depth=1,
                             body=[ArrayNode("A", ["i-1", "i+1"])])
        loop.start = 1
        assert loop.unroll({}) == ["A-0-2", "A-1-3"]

    def test_unroll_respects_step(self):
        loop = LoopBlockNode("i", actual_bound=4, sim_bound=3, depth=1,
                             body=[ArrayNode("A", ["i"])], step=32)
        assert loop.unroll({}) == ["A-0", "A-32", "A-64"]

    def test_unroll_respects_start_and_step(self):
        loop = LoopBlockNode("i", actual_bound=4, sim_bound=2, depth=1,
                             body=[ArrayNode("A", ["i"])], step=32)
        loop.start = 16
        assert loop.unroll({}) == ["A-16", "A-48"]

    def test_unroll_nested_loops(self):
        inner = LoopBlockNode("j", actual_bound=100, sim_bound=2, depth=2,
                              body=[ArrayNode("A", ["i", "j"])])
        outer = LoopBlockNode("i", actual_bound=100, sim_bound=2, depth=1,
                              body=[inner])
        assert outer.unroll({}) == ["A-0-0", "A-0-1", "A-1-0", "A-1-1"]

    def test_unroll_multiple_body_nodes(self):
        loop = LoopBlockNode("i", actual_bound=10, sim_bound=2, depth=1,
                             body=[ArrayNode("A", ["i"]), ArrayNode("B", ["i"])])
        assert loop.unroll({}) == ["A-0", "B-0", "A-1", "B-1"]


class TestParseTrace:
    def test_parse_scalar_node(self):
        data = [{"type": "Scalar", "name": "x"}]
        nodes = parse_trace(data)
        assert len(nodes) == 1
        assert isinstance(nodes[0], ScalarNode)
        assert nodes[0].name == "x"

    def test_parse_array_node(self):
        data = [{"type": "Array", "name": "A", "indices": ["i", "j"]}]
        nodes = parse_trace(data)
        assert len(nodes) == 1
        assert isinstance(nodes[0], ArrayNode)
        assert nodes[0].indices == ["i", "j"]

    def test_parse_array_shape_metadata(self):
        data = [{"type": "Array", "name": "A", "indices": ["i", "j"],
                 "shape": [4, 16], "elem_size": 8}]
        nodes = parse_trace(data)
        assert nodes[0].shape == [4, 16]
        assert nodes[0].elem_size == 8

    def test_parse_array_object_id(self):
        data = [{"type": "Array", "name": "A", "object": "global::A",
                 "indices": ["i"]}]
        nodes = parse_trace(data)
        assert nodes[0].object_id == "global::A"

    def test_parse_array_access_path(self):
        path = [
            {"kind": "field", "name": "items", "index": 1},
            {"kind": "index", "value": "i"},
            {"kind": "field", "name": "x", "index": 0},
        ]
        data = [{"type": "Array", "name": "o.items[i].x", "object": "global::o",
                 "indices": ["i"], "access_path": path}]
        nodes = parse_trace(data)
        assert nodes[0].access_path == path

    def test_parse_v2_metadata_fills_array_shape(self):
        data = {
            "schema_version": 2,
            "metadata": {
                "objects": {
                    "global::A": {
                        "shape": [4, 16],
                        "elem_size": 8,
                    }
                }
            },
            "functions": [{
                "function": "kernel",
                "body": [{"type": "Array", "name": "A", "object": "global::A",
                          "indices": ["i", "j"]}],
            }],
        }
        nodes = parse_trace(data)
        assert nodes[0].shape == [4, 16]
        assert nodes[0].elem_size == 8
        assert nodes[0].object_id == "global::A"

    def test_parse_loop_node(self):
        data = [{"type": "Loop", "var": "i", "bound": 32, "depth": 1,
                 "body": [{"type": "Array", "name": "A", "indices": ["i"]}]}]
        nodes = parse_trace(data, sim_bound=2)
        assert isinstance(nodes[0], LoopBlockNode)
        assert nodes[0].actual_bound == 32
        assert nodes[0].sim_bound == 2
        assert nodes[0].var == "i"

    def test_parse_call_node(self):
        data = [{"type": "Call", "callee": "helper", "args": ["A", "i"]}]
        nodes = parse_trace(data)
        assert len(nodes) == 1
        assert isinstance(nodes[0], CallNode)
        assert nodes[0].callee == "helper"
        assert nodes[0].args == ["A", "i"]

    def test_parse_loop_start_field(self):
        data = [{"type": "Loop", "var": "i", "start": 1, "bound": 99, "depth": 1,
                 "body": [{"type": "Array", "name": "A", "indices": ["i-1"]}]}]
        nodes = parse_trace(data, sim_bound=2)
        assert nodes[0].actual_bound == 98
        assert nodes[0].start == 1
        assert nodes[0].unroll({}) == ["A-0", "A-1"]

    def test_parse_loop_step_field(self):
        data = [{"type": "Loop", "var": "i", "start": 0, "bound": 96, "step": 32,
                 "depth": 1,
                 "body": [{"type": "Array", "name": "A", "indices": ["i"]}]}]
        nodes = parse_trace(data, sim_bound=4)
        assert nodes[0].actual_bound == 3
        assert nodes[0].step == 32
        assert nodes[0].unroll({}) == ["A-0", "A-32", "A-64"]

    def test_parse_loop_step_field_rounds_up_trip_count(self):
        data = [{"type": "Loop", "var": "i", "start": 0, "bound": 100, "step": 32,
                 "depth": 1,
                 "body": [{"type": "Array", "name": "A", "indices": ["i"]}]}]
        nodes = parse_trace(data, sim_bound=4)
        assert nodes[0].actual_bound == 4
        assert nodes[0].unroll({}) == ["A-0", "A-32", "A-64", "A-96"]

    def test_parse_loop_body_is_parsed_recursively(self):
        data = [{"type": "Loop", "var": "i", "bound": 10, "depth": 1,
                 "body": [{"type": "Array", "name": "A", "indices": ["i"]}]}]
        nodes = parse_trace(data)
        assert isinstance(nodes[0].body[0], ArrayNode)

    def test_parse_unknown_type_raises(self):
        data = [{"type": "Unknown", "name": "x"}]
        with pytest.raises(ValueError):
            parse_trace(data)

    def test_parse_matmul_json(self):
        data = [{"function": "matmul", "body": [
            {"type": "Loop", "var": "i", "bound": 32, "depth": 1, "body": [
                {"type": "Loop", "var": "j", "bound": 32, "depth": 2, "body": [
                    {"type": "Loop", "var": "k", "bound": 64, "depth": 3, "body": [
                        {"type": "Array", "name": "A", "indices": ["i", "k"]},
                    ]}
                ]}
            ]}
        ]}]
        nodes = parse_trace(data[0]["body"])
        assert len(nodes) == 1
        assert isinstance(nodes[0], LoopBlockNode)
        assert nodes[0].var == "i"
        assert nodes[0].actual_bound == 32


class TestLoopIterationCount:
    def test_positive_step(self):
        assert loop_iteration_count(0, 96, 32) == 3

    def test_positive_step_rounds_up(self):
        assert loop_iteration_count(0, 100, 32) == 4

    def test_empty_positive_range(self):
        assert loop_iteration_count(100, 100, 32) == 0

    def test_negative_step(self):
        assert loop_iteration_count(96, 0, -32) == 3
