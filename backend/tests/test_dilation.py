import pytest
from lru_sim import ReuseProfile
from dilation import (
    DilationStrategy,
    Predict2DStrategy,
    Predict3DStrategy,
    DilationStrategyFactory,
    DilationContextBuilder,
    DilationPredictor,
)


def make_profile(histogram: dict) -> ReuseProfile:
    p = ReuseProfile()
    p.histogram = dict(histogram)
    return p


class TestDilationStrategyFactory:
    def test_depth2_returns_2d_strategy(self):
        s = DilationStrategyFactory.get_strategy(2)
        assert isinstance(s, Predict2DStrategy)

    def test_depth3_returns_3d_strategy(self):
        s = DilationStrategyFactory.get_strategy(3)
        assert isinstance(s, Predict3DStrategy)

    def test_unsupported_depth_raises(self):
        with pytest.raises(NotImplementedError):
            DilationStrategyFactory.get_strategy(4)


class TestDilationContextBuilder:
    def _full_builder_2d(self):
        return (
            DilationContextBuilder()
            .set_target_bounds({"j": 4, "k": 4})
            .set_base_profile(make_profile({1: 10}))
            .add_coefficient("Incr_J", {})
            .add_coefficient("Incr_K", {})
            .add_coefficient("Coff_JK", {})
        )

    def test_build_succeeds_with_all_fields(self):
        ctx = self._full_builder_2d().build()
        assert "bounds" in ctx and "base" in ctx and "coeffs" in ctx

    def test_build_missing_bounds_raises(self):
        builder = (
            DilationContextBuilder()
            .set_base_profile(make_profile({}))
            .add_coefficient("Incr_J", {})
        )
        with pytest.raises(ValueError):
            builder.build()

    def test_build_missing_base_raises(self):
        builder = (
            DilationContextBuilder()
            .set_target_bounds({"j": 4, "k": 4})
            .add_coefficient("Incr_J", {})
        )
        with pytest.raises(ValueError):
            builder.build()

    def test_build_missing_coeffs_raises(self):
        builder = (
            DilationContextBuilder()
            .set_target_bounds({"j": 4, "k": 4})
            .set_base_profile(make_profile({}))
        )
        with pytest.raises(ValueError):
            builder.build()

    def test_chaining_returns_builder(self):
        b = DilationContextBuilder()
        assert b.set_target_bounds({}) is b
        assert b.set_base_profile(make_profile({})) is b
        assert b.add_coefficient("X", {}) is b


class TestPredict2DStrategy:
    def _predict(self, base_hist, bounds, coeffs):
        builder = (
            DilationContextBuilder()
            .set_target_bounds(bounds)
            .set_base_profile(make_profile(base_hist))
        )
        for name, vals in coeffs.items():
            builder.add_coefficient(name, vals)
        return DilationPredictor(2).execute(builder)

    def test_all_zero_coeffs_returns_base(self):
        # dist_j = 4-2 = 2, dist_k = 4-2 = 2, all coeffs 0 → Frq == base
        result = self._predict(
            {1: 10},
            {"j": 4, "k": 4},
            {"Incr_J": {}, "Incr_K": {}, "Coff_JK": {}},
        )
        assert result.histogram[1] == 10

    def test_incr_j_only_scales_linearly(self):
        # dist_j = 3-2 = 1, dist_k = 2-2 = 0
        # Frq[1] = 5 + 1*4 + 0*0 + 0*1*0 = 9
        result = self._predict(
            {1: 5},
            {"j": 3, "k": 2},
            {"Incr_J": {1: 4}, "Incr_K": {}, "Coff_JK": {}},
        )
        assert result.histogram[1] == 9

    def test_full_2d_equation(self):
        # dist_j = 4-2 = 2, dist_k = 5-2 = 3
        # Frq[1] = 10 + 2*2 + 3*1 + 1*2*3 = 10+4+3+6 = 23
        result = self._predict(
            {1: 10},
            {"j": 4, "k": 5},
            {"Incr_J": {1: 2}, "Incr_K": {1: 1}, "Coff_JK": {1: 1}},
        )
        assert result.histogram[1] == 23

    def test_multiple_reuse_distances(self):
        # rd=0 and rd=1 both dilated independently
        result = self._predict(
            {0: 4, 1: 6},
            {"j": 3, "k": 2},
            {"Incr_J": {0: 1, 1: 2}, "Incr_K": {}, "Coff_JK": {}},
        )
        # dist_j=1, dist_k=0
        assert result.histogram[0] == 4 + 1 * 1   # 5
        assert result.histogram[1] == 6 + 1 * 2   # 8


class TestPredict3DStrategy:
    def _predict(self, base_hist, bounds, coeffs):
        builder = (
            DilationContextBuilder()
            .set_target_bounds(bounds)
            .set_base_profile(make_profile(base_hist))
        )
        for name, vals in coeffs.items():
            builder.add_coefficient(name, vals)
        return DilationPredictor(3).execute(builder)

    def _zero_coeffs_3d(self):
        return {k: {} for k in
                ["Incr_I", "Incr_J", "Incr_K",
                 "Coff_IJ", "Coff_JK", "Coff_IK", "Coff_IJK"]}

    def test_all_zero_coeffs_returns_base(self):
        result = self._predict(
            {2: 8},
            {"i": 4, "j": 4, "k": 4},
            self._zero_coeffs_3d(),
        )
        assert result.histogram[2] == 8

    def test_incr_i_only(self):
        # dist_i=1, dist_j=0, dist_k=0
        # Frq[2] = 5 + 1*3 = 8
        coeffs = self._zero_coeffs_3d()
        coeffs["Incr_I"] = {2: 3}
        result = self._predict(
            {2: 5},
            {"i": 3, "j": 2, "k": 2},
            coeffs,
        )
        assert result.histogram[2] == 8

    def test_cross_term_ijk(self):
        # dist_i=1, dist_j=1, dist_k=1
        # Frq[1] = 2 + 1*1 + 1*1 + 1*1 + 1*1*1 + 1*1*1 + 1*1*1 + 1*1*1*1 = 2+1+1+1+1+1+1+1 = 9
        coeffs = {k: {1: 1} for k in
                  ["Incr_I", "Incr_J", "Incr_K",
                   "Coff_IJ", "Coff_JK", "Coff_IK", "Coff_IJK"]}
        result = self._predict(
            {1: 2},
            {"i": 3, "j": 3, "k": 3},
            coeffs,
        )
        assert result.histogram[1] == 9