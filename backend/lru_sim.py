from typing import Dict, List, Set


class ReuseProfile:
    def __init__(self):
        self.histogram: Dict[int, int] = {}
        self.cold_misses: Set[str] = set()


class LRUProfiler:
    @staticmethod
    def calculate(trace: List[str]) -> ReuseProfile:
        profile = ReuseProfile()
        stack: List[str] = []  # most-recently-used at index 0

        for addr in trace:
            if addr not in stack:
                profile.cold_misses.add(addr)
            else:
                dist = stack.index(addr)
                profile.histogram[dist] = profile.histogram.get(dist, 0) + 1
                stack.pop(dist)

            stack.insert(0, addr)

        return profile