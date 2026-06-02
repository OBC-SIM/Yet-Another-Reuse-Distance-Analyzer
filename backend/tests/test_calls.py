import pytest

from calls import expand_calls


def test_expand_calls_substitutes_args_in_place():
    raw = [
        {
            "function": "helper",
            "params": ["x", "idx"],
            "annotations": ["yard.inline"],
            "body": [
                {"type": "Array", "name": "x", "indices": ["idx"]},
                {"type": "Array", "name": "x", "indices": ["idx+1"]},
            ],
        },
        {
            "function": "kernel",
            "params": ["a"],
            "annotations": ["yard.analyze"],
            "body": [
                {"type": "Loop", "var": "i", "start": 0, "bound": 4, "depth": 1, "body": [
                    {"type": "Array", "name": "a", "indices": ["i"]},
                    {"type": "Call", "callee": "helper", "args": ["a", "i"]},
                ]},
            ],
        },
    ]

    expanded = expand_calls(raw)
    assert [entry["function"] for entry in expanded] == ["kernel"]
    body = expanded[0]["body"][0]["body"]

    assert body == [
        {"type": "Array", "name": "a", "indices": ["i"]},
        {"type": "Array", "name": "a", "indices": ["i"]},
        {"type": "Array", "name": "a", "indices": ["i+1"]},
    ]


def test_expand_calls_rejects_recursion():
    raw = [{
        "function": "self",
        "params": [],
        "body": [{"type": "Call", "callee": "self", "args": []}],
    }]

    with pytest.raises(ValueError):
        expand_calls(raw)


def test_expand_calls_keeps_legacy_json_without_annotations():
    raw = [
        {"function": "helper", "params": [], "body": []},
        {"function": "kernel", "params": [], "body": []},
    ]

    assert [entry["function"] for entry in expand_calls(raw)] == ["helper", "kernel"]


def test_expand_calls_accepts_v2_root_object():
    raw = {
        "schema_version": 2,
        "metadata": {"objects": {}},
        "functions": [
            {
                "function": "helper",
                "params": ["x"],
                "annotations": ["yard.inline"],
                "body": [{"type": "Array", "name": "x", "indices": ["0"]}],
            },
            {
                "function": "kernel",
                "params": ["a"],
                "annotations": ["yard.analyze"],
                "body": [{"type": "Call", "callee": "helper", "args": ["a"]}],
            },
        ],
    }

    expanded = expand_calls(raw)
    assert [entry["function"] for entry in expanded] == ["kernel"]
    assert expanded[0]["body"] == [{"type": "Array", "name": "a", "indices": ["0"]}]


def test_expand_calls_fills_v2_array_metadata():
    raw = {
        "schema_version": 2,
        "metadata": {
            "objects": {
                "global::A": {
                    "shape": [16],
                    "elem_size": 8,
                }
            }
        },
        "functions": [{
            "function": "kernel",
            "body": [{"type": "Array", "name": "A", "object": "global::A",
                      "indices": ["i"]}],
        }],
    }

    expanded = expand_calls(raw)
    assert expanded[0]["body"][0]["shape"] == [16]
    assert expanded[0]["body"][0]["elem_size"] == 8


def test_expand_calls_substitutes_access_path_indices():
    raw = [
        {
            "function": "helper",
            "params": ["x", "idx"],
            "annotations": ["yard.inline"],
            "body": [{
                "type": "Array",
                "name": "x.items[idx].value",
                "indices": ["idx"],
                "access_path": [
                    {"kind": "field", "name": "items", "index": 1},
                    {"kind": "index", "value": "idx"},
                    {"kind": "field", "name": "value", "index": 0},
                ],
            }],
        },
        {
            "function": "kernel",
            "params": ["a"],
            "annotations": ["yard.analyze"],
            "body": [{"type": "Call", "callee": "helper", "args": ["a", "i"]}],
        },
    ]

    expanded = expand_calls(raw)
    node = expanded[0]["body"][0]

    assert node["name"] == "a.items[i].value"
    assert node["indices"] == ["i"]
    assert node["access_path"] == [
        {"kind": "field", "name": "items", "index": 1},
        {"kind": "index", "value": "i"},
        {"kind": "field", "name": "value", "index": 0},
    ]
