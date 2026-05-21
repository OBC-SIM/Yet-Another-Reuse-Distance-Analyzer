from abc import ABC, abstractmethod
from typing import Dict

from lru_sim import ReuseProfile


class DilationStrategy(ABC):
    @abstractmethod
    def predict(self, context: Dict) -> ReuseProfile:
        pass


class Predict2DStrategy(DilationStrategy):
    def predict(self, context: Dict) -> ReuseProfile:
        bounds = context["bounds"]
        base = context["base"]
        coeffs = context["coeffs"]

        dist_j = bounds["j"] - 2
        dist_k = bounds["k"] - 2

        predicted = ReuseProfile()
        for rd, b22 in base.histogram.items():
            freq = (
                b22
                + dist_j * coeffs["Incr_J"].get(rd, 0)
                + dist_k * coeffs["Incr_K"].get(rd, 0)
                + coeffs["Coff_JK"].get(rd, 0) * dist_j * dist_k
            )
            predicted.histogram[rd] = freq
        return predicted


class Predict3DStrategy(DilationStrategy):
    def predict(self, context: Dict) -> ReuseProfile:
        bounds = context["bounds"]
        base = context["base"]
        coeffs = context["coeffs"]

        dist_i = bounds["i"] - 2
        dist_j = bounds["j"] - 2
        dist_k = bounds["k"] - 2

        predicted = ReuseProfile()
        for rd, b222 in base.histogram.items():
            freq = (
                b222
                + dist_i * coeffs["Incr_I"].get(rd, 0)
                + dist_j * coeffs["Incr_J"].get(rd, 0)
                + dist_k * coeffs["Incr_K"].get(rd, 0)
                + coeffs["Coff_IJ"].get(rd, 0) * dist_i * dist_j
                + coeffs["Coff_JK"].get(rd, 0) * dist_j * dist_k
                + coeffs["Coff_IK"].get(rd, 0) * dist_i * dist_k
                + coeffs["Coff_IJK"].get(rd, 0) * dist_i * dist_j * dist_k
            )
            predicted.histogram[rd] = freq
        return predicted


class DilationStrategyFactory:
    @staticmethod
    def get_strategy(loop_depth: int) -> DilationStrategy:
        if loop_depth == 2:
            return Predict2DStrategy()
        elif loop_depth == 3:
            return Predict3DStrategy()
        raise NotImplementedError(f"{loop_depth}D Dilation은 아직 지원하지 않습니다.")


class DilationContextBuilder:
    def __init__(self):
        self._context: Dict = {}

    def set_target_bounds(self, bounds: Dict[str, int]) -> "DilationContextBuilder":
        self._context["bounds"] = bounds
        return self

    def set_base_profile(self, profile: ReuseProfile) -> "DilationContextBuilder":
        self._context["base"] = profile
        return self

    def add_coefficient(self, name: str, values: Dict[int, int]) -> "DilationContextBuilder":
        self._context.setdefault("coeffs", {})[name] = values
        return self

    def build(self) -> Dict:
        missing = [k for k in ("bounds", "base", "coeffs") if k not in self._context]
        if missing:
            raise ValueError(f"필수 파라미터 누락: {missing}")
        return self._context


class DilationPredictor:
    def __init__(self, loop_depth: int):
        self._strategy = DilationStrategyFactory.get_strategy(loop_depth)

    def execute(self, builder: DilationContextBuilder) -> ReuseProfile:
        return self._strategy.predict(builder.build())