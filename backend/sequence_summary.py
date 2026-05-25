from dataclasses import dataclass
from math import prod
from typing import List

from parser import index_variable


@dataclass(frozen=True)
class DenseSequence:
    name: str
    indices: tuple[str, ...]
    loop_vars: tuple[str, ...]
    starts: tuple[int, ...]
    bounds: tuple[int, ...]

    @property
    def size(self) -> int:
        return prod(self.bounds)

    def same_order(self, other: "DenseSequence") -> bool:
        return self == other


def dense_sequence(raw_node: dict) -> DenseSequence | None:
    loops: List[dict] = []
    node = raw_node
    while node["type"] == "Loop":
        loops.append(node)
        if len(node["body"]) != 1:
            return None
        node = node["body"][0]

    if node["type"] != "Array":
        return None
    if any(loop["bound"] <= loop.get("start", 0) for loop in loops):
        return None

    loop_vars = tuple(loop["var"] for loop in loops)
    index_vars = tuple(index_variable(idx) for idx in node["indices"])
    if index_vars != loop_vars:
        return None

    starts = tuple(loop.get("start", 0) for loop in loops)
    bounds = tuple(loop["bound"] - loop.get("start", 0) for loop in loops)
    return DenseSequence(
        name=node["name"],
        indices=tuple(node["indices"]),
        loop_vars=loop_vars,
        starts=starts,
        bounds=bounds,
    )
