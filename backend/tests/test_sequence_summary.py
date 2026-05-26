from sequence_summary import AccessPattern, SequenceSummary, summarize_sequence


def test_sequence_summary_records_first_and_final_patterns():
    raw = {
        "type": "Loop", "var": "i", "bound": 2, "depth": 1, "body": [
            {"type": "Array", "name": "tmp", "indices": ["i"]},
            {"type": "Array", "name": "tmp", "indices": ["i"]},
            {"type": "Array", "name": "b", "indices": ["i"]},
        ],
    }

    tmp = AccessPattern("tmp", (2,), (0,))
    b = AccessPattern("b", (2,), (0,))
    assert summarize_sequence(raw) == SequenceSummary(
        first_patterns=[tmp, b],
        final_patterns=[b, tmp],
    )


def test_sequence_summary_uses_shapes_not_loop_variable_names():
    left = {
        "type": "Loop", "var": "i", "bound": 2, "depth": 1, "body": [
            {"type": "Loop", "var": "j", "bound": 3, "depth": 2, "body": [
                {"type": "Array", "name": "tmp", "indices": ["i", "j"]},
            ]},
        ],
    }
    right = {
        "type": "Loop", "var": "x", "bound": 2, "depth": 1, "body": [
            {"type": "Loop", "var": "y", "bound": 3, "depth": 2, "body": [
                {"type": "Array", "name": "tmp", "indices": ["x", "y"]},
            ]},
        ],
    }

    assert summarize_sequence(left) == summarize_sequence(right)


def test_sequence_summary_rejects_unexpanded_calls():
    raw = {"type": "Call", "callee": "kernel", "args": []}

    assert summarize_sequence(raw) is None


def test_sequence_summary_rejects_non_affine_indices():
    raw = {
        "type": "Loop", "var": "i", "bound": 2, "depth": 1, "body": [
            {"type": "Array", "name": "a", "indices": ["i*j"]},
        ],
    }

    assert summarize_sequence(raw) is None
