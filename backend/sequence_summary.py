from dataclasses import dataclass
from math import prod
from typing import Dict, List

from parser import index_variable


@dataclass(frozen=True)
class AccessPattern:
    name: str
    shape: tuple[int, ...]
    starts: tuple[int, ...]

    @property
    def size(self) -> int:
        return prod(self.shape) if self.shape else 1


@dataclass(frozen=True)
class SequenceSummary:
    first_patterns: List[AccessPattern]
    final_patterns: List[AccessPattern]


def summarize_sequence(raw_node: dict) -> SequenceSummary | None:
    if raw_node["type"] == "Call":
        return None

    patterns: List[AccessPattern] = []

    def visit(node: dict, loop_ranges: Dict[str, tuple[int, int]]) -> bool:
        node_type = node["type"]
        if node_type == "Loop":
            start = node.get("start", 0)
            size = node["bound"] - start
            child_ranges = {**loop_ranges, node["var"]: (start, size)}
            for child in node["body"]:
                if not visit(child, child_ranges):
                    return False
            return True
        if node_type == "Array":
            shape: List[int] = []
            starts: List[int] = []
            for idx in node["indices"]:
                var = index_variable(idx)
                if var is None or var not in loop_ranges:
                    return False
                start, size = loop_ranges[var]
                starts.append(start)
                shape.append(size)
            patterns.append(AccessPattern(node["name"], tuple(shape), tuple(starts)))
            return True
        if node_type == "Scalar":
            patterns.append(AccessPattern(node["name"], (), ()))
            return True
        return False

    if not visit(raw_node, {}):
        return None

    first_patterns = list(dict.fromkeys(patterns))
    final_patterns = list(dict.fromkeys(reversed(patterns)))
    return SequenceSummary(first_patterns, final_patterns)
