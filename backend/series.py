from typing import List, Tuple


def predict_tail_series(points: List[Tuple[int, int]], target: int) -> int:
    points = sorted(dict(points).items())
    if not points:
        return 0
    if len(points) == 1:
        return points[0][1]
    if len(points) >= 5:
        xs = [x for x, _ in points[-5:]]
        if all(b - a == 1 for a, b in zip(xs, xs[1:])):
            values = [y for _, y in points[-5:]]
            diffs = [b - a for a, b in zip(values, values[1:])]
            diff2 = [b - a for a, b in zip(diffs, diffs[1:])]
            diff3 = [b - a for a, b in zip(diff2, diff2[1:])]
            if len(set(diff3)) == 1:
                value, diff, second = values[-1], diffs[-1], diff2[-1]
                for _ in range(target - xs[-1]):
                    second += diff3[-1]
                    diff += second
                    value += diff
                return value
    tail = points[-3:]
    if len(tail) == 3:
        (x0, y0), (x1, y1), (x2, y2) = tail
        if len({x0, x1, x2}) == 3:
            terms = (
                y0 * (target - x1) * (target - x2) / ((x0 - x1) * (x0 - x2)),
                y1 * (target - x0) * (target - x2) / ((x1 - x0) * (x1 - x2)),
                y2 * (target - x0) * (target - x1) / ((x2 - x0) * (x2 - x1)),
            )
            return round(sum(terms))
    x1, y1 = points[-2]
    x2, y2 = points[-1]
    return round(y2 + (target - x2) * (y2 - y1) / (x2 - x1))
