from __future__ import annotations

import copy
import re
from typing import Dict, List


_AFFINE_NAME = re.compile(r"^([A-Za-z_][A-Za-z0-9_]*)([+-]\d+)?$")


def _substitute_name(name: str, mapping: Dict[str, str]) -> str:
    if name in mapping:
        return mapping[name]
    match = _AFFINE_NAME.match(name)
    if match and match.group(1) in mapping:
        return mapping[match.group(1)] + (match.group(2) or "")
    return name


def _substitute_node(node: dict, mapping: Dict[str, str]) -> dict:
    node = copy.deepcopy(node)
    if node["type"] == "Array":
        node["name"] = _substitute_name(node["name"], mapping)
        node["indices"] = [_substitute_name(index, mapping) for index in node["indices"]]
    elif node["type"] == "Scalar":
        node["name"] = _substitute_name(node["name"], mapping)
    elif node["type"] == "Loop":
        node["body"] = [_substitute_node(child, mapping) for child in node["body"]]
    elif node["type"] == "Call":
        node["args"] = [_substitute_name(arg, mapping) for arg in node.get("args", [])]
    return node


def _expand_body(
    body: List[dict],
    functions: Dict[str, dict],
    stack: tuple[str, ...],
) -> List[dict]:
    expanded: List[dict] = []
    for node in body:
        if node["type"] == "Call":
            callee = node["callee"]
            if callee not in functions:
                raise ValueError(f"Unknown call target: {callee}")
            if callee in stack:
                raise ValueError(f"Recursive call expansion is not supported: {callee}")
            func = functions[callee]
            mapping = dict(zip(func.get("params", []), node.get("args", [])))
            substituted = [_substitute_node(child, mapping) for child in func["body"]]
            expanded.extend(_expand_body(substituted, functions, (*stack, callee)))
        elif node["type"] == "Loop":
            clone = copy.deepcopy(node)
            clone["body"] = _expand_body(clone["body"], functions, stack)
            expanded.append(clone)
        else:
            expanded.append(copy.deepcopy(node))
    return expanded


def expand_calls(module: List[dict]) -> List[dict]:
    functions = {entry["function"]: entry for entry in module}
    expanded = []
    for entry in module:
        clone = copy.deepcopy(entry)
        clone["body"] = _expand_body(entry["body"], functions, (entry["function"],))
        expanded.append(clone)
    return expanded
