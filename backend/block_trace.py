from typing import List, Tuple

from calls import expand_calls
from lru_sim import LRUProfiler, ReuseProfile
from parser import LoopBlockNode, parse_trace


def unroll_node_actual(raw_node: dict, granularity: str = "element",
                       cache_line_size: int = 64) -> List[str]:
    node = parse_trace([raw_node], sim_bound=2)[0]

    def apply_actual(loop: LoopBlockNode) -> None:
        loop.sim_bound = loop.actual_bound
        for child in loop.body:
            if isinstance(child, LoopBlockNode):
                apply_actual(child)

    if isinstance(node, LoopBlockNode):
        apply_actual(node)
    return node.unroll({}, granularity, cache_line_size)


def function_trace(func_entry: dict, granularity: str = "element",
                   cache_line_size: int = 64) -> List[str]:
    trace: List[str] = []
    for node in func_entry["body"]:
        trace.extend(unroll_node_actual(node, granularity, cache_line_size))
    return trace


def block_trace_results(raw: list, granularity: str = "element",
                        cache_line_size: int = 64) -> List[Tuple[str, ReuseProfile, List[str]]]:
    expanded = expand_calls(raw)
    results: List[Tuple[str, ReuseProfile, List[str]]] = []

    def flush_flat(func_name: str, trace: List[str]) -> None:
        if trace:
            profile = LRUProfiler.calculate(trace)
            name = f"{func_name}  (flat, {len(trace)} accesses)"
            results.append((name, profile, list(trace)))
            trace.clear()

    for func_entry in expanded:
        func_name = func_entry["function"]
        flat_trace: List[str] = []
        for node in func_entry["body"]:
            if node["type"] == "Loop":
                flush_flat(func_name, flat_trace)
                trace = unroll_node_actual(node, granularity, cache_line_size)
                profile = LRUProfiler.calculate(trace)
                name = f"{func_name}  {node['var']}-loop (bound={node['bound']})"
                results.append((name, profile, trace))
            else:
                flat_trace.extend(unroll_node_actual(node, granularity, cache_line_size))
        flush_flat(func_name, flat_trace)
    return results
