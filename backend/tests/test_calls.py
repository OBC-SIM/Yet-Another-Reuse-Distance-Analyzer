import pytest

from calls import expand_calls


def test_expand_calls_substitutes_args_in_place():
    raw = [
        {
            "function": "helper",
            "params": ["x", "idx"],
            "body": [
                {"type": "Array", "name": "x", "indices": ["idx"]},
                {"type": "Array", "name": "x", "indices": ["idx+1"]},
            ],
        },
        {
            "function": "kernel",
            "params": ["a"],
            "body": [
                {"type": "Loop", "var": "i", "start": 0, "bound": 4, "depth": 1, "body": [
                    {"type": "Array", "name": "a", "indices": ["i"]},
                    {"type": "Call", "callee": "helper", "args": ["a", "i"]},
                ]},
            ],
        },
    ]

    expanded = expand_calls(raw)
    body = expanded[1]["body"][0]["body"]

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
