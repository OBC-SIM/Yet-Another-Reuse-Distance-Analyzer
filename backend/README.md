# YARDA C++ Backend

This backend consumes legacy or APE v2 Loop Annotated Trace JSON and computes
reuse-distance histograms without Python. It supports exact element/cache-line
unrolling.

## Build and test

Requirements: LLVM 14, CMake 3.20+, a C++17 compiler, nlohmann/json, and
GTest. The root build configures both the frontend submodule and this backend.

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build --output-on-failure
```

CTest runs the frontend and backend test suites together.

## Run

```bash
./build/backend/yarda_cpp tasks/polybench_atax_g_ape.json \
  --mode unroll \
  --granularity cache-line \
  --cache backend/config/cache.32b.yaml \
  --export atax_rdh.json
```

The unroll path implements LAT v1/v2 normalization, annotated direct-call
expansion, block profiles, and Python-compatible JSON export. Exact unrolling
uses a Fenwick tree for O(N log N) reuse-distance profiling. `--mode unroll`
remains accepted for command-line compatibility; `--mode predict` is not
supported. Cache-line granularity requires a versioned YAML hierarchy passed
through `--cache`; core 0's configured L1 line size defines trace grouping.
APE v2 cache-line references use the canonical object ID, so different index
expressions for the same storage share one line identity. Legacy accesses
without an object ID fall back to their reference name. Cache-line profiles
produced by the earlier name-based behavior must be regenerated.

For task-isolated linked-address mapping, pass a non-PIE executable and cache
configuration:

```bash
./build/backend/yarda_cpp task_ape.json \
  --elf task.elf \
  --cache backend/config/cache.32b.yaml \
  --export task_mapping.json
```

This path accepts only `ET_EXEC`, resolves canonical global objects once, and
maps them with core 0's L1 geometry. The versioned JSON preserves task-local
source ordinals, load/store operations, cross-line provenance, complete
resolution coverage, and known non-inline static call-site exclusions. The
`known_non_inline_static_call_sites` count is taken after inline expansion and
does not multiply sites by loop iterations. Calls to another analyzed root are
caller-local opaque sites while the callee remains a separate task. This mode
intrinsically maps cache lines: omit `--granularity` or pass `cache-line`;
explicit `element` is rejected. Its `linked_absolute` addresses are linked
virtual addresses, not automatically physical addresses. Without `--export`,
the JSON is written to stdout.
