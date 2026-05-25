from typing import Dict, List, Set

from lru_sim import ReuseProfile
from sequence_summary import DenseSequence


class BlockMerger:
    def __init__(self):
        self.global_profile = ReuseProfile()
        self.global_lru_stack: List[str] = []  # MRU at index 0
        self.last_sequence: DenseSequence | None = None

    def adjust_cold_misses(self, block_cold_misses: Set[str]) -> None:
        for addr in block_cold_misses:
            if addr not in self.global_profile.cold_misses:
                self.global_profile.cold_misses.add(addr)

    def adjust_array_reuses(self, block_trace: List[str]) -> Dict[int, int]:
        adjusted: Dict[int, int] = {}
        prior_addrs = set(self.global_lru_stack)
        for addr in block_trace:
            if addr in self.global_lru_stack:
                dist = self.global_lru_stack.index(addr)
                if addr in prior_addrs:
                    adjusted[dist] = adjusted.get(dist, 0) + 1
                    prior_addrs.remove(addr)
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

        self.last_sequence = None
        return self.global_profile

    def merge_sequence(
        self,
        block_profile: ReuseProfile,
        sequence: DenseSequence,
        fallback_trace: List[str],
    ) -> ReuseProfile:
        for rd, freq in block_profile.histogram.items():
            self.global_profile.histogram[rd] = self.global_profile.histogram.get(rd, 0) + freq

        self.adjust_cold_misses(block_profile.cold_misses)

        if self.last_sequence and sequence.same_order(self.last_sequence):
            rd = sequence.size - 1
            self.global_profile.histogram[rd] = (
                self.global_profile.histogram.get(rd, 0) + sequence.size
            )
            self.global_lru_stack.clear()
        else:
            self.adjust_array_reuses(fallback_trace)

        self.last_sequence = sequence
        return self.global_profile
