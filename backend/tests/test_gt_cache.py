from gt_cache import _compute_addr_maps, _to_int_trace


def test_addr_map_uses_all_affine_stencil_extents():
    raw = {
        "type": "Loop", "var": "i", "start": 1, "bound": 39, "depth": 1,
        "body": [{
            "type": "Loop", "var": "j", "start": 1, "bound": 39, "depth": 2,
            "body": [
                {"type": "Array", "name": "A", "indices": ["i", "j"]},
                {"type": "Array", "name": "A", "indices": ["i", "j-1"]},
                {"type": "Array", "name": "A", "indices": ["i", "j+1"]},
                {"type": "Array", "name": "A", "indices": ["i-1", "j"]},
                {"type": "Array", "name": "A", "indices": ["i+1", "j"]},
            ],
        }],
    }
    bases, cols, lows = _compute_addr_maps(raw)
    assert cols["A"] == 40
    assert lows["A"] == (0, 0)
    assert _to_int_trace(["A-1-39", "A-2-0"], bases, cols, lows) == ["79", "80"]


def test_addr_map_shifts_negative_lower_affine_extent():
    raw = {
        "type": "Loop", "var": "i", "start": 0, "bound": 3, "depth": 1,
        "body": [
            {"type": "Array", "name": "A", "indices": ["i-1", "0"]},
            {"type": "Array", "name": "A", "indices": ["i", "0"]},
        ],
    }
    bases, cols, lows = _compute_addr_maps(raw)
    assert lows["A"] == (-1, 0)
    assert _to_int_trace(["A--1-0", "A-0-0"], bases, cols, lows) == ["0", "1"]
