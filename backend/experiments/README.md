# Cache hierarchy evaluation (B12)

This optional C++ runner compares the materialized batch core with streaming
summaries on identical MAP, ET_EXEC and cache bytes. It uses the existing
`exact-two-level-lru-demand-v1`, `full-exact`, `linked_absolute` model, core 0,
and independently cold tasks. Scripts orchestrate compilation and processes;
they do not implement CSRD, residency or First-Hit Count calculations.

Current runs export [RESULT v2](../../docs/cache-hierarchy-artifacts-v2.md)
with glossary metric names. Historical B12 measurements retain their original
v1 artifacts and identities.

## Build and run

Linux `/proc`, a POSIX shell, Clang/LLVM 14 and the regular backend dependencies
are required. From the repository root:

```sh
cmake -S . -B build-evaluation -DCMAKE_BUILD_TYPE=Release \
  '-DCMAKE_CXX_FLAGS_RELEASE=-O2 -DNDEBUG' \
  -DYARDA_BUILD_REGION_FRONTEND=ON \
  -DYARDA_CLANG_INCLUDE_DIR=/usr/lib/llvm-14/include \
  -DYARDA_BUILD_HIERARCHY_EXPERIMENTS=ON
cmake --build build-evaluation --parallel 4
ctest --test-dir build-evaluation --output-on-failure
sh backend/experiments/run_cache_hierarchy_evaluation.sh \
  build-evaluation benchmark-results/hierarchy-evaluation
```

The output directory must not exist. Optional third/fourth arguments select a
manifest and repetition count. Fewer than ten repeats are smoke runs. Inputs
and binaries must remain unchanged throughout execution. Run measurements
without concurrent builds/tests. Experiments default OFF; enabling them adds
short GTests and C-to-result smoke/failure tests. CTest never runs the full
ten-repeat matrix or asserts performance thresholds.

## Workloads

| Family | References | Distinct L1 lines |
| --- | --- | --- |
| Fixed domain | 1,024; 16,384; 131,072; 1,048,576 | 16 |
| Growing domain | 131,072 | 64; 256; 1,024; 4,096 |
| Adapted ATAX | micro 2x3, MINI 38x42, SMALL 116x124 | checked from linked storage |
| Rejections | source budget exhaustion; dynamic loop bound | no RESULT |

Synthetic inputs use a 32-byte-aligned `double[DOMAIN][4]`. Each update emits a
load then a store; only the compact loop bound changes with repetition count.
The tiny cache geometry forces both histories to process repeated misses.
Growing-domain cases hold reference count fixed. The longest fixed-domain case
uses its exact inclusive source/line allowances.

ATAX adapts PolyBench/C 4.2.1 and the independently checked B11 region fixture.
MINI/SMALL dimensions match the original `atax.h`. Static arrays and explicit
linker sections replace the original allocation; literal bounds and B11's
compound assignments determine the fixed Clang 14 access order. Initialization,
printing and runtime instrumentation are excluded. The original program need
not have the same access order, addresses or cache behavior. Results cover only
the listed adapted kernel/datasets, not the PolyBench suite.

Reference-source SHA-256 (not a runtime dependency):

- `atax.c`: `b3925adb41b5efb545ee1c9278ee2776e4970fdec64e72398494a336d59facbb`
- `atax.h`: `b896f0a5fab49f4d52b1fb2ea7bcd7c4afabecb20bfee0deea271ec05b033761`

All MAP uses `clang14-o0-region-v1`. Companion ELF uses Clang 14 `-O0 -g
-fno-pie -no-pie` and recorded section addresses. Analyzer `-O2` is separate
from input `-O0`. Prepared case files contain paths, hashes, dimensions, scope,
compiler flags and all four effective work limits. Compiler preparation wall
seconds have one-second resolution and are excluded from analyzer timings.
`preparation_in_timing` refers to analyzer node/layout preparation, not compilation.

## Correctness and measurements

A separate `verify` process checks every source against fixture-derived
object/offset/width/operation expectations and fixed linked bases. It compares
every streaming event with batch, including CSRD, FSL and LLC provenance, then
compares RESULT. Actual `yarda_cpp` output and every successful measured RESULT
must have identical bytes. Existing independent-oracle E2E tests remain an
additional authority; producer sharing is not an independent source oracle.

Each sample freshly execs the same binary and runs exactly one path:

- `batch`: budgeted source collection and the existing batch core. The adapter
  validates line allowance with the mapper before the batch core maps again.
  This extra mapping and materialized trace/event destruction are timed.
- `streaming`: summaries with event sinks, telemetry and state measurement off.
- `instrumented`: summaries with task-boundary storage snapshots and compaction
  clocks. Compare separately with plain streaming to expose measurement cost.

`total_time_ns` includes case/input reading, hashing and RESULT dumping, ending
before diagnostic formatting and output publication. `analysis_time_ns`
includes all producer preparation, resolution, analysis and summary creation.
Input, analysis and serialization are disjoint intervals. Compaction is nested
within analysis and must not be added to it. Verify timings are not samples.

Primary RSS is Linux `/proc/self/status` VmHWM at completed analysis, normalized
to bytes: the current executable image's high-water mark including inputs and
allocator overhead. Process-lifetime `getrusage` RSS is also recorded because a
larger launcher before exec can affect it. Compiler processes are separate.
The shell cap limits virtual address space, not RSS.

Each case/path gets a warm-up followed by at least ten formal samples. Ordering
alternates batch/streaming/instrumented and its reverse. Reports contain median,
min, max and linearly interpolated IQR. Failed samples never enter distributions.
Groups reject mixed input identities or binary hashes, even if versions match.

## Evidence and interpretation

- `manifest.json`, `inputs/*/case.json`: scope, preparation and input identity.
- `provenance.json`, `sources.json`, build cache/commands: binaries, source and host.
- `runs/*/verify/`: independent source verification and actual CLI parity.
- `runs/*/{batch,streaming,instrumented}-*/`: RESULT, measurements and command logs.
- `samples.json`, `summary.json`: raw sample index and successful-only aggregates.

Success, unsupported input, budget exhaustion, timeout, resource failure and
unexpected error are distinct. Unexpected execution failures make the run fail
after preserving evidence; unexpected compiler failures stop preparation. Failed
analyses publish no RESULT. Requiring new output directories prevents stale
results from becoming successful samples. Measurements have an experiment
schema separate from public TELEMETRY and semantic RESULT.

`CsrdStatistics` reports actual task/level history and container counts.
Historical entries and touched sets never shrink, so final values also give
their task-local peaks. Fenwick capacity, hash buckets and maximum compaction
scratch are separate quantities, excluding allocator overhead. Completed
summaries and histograms also occupy memory. Discard every snapshot if a later
task or callback fails. `O(V)` describes retained analyzer state, not all process
allocations. See [the evaluation record](../../docs/cache-hierarchy-evaluation.md)
for measurements and claim criteria.
