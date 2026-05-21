from lru_sim import ReuseProfile, LRUProfiler


class TestReuseProfile:
    def test_initial_state(self):
        p = ReuseProfile()
        assert p.histogram == {}
        assert p.cold_misses == set()


class TestLRUProfiler:
    def test_all_cold_misses(self):
        p = LRUProfiler.calculate(["A", "B", "C"])
        assert p.histogram == {}
        assert p.cold_misses == {"A", "B", "C"}

    def test_reuse_distance_one(self):
        # A → B → A: one element (B) between two A accesses
        p = LRUProfiler.calculate(["A", "B", "A"])
        assert p.histogram == {1: 1}
        assert "A" in p.cold_misses
        assert "B" in p.cold_misses

    def test_reuse_distance_zero(self):
        # A → A: no elements between
        p = LRUProfiler.calculate(["A", "A"])
        assert p.histogram == {0: 1}
        assert p.cold_misses == {"A"}

    def test_reuse_distance_two(self):
        # A → B → C → A: two elements (B, C) between
        p = LRUProfiler.calculate(["A", "B", "C", "A"])
        assert p.histogram == {2: 1}

    def test_histogram_accumulates(self):
        # A→B→A (dist 1), C→D→C (dist 1) → histogram[1] == 2
        p = LRUProfiler.calculate(["A", "B", "A", "C", "D", "C"])
        assert p.histogram[1] == 2

    def test_multiple_reuses_same_address(self):
        # A→B→A→B→A: A reuses at dist=1 (x2), B reuse at dist=1 (x1) → total 3
        p = LRUProfiler.calculate(["A", "B", "A", "B", "A"])
        assert p.histogram[1] == 3

    def test_cold_misses_counted_once(self):
        # A accessed three times → cold miss counted only once
        p = LRUProfiler.calculate(["A", "B", "A", "A"])
        assert p.cold_misses == {"A", "B"}

    def test_empty_trace(self):
        p = LRUProfiler.calculate([])
        assert p.histogram == {}
        assert p.cold_misses == set()