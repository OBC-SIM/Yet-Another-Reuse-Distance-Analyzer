from typing import Dict, Iterable, List, Set

from lru_sim import ReuseProfile
from sequence_summary import AccessPattern, SequenceSummary


class BlockMerger:
    def __init__(self):
        self.global_profile = ReuseProfile()
        self.global_lru_stack: List[str] = []  # MRU at index 0
        self.symbolic_stack: List[AccessPattern] = []
        self.symbolic_reused: Set[str] = set()

    def adjust_cold_misses(self, block_cold_misses: Set[str]) -> None:
        for addr in block_cold_misses:
            if addr not in self.global_profile.cold_misses:
                self.global_profile.cold_misses.add(addr)

    def adjust_array_reuses(self, block_trace: Iterable[str]) -> Dict[int, int]:
        adjusted: Dict[int, int] = {}
        prior_addrs = set(self.global_lru_stack)
        for addr in block_trace:
            if addr in self.global_lru_stack:
                dist = self.global_lru_stack.index(addr)
                if addr in prior_addrs:
                    adjusted[dist] = adjusted.get(dist, 0) + 1
                    prior_addrs.remove(addr)
                self.global_lru_stack.pop(dist)
            elif addr not in self.symbolic_reused:
                dist = self._symbolic_distance(addr)
                if dist is not None:
                    adjusted[dist] = adjusted.get(dist, 0) + 1
                    self.symbolic_reused.add(addr)
            self.global_lru_stack.insert(0, addr)
        return adjusted

    def merge_block(self, block_profile: ReuseProfile, block_trace: List[str]) -> ReuseProfile:
        for rd, freq in block_profile.histogram.items():
            self.global_profile.histogram[rd] = self.global_profile.histogram.get(rd, 0) + freq

        self.adjust_cold_misses(block_profile.cold_misses)

        cross_reuses = self.adjust_array_reuses(block_trace)
        for rd, freq in cross_reuses.items():
            self.global_profile.histogram[rd] = self.global_profile.histogram.get(rd, 0) + freq

        self.symbolic_stack.clear()
        return self.global_profile

    def merge_sequence(
        self,
        block_profile: ReuseProfile,
        sequence: SequenceSummary,
        fallback_trace: List[str],
    ) -> ReuseProfile:
        for rd, freq in block_profile.histogram.items():
            self.global_profile.histogram[rd] = self.global_profile.histogram.get(rd, 0) + freq

        self.adjust_cold_misses(block_profile.cold_misses)

        if not self._merge_symbolic_reuses(sequence):
            cross_reuses = self.adjust_array_reuses(fallback_trace)
            for rd, freq in cross_reuses.items():
                self.global_profile.histogram[rd] = self.global_profile.histogram.get(rd, 0) + freq
        return self.global_profile

    def _merge_symbolic_reuses(self, sequence: SequenceSummary) -> bool:
        if not sequence.first_patterns:
            return False

        stack = list(self.symbolic_stack)
        for pattern in sequence.first_patterns:
            for index, addr in enumerate(list(self.global_lru_stack)):
                if self._addr_matches_pattern(addr, pattern):
                    self.global_profile.histogram[index] = (
                        self.global_profile.histogram.get(index, 0) + 1
                    )
                    self.global_lru_stack.pop(index)
                    break
            if pattern in stack:
                index = stack.index(pattern)
                rd = sum(segment.size for segment in stack[:index]) + pattern.size - 1
                self.global_profile.histogram[rd] = (
                    self.global_profile.histogram.get(rd, 0) + pattern.size
                )
                stack.pop(index)
            stack.insert(0, pattern)

        final_patterns = list(sequence.final_patterns)
        retained = [pattern for pattern in stack if pattern not in final_patterns]
        self.symbolic_stack = final_patterns + retained
        self.global_lru_stack.clear()
        self.symbolic_reused.clear()
        return True

    def _symbolic_distance(self, addr: str) -> int | None:
        parts = addr.split("-")
        if not parts:
            return None
        name, raw_indices = parts[0], parts[1:]
        try:
            indices = tuple(int(index) for index in raw_indices)
        except ValueError:
            return None

        prefix = 0
        for pattern in self.symbolic_stack:
            if pattern.name == name and len(pattern.shape) == len(indices):
                rank = self._rank_in_pattern(pattern, indices)
                if rank is not None:
                    return prefix + rank
            prefix += pattern.size
        return None

    @staticmethod
    def _addr_matches_pattern(addr: str, pattern: AccessPattern) -> bool:
        parts = addr.split("-")
        if not parts or parts[0] != pattern.name or len(parts[1:]) != len(pattern.shape):
            return False
        try:
            indices = tuple(int(index) for index in parts[1:])
        except ValueError:
            return False
        return BlockMerger._rank_in_pattern(pattern, indices) is not None

    @staticmethod
    def _rank_in_pattern(pattern: AccessPattern, indices: tuple[int, ...]) -> int | None:
        forward_rank = 0
        stride = 1
        offsets = [index - start for index, start in zip(indices, pattern.starts)]
        if any(offset < 0 or offset >= bound for offset, bound in zip(offsets, pattern.shape)):
            return None
        for offset, bound in reversed(list(zip(offsets, pattern.shape))):
            forward_rank += offset * stride
            stride *= bound
        return pattern.size - 1 - forward_rank
