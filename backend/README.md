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

Region LAT is accepted by the task mapping and streaming hierarchy APIs. Its
`analysis_scope` is validated before task delivery, and result/event IDs use
`region:<UTF-8 byte length>:<original function>:APE_ANALYZE`. Original function
and object bindings remain unchanged; legacy whole-module unroll rejects region
LAT. Enable the optional source frontend with `YARDA_BUILD_REGION_FRONTEND=ON`
to also build the C-to-LAT/ET_EXEC integration fixtures. See the
[region contract](../docs/analysis-regions-v1.md) for selection semantics.

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
`known_non_inline_static_call_sites` count includes only known non-inline
`Call` nodes present in the input LAT after inline expansion; it is not a census
of every call in the original C source and does not multiply sites by loop
iterations. Calls to another analyzed root are caller-local opaque sites while
the callee remains a separate task. This mode intrinsically maps cache lines:
omit `--granularity` or pass `cache-line`; explicit `element` is rejected. Its
`linked_absolute` addresses are linked virtual addresses, not automatically
physical addresses. Without `--export`, the JSON is written to stdout.

Every LAT expansion path, including streaming hierarchy analysis, is limited to
100,000 call-expansion node visits and an inline call depth of 256. The CLI and
legacy batch APIs retain the default 1,000,000 iterations per loop and
1,000,000 cumulative loop iterations. These CLI and batch paths do not cap
source access count.
The `--elf` report materializes every resolved access and mapped line reference,
so large traces can exhaust host memory. Exceeding a structural expansion limit
fails the complete invocation instead of returning a partial trace.

## Streaming hierarchy work limits

The C++ streaming APIs expose independent single-loop and cumulative-loop
allowances through `LoopWorkLimits` in `yarda/trace/work_limits.hpp`. Both
default to 1,000,000 iterations. Hierarchy callers set them alongside the
existing source/line emission allowances:

```cpp
yarda::StreamingHierarchyOptions options;
options.loop_limits = {2'000'000, 20'000'000};
options.emission_limits = {10'000'000, 20'000'000};
auto result = yarda::analyze_streaming_hierarchy(lat, objects, hierarchy, options);
```

Producer callers use
`stream_resolved_task_accesses(lat, objects, sink, emission_budget, loop_limits)`.
The existing three- and four-argument overloads retain default loop limits.
Hierarchy emission defaults remain 1,000,000 sources and 10,000,000 source-to-L1
line references. These configurable settings are C++ APIs; CLI flags follow in B10.

All limits are inclusive. Zero permits no iterations or emissions for that
specific budget; it is never an unlimited sentinel. Zero-trip loops and flat
accesses do not consume loop work, while loops with empty bodies still do.
At each dynamic loop entry, the single-loop allowance is checked and the entire
trip count is reserved from one module budget before the body executes. For
example, a two-iteration outer loop with a three-iteration inner loop consumes
`2 + 2 * 3 = 8` cumulative iterations. Task boundaries reset cache state and
source ordinals, but do not reset cumulative loop/source/line allowances.

Raising source/line limits alone does not raise loop limits. Cross-line accesses
consume one line allowance per source-to-L1 reference; LLC forwarding is not
charged again. Node/depth guards, checked arithmetic and address validation
remain active. Any exhaustion fails the whole invocation, stops callbacks and
returns no partial result. Discard any previously collected sink events on
failure. Event truncation alone still permits complete analysis.

## Prepared loop execution

Task and legacy unrolling prepare each reached static LAT node once per subtree
traversal. Repeated execution reuses the loop body and integer variable slots,
including lexical shadowing, instead of copying JSON bodies and variable maps.
Supported index expressions keep their existing spelling and rejection rules;
address layout and ELF extent checks still run through the existing resolver.

Preparation follows execution order. Zero-trip bodies are not prepared, loop
work is reserved at every dynamic entry, and later malformed nodes cannot
preempt an earlier consumer exception. All preparation occurs inside the
analysis call. Prepared nodes borrow the immutable expanded input and are
released when traversal returns or fails. Their storage follows static nodes,
indices and loop slots, with no per-access history or cache shared between
analyses. Full-exact cache history remains a separate distinct-line cost.
