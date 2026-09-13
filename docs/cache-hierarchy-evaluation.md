# Cache hierarchy evaluation — B12

The streaming backend meets the documented lightweight criteria on this bounded
prepared-input workload matrix. This result concerns the analyzer process; C
compilation and original PolyBench execution are outside the timed interval.

## Reproduction and identity

The [runner README](../backend/experiments/README.md) defines commands, workloads,
address changes, work limits, timing/RSS boundaries and failure handling.
The [local evidence archive](../benchmark-results/yarda-b12-hfsinnac/README.md)
contains raw samples, logs, source/binary hashes, source changes and heap profiles.
The archive is intentionally Git-ignored and is not included in a fresh clone.

- Parent baseline: `55bed3e19a6abb01a29a897b8e55e18ea627f9b3` plus the B12 changes.
- Frontend: `3cc6f9276916693c915b20aa412966c2c35bf101`.
- Evaluator SHA-256: `16645a33c26da966514385889f58aa3531c656b9e93f4dfd20b73a5ffcd308ad`.
- GNU C++ 11.4, Release `-O2 -DNDEBUG`; input pipeline Clang/LLVM 14, O0.
- Linux, AMD Ryzen 9 9900X3D. CPU affinity/governor were not controlled;
  the governor interface was unavailable. Initial load averages: 0.38/0.89/1.24.
- One warm-up and ten independent samples per case/path, alternating order.
- 11 successful cases × 3 paths × 10 = **330 successful samples**.
  Three source-budget failures and one frontend rejection are separate records.
- All 33 successful groups have ten samples. Binary hashes, 160 captured source
  hashes and generated LAT/ELF/cache hashes were rechecked after measurement.

## Runtime and process memory

Values below are medians. Time includes input reading/hashing (including the
evaluator binary hash), analyzer preparation, analysis and RESULT dumping.
Peak RSS is current-image Linux VmHWM in MiB. Full min/max/IQR and analysis-only
intervals are in [CSV](../benchmark-results/yarda-b12-hfsinnac/evaluation.csv) and
[raw aggregate JSON](../benchmark-results/yarda-b12-hfsinnac/evaluation-final/summary.json).

| Case | References | Batch ms | Streaming ms | Batch MiB | Streaming MiB |
| --- | ---: | ---: | ---: | ---: | ---: |
| fixed-v16-n1024 | 1,024 | 44.609 | 43.660 | 8.438 | 7.969 |
| fixed-v16-n16384 | 16,384 | 49.309 | 45.065 | 18.787 | 7.875 |
| fixed-v16-n131072 | 131,072 | 101.050 | 56.404 | 96.859 | 7.969 |
| fixed-v16-n1048576 | 1,048,576 | 687.329 | 146.588 | 685.762 | 7.875 |
| growing-v64-n131072 | 131,072 | 99.913 | 57.165 | 96.859 | 8.062 |
| growing-v256-n131072 | 131,072 | 102.105 | 60.035 | 97.143 | 7.875 |
| growing-v1024-n131072 | 131,072 | 103.761 | 59.627 | 97.232 | 7.875 |
| growing-v4096-n131072 | 131,072 | 106.080 | 61.288 | 97.684 | 8.438 |
| atax-micro | 53 | 41.346 | 41.048 | 7.875 | 7.969 |
| atax-mini | 12,848 | 46.085 | 41.979 | 15.484 | 8.250 |
| atax-small | 115,312 | 96.222 | 55.286 | 77.006 | 9.938 |

The longest fixed-domain case reduces median RSS by **98.85%**
and total median runtime by **4.69×** relative to the budgeted batch adapter.
Small cases are dominated by input/hash costs and process-page variation: the
ATAX micro streaming RSS is slightly higher than batch. Timing changes within
observed IQR should not be interpreted as consistent improvements.

The prepared static/global ATAX MINI and SMALL cases complete all analysis,
with complete coverage and passing invariants. The source operations and
addresses are checked independently for every source, not just by source count.
These adapted storage/layout results do not establish original-suite cache
behavior or support for other kernels/datasets.

## Retained state and allocation evidence

![Measured process RSS and retained state scaling](../benchmark-results/yarda-b12-hfsinnac/scaling.svg)

At fixed V=16, increasing references from 1,024 to 1,048,576 keeps each level at
16 history entries. L1 retains 34 allocated Fenwick elements and LLC 68 at every
scale; histogram key counts remain 2 and 1. When V grows to 4,096 at fixed
131,072 references, each history has 4,096 entries, with 8,194/8,196 allocated
Fenwick elements. Per-touched-set floors explain the capacity constants.

Massif provides separate allocation observations on the final binary:

| Case/path | Peak heap bytes |
| --- | ---: |
| fixed-v16-n1024-streaming | 139,426 |
| fixed-v16-n131072-batch | 86,085,511 |
| fixed-v16-n131072-streaming | 139,480 |
| growing-v4096-n131072-streaming | 575,795 |

The batch profile identifies full `HierarchyAccessEvent` and mapped/resolved
vector allocations. Streaming profiles contain no such allocation frames.
This observation is paired with the source ownership/call-path audit: streaming
uses synchronous source/line callbacks and an empty event sink, retaining task
summaries and distinct-line histories. Massif heap counts are not process RSS;
profiling runs are excluded from native timing/RSS samples.

## Compaction and measurement overhead

For the longest fixed-domain case, instrumented total time is 152.272 ms
versus 146.588 ms plain streaming (3.88% increase).
It performs 174,756 compactions with 11.279 ms median measured
compaction time. This interval includes rebuilt-tree/sorting allocations and
destruction, is nested inside analysis, and is not added to total time.
Instrumented and uninstrumented samples are always reported separately.

## Validation and claim gate

| Check | Result |
| --- | --- |
| Immutable B11 baseline, OFF Release / ON Debug | 885 / 896 CTest passed |
| B12 OFF Release | 893 CTest passed |
| B12 ON Debug / ON O2 Release, experiments enabled | 920 / 920 CTest passed |
| Direct backend / hierarchy / config / CLI GTest | 346 / 430 / 19 / 29 passed |
| New evaluator GTest | 14 passed |
| State and evaluator GTest Valgrind | 8 / 14 passed, zero memory errors |
| Generated instrumented synthetic / ATAX / budget failure Valgrind | zero errors and lost bytes |
| Independent source checks, all-event batch/streaming parity, actual CLI parity | 11/11 cases passed |
| RESULT bytes across all native measured paths | 330/330 matched verified baselines |
| RSS launcher isolation | 256 MiB prior allocation excluded by VmHWM |

The LLVM command-line registry retains its existing 2,574 bytes in 34 reachable
blocks in evaluator/CLI-linked processes; definite, indirect and possible lost
bytes are zero. Core state tests release all heap allocations.

All six gates are satisfied for the documented matrix: semantic parity, no
full trace/event materialization on summaries, fixed-V state independent of N,
O(V)-consistent domain scaling, repeatable long-trace RSS reduction, and public
runtime/compaction costs. The retained-state contract remains O(V), including
historical evicted lines. P1/P2 historical five-sample measurements are not part
of this ten-sample evaluation.
