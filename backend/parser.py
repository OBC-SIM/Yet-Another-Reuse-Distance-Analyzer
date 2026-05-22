from abc import ABC, abstractmethod
from typing import Dict, List
import re


_AFFINE_INDEX = re.compile(r"^([A-Za-z_][A-Za-z0-9_]*)([+-]\d+)?$")


class TraceNode(ABC):
    @abstractmethod
    def unroll(self, env: Dict[str, int]) -> List[str]:
        pass


class ScalarNode(TraceNode):
    def __init__(self, name: str):
        self.name = name

    def unroll(self, env: Dict[str, int]) -> List[str]:
        return [self.name]


class ArrayNode(TraceNode):
    def __init__(self, name: str, indices: List[str]):
        self.name = name
        self.indices = indices

    def unroll(self, env: Dict[str, int]) -> List[str]:
        resolved = [str(resolve_index(idx, env)) for idx in self.indices]
        return [self.name + "-" + "-".join(resolved)]


class LoopBlockNode(TraceNode):
    def __init__(self, var: str, actual_bound: int, sim_bound: int,
                 depth: int, body: List[TraceNode]):
        self.var = var
        self.actual_bound = actual_bound
        self.sim_bound = sim_bound
        self.start = 0
        self.depth = depth
        self.body = body

    def unroll(self, env: Dict[str, int]) -> List[str]:
        result = []
        for i in range(self.start, self.start + self.sim_bound):
            child_env = {**env, self.var: i}
            for node in self.body:
                result.extend(node.unroll(child_env))
        return result


def resolve_index(index: str, env: Dict[str, int]) -> str:
    if index in env:
        return str(env[index])
    match = _AFFINE_INDEX.match(index)
    if match and match.group(1) in env:
        offset = int(match.group(2) or "0")
        return str(env[match.group(1)] + offset)
    return index


def index_variable(index: str) -> str | None:
    if _AFFINE_INDEX.match(index):
        return _AFFINE_INDEX.match(index).group(1)
    return None


def _parse_node(data: dict, sim_bound: int) -> TraceNode:
    t = data["type"]
    if t == "Scalar":
        return ScalarNode(data["name"])
    elif t == "Array":
        return ArrayNode(data["name"], data["indices"])
    elif t == "Loop":
        body = [_parse_node(child, sim_bound) for child in data["body"]]
        start = data.get("start", 0)
        actual_bound = max(0, data["bound"] - start)
        node = LoopBlockNode(data["var"], actual_bound, min(sim_bound, actual_bound),
                             data["depth"], body)
        node.start = start
        return node
    else:
        raise ValueError(f"Unknown node type: {t}")


def parse_trace(json_data: list, sim_bound: int = 2) -> List[TraceNode]:
    return [_parse_node(node, sim_bound) for node in json_data]
