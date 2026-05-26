import gt_cache


FUNCTION_ENTRY = {
    "function": "cached_func",
    "body": [
        {"type": "Array", "name": "A", "indices": ["0"]},
        {"type": "Loop", "var": "i", "bound": 4, "depth": 1, "body": [
            {"type": "Array", "name": "A", "indices": ["i"]},
        ]},
        {"type": "Array", "name": "A", "indices": ["0"]},
    ],
}


def test_function_ground_truth_cached_reuses_saved_profile(tmp_path):
    gt_cache.GT_CACHE_PATH = str(tmp_path / "gt_cache.json")
    gt_cache._GT_CACHE = None

    first, first_cached, first_unroll = gt_cache.function_ground_truth_cached(FUNCTION_ENTRY)
    second, second_cached, second_unroll = gt_cache.function_ground_truth_cached(FUNCTION_ENTRY)

    assert first_cached is False
    assert second_cached is True
    assert second.histogram == first.histogram
    assert second.cold_misses == first.cold_misses
    assert second_unroll == first_unroll


def test_function_ground_truth_cache_key_is_separate_from_loop_cache(tmp_path):
    gt_cache.GT_CACHE_PATH = str(tmp_path / "gt_cache.json")
    gt_cache._GT_CACHE = None

    loop = FUNCTION_ENTRY["body"][1]
    gt_cache.ground_truth_cached(loop)
    _, cached, _ = gt_cache.function_ground_truth_cached(FUNCTION_ENTRY)

    assert cached is False
