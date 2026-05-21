import json
import pytest
from parser import ScalarNode, ArrayNode, LoopBlockNode, parse_trace


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


class TestLoopBlockNode:
    def test_unroll_iterates_sim_bound(self):
        loop = LoopBlockNode("i", actual_bound=100, sim_bound=2, depth=1,
                             body=[ArrayNode("A", ["i"])])
        assert loop.unroll({}) == ["A-0", "A-1"]

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

    def test_parse_loop_node(self):
        data = [{"type": "Loop", "var": "i", "bound": 32, "depth": 1,
                 "body": [{"type": "Array", "name": "A", "indices": ["i"]}]}]
        nodes = parse_trace(data, sim_bound=2)
        assert isinstance(nodes[0], LoopBlockNode)
        assert nodes[0].actual_bound == 32
        assert nodes[0].sim_bound == 2
        assert nodes[0].var == "i"

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
        with open("tasks/matmul_loop_annotated_trace.json") as f:
            data = json.load(f)
        nodes = parse_trace(data)
        assert len(nodes) == 1
        assert isinstance(nodes[0], LoopBlockNode)
        assert nodes[0].var == "i"
        assert nodes[0].actual_bound == 32