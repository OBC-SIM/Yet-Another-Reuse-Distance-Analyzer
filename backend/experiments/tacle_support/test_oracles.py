"""Check that trace verification detects address, ordering and width errors."""
from copy import deepcopy

import pytest

from backend.experiments.tacle_support.oracles import compare_trace, source_accesses


def fixture():
    expected = [("array", 0, 4, "load"), ("array", 4, 4, "store")]
    actual = [{"object_id": "global::array", "object_byte_offset": offset,
               "access_size": 4, "operation": op, "source_access_ordinal": i,
               "linked_byte_address": 4096 + offset}
              for i, (_, offset, _, op) in enumerate(expected)]
    return expected, actual, {"array": (4096, 8)}


def test_matches_independently_linked_symbol_and_source_sequence():
    expected, actual, symbols = fixture()
    assert compare_trace(actual, expected, symbols) is None


@pytest.mark.parametrize("field,value", [("object_id", "global::other"),
    ("object_byte_offset", 4), ("access_size", 1), ("operation", "store"),
    ("linked_byte_address", 0), ("source_access_ordinal", 1)])
def test_rejects_corrupted_access_even_when_count_is_correct(field, value):
    expected, actual, symbols = fixture()
    actual[0][field] = value
    assert compare_trace(actual, expected, symbols) is not None


def test_rejects_repeated_or_missing_access():
    expected, actual, symbols = fixture()
    assert compare_trace(actual + [deepcopy(actual[0])], expected, symbols) is not None
    assert compare_trace(actual[:-1], expected, symbols) is not None


def test_requires_expected_access_to_fit_real_elf_object():
    expected, actual, symbols = fixture()
    symbols["array"] = (4096, 4)
    assert compare_trace(actual, expected, symbols) is not None


@pytest.mark.parametrize("name,count,first,last", [
    ("matrix1", 4100, ("matrix1_C", 0, 4, "store"), ("matrix1_C", 396, 4, "store")),
    ("complex_updates", 256, ("complex_updates_C", 0, 4, "load"),
     ("complex_updates_D", 124, 4, "store")),
    ("iir", 49, ("iir_x", 0, 4, "load"), ("iir_wi", 24, 4, "store")),
    ("jfdctint", 384, ("jfdctint_data", 0, 4, "load"),
     ("jfdctint_data", 60, 4, "store"))])
def test_registered_source_sequences_keep_reviewed_counts_and_endpoints(name, count, first, last):
    accesses = source_accesses(name)
    assert len(accesses) == count
    assert accesses[0] == first
    assert accesses[-1] == last


def test_unknown_source_has_no_oracle():
    assert source_accesses("unreviewed") is None
