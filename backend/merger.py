from typing import Dict, List, Set

from lru_sim import ReuseProfile


class BlockMerger:
    def __init__(self):
        self.global_profile = ReuseProfile()
        self.global_lru_stack: List[str] = []  # MRU at index 0

    def adjust_cold_misses(self, block_cold_misses: Set[str]) -> None:
        for addr in block_cold_misses:
            if addr not in self.global_profile.cold_misses:
                self.global_profile.cold_misses.add(addr)

    def adjust_array_reuses(self, block_trace: List[str]) -> Dict[int, int]:
        adjusted: Dict[int, int] = {}
        for addr in block_trace:
            if addr in self.global_lru_stack:
                dist = self.global_lru_stack.index(addr)
                adjusted[dist] = adjusted.get(dist, 0) + 1
                self.global_lru_stack.pop(dist)
            self.global_lru_stack.insert(0, addr)
        return adjusted

    def merge_block(self, block_profile: ReuseProfile, block_trace: List[str]) -> ReuseProfile:
        for rd, freq in block_profile.histogram.items():
            self.global_profile.histogram[rd] = self.global_profile.histogram.get(rd, 0) + freq

        self.adjust_cold_misses(block_profile.cold_misses)

        cross_reuses = self.adjust_array_reuses(block_trace)
        for rd, freq in cross_reuses.items():
            self.global_profile.histogram[rd] = self.global_profile.histogram.get(rd, 0) + freq

        return self.global_profile