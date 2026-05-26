from abc import ABC, abstractmethod
from typing import Dict, List
import re


_AFFINE_INDEX = re.compile(r"^([A-Za-z_][A-Za-z0-9_]*)([+-]\d+)?$")


class TraceNode(ABC):
    @abstractmethod
    def unroll(self, env: Dict[str, int], granularity: str = "element",
               cache_line_size: int = 64) -> List[str]:
        pass


class ScalarNode(TraceNode):
    def __init__(self, name: str):
        self.name = name

    def unroll(self, env: Dict[str, int], granularity: str = "element",
               cache_line_size: int = 64) -> List[str]:
        return [self.name]


class ArrayNode(TraceNode):
    def __init__(self, name: str, indices: List[str],
                 shape: List[int] | None = None, elem_size: int | None = None):
        self.name = name
        self.indices = indices
        self.shape = shape
        self.elem_size = elem_size

    def unroll(self, env: Dict[str, int], granularity: str = "element",
               cache_line_size: int = 64) -> List[str]:
        resolved = [resolve_index(idx, env) for idx in self.indices]
        if granularity == "cache-line":
            line_key = self._cache_line_key(resolved, cache_line_size)
            if line_key:
                return [line_key]
        return [self.name + "-" + "-".join(str(idx) for idx in resolved)]

    def _cache_line_key(self, indices: List[str], cache_line_size: int) -> str | None:
        if self.elem_size is None:
            return None
        try:
            numeric = [int(index) for index in indices]
        except ValueError:
            return None
        if len(numeric) == 1:
            linear_index = numeric[0]
        elif self.shape and len(self.shape) in (len(numeric), len(numeric) - 1):
            shape = self.shape[-(len(numeric) - 1):]
            linear_index = 0
            for pos, index in enumerate(numeric):
                stride = 1
                for dim in shape[pos:]:
                    stride *= dim
                linear_index += index * stride
        else:
            return None
        line = (linear_index * self.elem_size) // cache_line_size
        return f"{self.name}-line-{line}"


class CallNode(TraceNode):
    def __init__(self, callee: str, args: List[str]):
        self.callee = callee
        self.args = args

    def unroll(self, env: Dict[str, int], granularity: str = "element",
               cache_line_size: int = 64) -> List[str]:
        raise RuntimeError("CallNode must be expanded before unroll")


class LoopBlockNode(TraceNode):
    def __init__(self, var: str, actual_bound: int, sim_bound: int,
                 depth: int, body: List[TraceNode]):
        self.var = var
        self.actual_bound = actual_bound
        self.sim_bound = sim_bound
        self.start = 0
        self.depth = depth
        self.body = body

    def unroll(self, env: Dict[str, int], granularity: str = "element",
               cache_line_size: int = 64) -> List[str]:
        result = []
        for i in range(self.start, self.start + self.sim_bound):
            child_env = {**env, self.var: i}
            for node in self.body:
                result.extend(node.unroll(child_env, granularity, cache_line_size))
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
        return ArrayNode(data["name"], data["indices"],
                         data.get("shape"), data.get("elem_size"))
    elif t == "Call":
        return CallNode(data["callee"], data.get("args", []))
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
