"""Source-derived access sequences for four TACLeBench V2.0 kernels.

Local temporaries disappear under mem2reg. Operand order follows Clang 14 O0
(checked in pre-MAP IR where C leaves operand evaluation order unspecified).
These formulas do not consume MAP loop bounds, indices or object metadata.
"""

def _matrix1(emit):
    for k in range(10):
        for i in range(10):
            emit("C", 10 * k + i, "store")
            for f in range(10):
                emit("A", 10 * i + f, "load")
                emit("B", 10 * k + f, "load")
                emit("C", 10 * k + i, "load")
                emit("C", 10 * k + i, "store")


def _complex_updates(emit):
    for i in range(16):
        q = 2 * i
        for symbol, index, op in [
            ("C", q, "load"), ("A", q, "load"), ("B", q, "load"), ("D", q, "store"),
            ("A", q + 1, "load"), ("B", q + 1, "load"), ("D", q, "load"), ("D", q, "store"),
            ("C", q + 1, "load"), ("A", q + 1, "load"), ("B", q, "load"), ("D", q + 1, "store"),
            ("A", q, "load"), ("B", q + 1, "load"), ("D", q + 1, "load"), ("D", q + 1, "store")]:
            emit(symbol, index, op)


def _iir(emit):
    emit("x", 0, "load")
    for f in range(4):
        c, w = 5 * f, 2 * f
        for symbol, index, op in [
            ("coefficients", c, "load"), ("wi", w, "load"),
            ("coefficients", c + 1, "load"), ("wi", w + 1, "load"),
            ("coefficients", c + 2, "load"), ("coefficients", c + 3, "load"),
            ("wi", w, "load"), ("coefficients", c + 4, "load"), ("wi", w + 1, "load"),
            ("wi", w, "load"), ("wi", w + 1, "store"), ("wi", w, "store")]:
            emit(symbol, index, op)


def _jfdctint(emit):
    loads = [0, 7, 0, 7, 1, 6, 1, 6, 2, 5, 2, 5, 3, 4, 3, 4]
    stores = [0, 4, 2, 6, 7, 5, 3, 1]
    for columns in (False, True):
        for i in range(8):
            for offsets, op in ((loads, "load"), (stores, "store")):
                for d in offsets:
                    emit("data", i + 8 * d if columns else 8 * i + d, op)


# Each source formula is registered with its reviewed IR and source-tree pins.
# The tree hash is SHA256(json.dumps({relative_path: file_sha256}, sort_keys=True));
# paths include src/. A name alone must never authorize a different input.
ORACLES = {
    "matrix1": ("13f583ce01294d4e365091067368e4111debd84846a940d1d7b0f17db38ae2c4",
                "7dd38d6dd6f3219dbe0052638a80fffe24c6570ed85acb8416fc3f7b5b3a4921", _matrix1),
    "complex_updates": ("fac02eb3db59da18efa23adcc0f749878c971a665160437056ed943e34171368",
                        "21ea1ea01bb835a2cfdfdd1678aafe7bc7c29947b10294999bc34a89bfc066b2",
                        _complex_updates),
    "iir": ("55f8ff68387ed0af489d4889362481b6e84248b4b12fca6e1242f7d6609bc8d4",
            "79e8d69e8d87285af5bcfbcfa06d25b040c32ebb396229cc55bb68bcc7899793", _iir),
    "jfdctint": ("83bd43cdc042a592b395eb29204b25329f8efaf8e9e750959a26ca8d8cee197c",
                 "c34f8c399e507ace8f526b672d7436a3debbe7bfa8a9975fba2dd7a4e2ff4a6a", _jfdctint),
}


def source_accesses(name):
    """Return (ELF symbol, byte offset, byte width, operation), or None."""
    oracle = ORACLES.get(name)
    if oracle is None:
        return None
    result = []

    def emit(symbol, index, op):
        result.append((name + "_" + symbol, 4 * index, 4, op))

    oracle[2](emit)
    return result


def compare_trace(actual, expected, symbols):
    """Return the first mismatch against source formulas and nm's ELF symbols."""
    if len(actual) != len(expected):
        return f"access count: got {len(actual)}, expected {len(expected)}"
    for i, (access, (symbol, offset, width, op)) in enumerate(zip(actual, expected)):
        if symbol not in symbols:
            return f"missing ELF symbol: {symbol}"
        base, size = symbols[symbol]
        if offset < 0 or offset + width > size:
            return f"oracle access outside ELF object: {symbol}+{offset}"
        wanted = dict(object_id="global::" + symbol, object_byte_offset=offset,
                      access_size=width, operation=op, source_access_ordinal=i,
                      linked_byte_address=base + offset)
        for key, value in wanted.items():
            if access.get(key) != value:
                return f"access {i} {key}: got {access.get(key)}, expected {value}"
    return None
