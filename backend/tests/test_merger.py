import pytest
from lru_sim import ReuseProfile
from merger import BlockMerger


def make_profile(histogram: dict, cold_misses: set = None) -> ReuseProfile:
    p = ReuseProfile()
    p.histogram = dict(histogram)
    p.cold_misses = set(cold_misses or [])
    return p


class TestBlockMergerInit:
    def test_initial_state(self):
        merger = BlockMerger()
        assert merger.global_profile.histogram == {}
        assert merger.global_profile.cold_misses == set()
        assert merger.global_lru_stack == []


class TestAdjustColdMisses:
    def test_new_cold_misses_added(self):
        merger = BlockMerger()
        merger.adjust_cold_misses({"A-0", "B-0"})
        assert merger.global_profile.cold_misses == {"A-0", "B-0"}

    def test_duplicate_not_re_added(self):
        merger = BlockMerger()
        merger.adjust_cold_misses({"A-0", "B-0"})
        merger.adjust_cold_misses({"A-0", "C-0"})
        assert merger.global_profile.cold_misses == {"A-0", "B-0", "C-0"}


class TestAdjustArrayReuses:
    def test_empty_global_lru_no_cross_reuse(self):
        # 첫 블록 — global_lru_stack이 비어있으므로 cross-block reuse 없음
        merger = BlockMerger()
        result = merger.adjust_array_reuses(["A-0", "B-0"])
        assert result == {}

    def test_first_block_populates_global_lru(self):
        merger = BlockMerger()
        merger.adjust_array_reuses(["A-0", "B-0"])
        assert set(merger.global_lru_stack) == {"A-0", "B-0"}

    def test_cross_block_reuse_detected(self):
        # 블록1: [A-0, B-0] → global_lru = [B-0, A-0] (MRU-front)
        # 블록2: [A-0] 재접근 → idx=1, dist=1
        merger = BlockMerger()
        merger.adjust_array_reuses(["A-0", "B-0"])
        result = merger.adjust_array_reuses(["A-0"])
        assert result == {1: 1}

    def test_most_recently_used_distance_zero(self):
        # 블록1: [A-0, B-0] → global_lru = [B-0, A-0] (B-0이 MRU, idx=0)
        # 블록2: [B-0] 재접근 → dist=0
        merger = BlockMerger()
        merger.adjust_array_reuses(["A-0", "B-0"])
        result = merger.adjust_array_reuses(["B-0"])
        assert result == {0: 1}

    def test_multiple_cross_block_reuses(self):
        # trace: A-0 → B-0 → C-0
        # stack: top<C-0, B-0, A-0>bot
        #
        # trace: A-0 → B-0
        #   A-0: stack[2]=A-0 → dist=2, 갱신 → top<A-0, C-0, B-0>bot
        #   B-0: stack[2]=B-0 → dist=2, 갱신 → top<B-0, A-0, C-0>bot
        merger = BlockMerger()
        merger.adjust_array_reuses(["A-0", "B-0", "C-0"])
        result = merger.adjust_array_reuses(["A-0", "B-0"])
        assert result == {2: 2}

    def test_global_lru_updated_after_cross_block_reuse(self):
        # A-0 재접근 후 MRU 위치(index=0)로 이동해야 함
        merger = BlockMerger()
        merger.adjust_array_reuses(["A-0", "B-0"])
        merger.adjust_array_reuses(["A-0"])
        assert merger.global_lru_stack[0] == "A-0"

    def test_new_address_in_second_block_appended(self):
        merger = BlockMerger()
        merger.adjust_array_reuses(["A-0"])
        merger.adjust_array_reuses(["B-0"])  # B-0은 새 주소
        assert "B-0" in merger.global_lru_stack


class TestMergeBlock:
    def test_block_histogram_accumulated(self):
        merger = BlockMerger()
        merger.merge_block(make_profile({1: 3}, set()), [])
        result = merger.merge_block(make_profile({1: 2}, set()), [])
        assert result.histogram[1] == 5

    def test_cold_misses_deduplicated(self):
        merger = BlockMerger()
        merger.merge_block(make_profile({}, {"A-0", "B-0"}), ["A-0", "B-0"])
        result = merger.merge_block(make_profile({}, {"A-0", "C-0"}), ["A-0", "C-0"])
        assert result.cold_misses == {"A-0", "B-0", "C-0"}

    def test_cross_block_reuse_added_to_histogram(self):
        # 블록1 후 global_lru = [B-0, A-0]
        # 블록2에서 B-0 재접근 → dist=0 이 histogram에 추가됨
        merger = BlockMerger()
        merger.merge_block(make_profile({}, {"A-0", "B-0"}), ["A-0", "B-0"])
        result = merger.merge_block(make_profile({}, {"B-0"}), ["B-0"])
        assert result.histogram.get(0, 0) == 1

    def test_returns_global_profile(self):
        merger = BlockMerger()
        result = merger.merge_block(make_profile({2: 1}, set()), [])
        assert result is merger.global_profile