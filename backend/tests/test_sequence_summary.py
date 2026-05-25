from sequence_summary import DenseSequence, dense_sequence


def test_dense_sequence_extracts_perfect_array_loop():
    raw = {
        "type": "Loop", "var": "i", "bound": 4, "depth": 1, "body": [
            {"type": "Loop", "var": "j", "bound": 5, "depth": 2, "body": [
                {"type": "Array", "name": "tmp", "indices": ["i", "j"]},
            ]},
        ],
    }

    assert dense_sequence(raw) == DenseSequence(
        name="tmp",
        indices=("i", "j"),
        loop_vars=("i", "j"),
        starts=(0, 0),
        bounds=(4, 5),
    )


def test_dense_sequence_rejects_interleaved_accesses():
    raw = {
        "type": "Loop", "var": "i", "bound": 4, "depth": 1, "body": [
            {"type": "Array", "name": "a", "indices": ["i"]},
            {"type": "Array", "name": "b", "indices": ["i"]},
        ],
    }

    assert dense_sequence(raw) is None


def test_dense_sequence_requires_index_order_to_match_loop_order():
    raw = {
        "type": "Loop", "var": "i", "bound": 4, "depth": 1, "body": [
            {"type": "Loop", "var": "j", "bound": 5, "depth": 2, "body": [
                {"type": "Array", "name": "tmp", "indices": ["j", "i"]},
            ]},
        ],
    }

    assert dense_sequence(raw) is None
