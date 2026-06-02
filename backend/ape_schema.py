from __future__ import annotations

import copy
from typing import Any, Dict, List, Tuple


def normalize_module(raw: Any) -> List[dict]:
    functions, objects = _split_root(raw)
    if functions is None:
        raise ValueError("APE module root must contain 'functions'")
    return [_enrich_function(entry, objects) for entry in functions]


def normalize_trace(raw: Any) -> List[dict]:
    if isinstance(raw, dict) and "type" in raw:
        return [_enrich_node(copy.deepcopy(raw), {})]
    if isinstance(raw, list) and (not raw or "type" in raw[0]):
        return [_enrich_node(copy.deepcopy(node), {}) for node in raw]
    functions, objects = _split_root(raw)
    if functions is not None:
        nodes: List[dict] = []
        for entry in functions:
            nodes.extend(copy.deepcopy(entry.get("body", [])))
        return [_enrich_node(node, objects) for node in nodes]
    return []


def _split_root(raw: Any) -> Tuple[List[dict] | None, Dict[str, dict]]:
    if isinstance(raw, list):
        return raw, {}
    if not isinstance(raw, dict):
        raise ValueError("APE root must be a function list or object")

    objects = raw.get("metadata", {}).get("objects", {})
    if not isinstance(objects, dict):
        objects = {}

    if "functions" in raw:
        functions = raw["functions"]
        if not isinstance(functions, list):
            raise ValueError("APE root field 'functions' must be a list")
        return functions, objects

    if "type" in raw:
        return None, objects

    raise ValueError("APE root object must contain 'functions'")


def _enrich_function(entry: dict, objects: Dict[str, dict]) -> dict:
    clone = copy.deepcopy(entry)
    clone["body"] = [_enrich_node(node, objects) for node in clone.get("body", [])]
    return clone


def _enrich_node(node: dict, objects: Dict[str, dict]) -> dict:
    if node.get("type") == "Array":
        object_id = node.get("object")
        meta = objects.get(object_id, {}) if object_id else {}
        if "shape" not in node and "shape" in meta:
            node["shape"] = copy.deepcopy(meta["shape"])
        if "elem_size" not in node and "elem_size" in meta:
            node["elem_size"] = meta["elem_size"]
    elif node.get("type") == "Loop":
        node["body"] = [_enrich_node(child, objects) for child in node.get("body", [])]
    return node
