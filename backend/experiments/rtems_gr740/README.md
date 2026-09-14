# RTEMS 6 / GR740 linked-input end-to-end evaluation

This experiment compiles the **same kernel C source** with the strict Clang 14
region frontend (`--target=sparc-unknown-rtems6`) and `sparc-rtems6-gcc` linked
against the GR740 BSP. The resulting input is a real big-endian SPARC ELF32
`ET_EXEC`, with `Init`, RTEMS startup and live kernel objects. GCC uses the
installed BSP's `-mcpu=leon3`, `-mfpu -mhard-float`, `-O0 -g` options, following
`/workspace/experiments/common.mk`. Host analyzer optimization is separately
fixed at `-O2 -DNDEBUG`.

## Reproduce

From the YARDA root, with LLVM/Clang 14, the normal C++ dependencies and the
RTEMS 6 GR740 toolchain installed at `/opt/rtems/6`:

```sh
cmake -S . -B /tmp/yarda-rtems-build -DCMAKE_BUILD_TYPE=Release \
  '-DCMAKE_CXX_FLAGS_RELEASE=-O2 -DNDEBUG' \
  -DYARDA_BUILD_REGION_FRONTEND=ON \
  -DYARDA_CLANG_INCLUDE_DIR=/usr/lib/llvm-14/include \
  -DYARDA_BUILD_HIERARCHY_EXPERIMENTS=ON
cmake --build /tmp/yarda-rtems-build --parallel 4
ctest --test-dir /tmp/yarda-rtems-build --output-on-failure --parallel 4
python3 backend/experiments/rtems_gr740/run.py /tmp/yarda-rtems-build \
  benchmark-results/rtems-gr740
```

Output must be new. `--smoke --repeats 1` runs three micro cases and the rejection
matrix. Full evaluation uses 16 cases, one warm-up per mode, and ten independent
processes per case/mode (480 measured samples). Batch, streaming and instrumented
order alternates each round. Run measurements after builds/tests have finished.
`BUILD_TESTING=ON` and the experiments option build the independent verifier.
CTest adds a cross-toolchain smoke test when `sparc-rtems6-gcc` is discoverable.

To attempt RTEMS execution as well, append
`--simulator /path/to/laysim-gr740-cli`. Every image's `Init` validates one warm-up
and ten jobs, printing `YARDA_RTEMS_COMPLETE` only after all numerical checks pass.
The runner preserves the first simulator failure in `target-runtime.json` and
continues host-side analysis. Simulator availability is a separate result from
successful static e2e analysis; absence of target execution is never a runtime pass.
An individual image can also be built/run with the provided Makefile.

## Workloads and evidence

| Kernel | MICRO | MINI | SMALL | MEDIUM | Additional |
| --- | --- | --- | --- | --- | --- |
| ATAX | M=2, N=3 | 38, 42 | 116, 124 | 390, 410 | — |
| BiCG | M=2, N=3 | 38, 42 | 116, 124 | 390, 410 | — |
| MVT | N=3 | 40 | 120 | 400 | Custom N=520, working set >2 MiB |
| Line sweep | — | — | — | — | 4,096 lines; 1, 16, 128 sweeps |

ATAX/BiCG stream a matrix while updating vectors; MVT includes a transposed
matrix traversal. These kernels emphasize memory access and have low arithmetic
intensity. This is not proof that an unmeasured GR740 execution is bandwidth-bound.
The sweep holds the historical line domain fixed while increasing references.
`cases.py` is the executable matrix, with exact inclusive source/line/loop limits.

PolyBench/C 4.2.1 MINI/SMALL/MEDIUM dimensions and arithmetic are retained.
Static 32-byte-aligned arrays replace heap/parameter storage. Initialization uses
`A[i][j] = i + 2*j + 1` and unit vectors, so row/column traversal differs and every
live output has an exact closed-form reference below double's integer limit. Initialization,
output validation, RTEMS calls, stacks and compiler spills lie outside the selected
source region. Names `y_1/y_2` become `y1/y2` in MVT. The PolyBench license is
included in `POLYBENCH-LICENSE.txt`. This does not cover the full PolyBench suite.

Every case is rebuilt twice; both ELF and LAT must be byte-identical. GNU `nm`
supplies independently parsed symbol bases and sizes. C-derived nested-loop
expectations verify every source's object, offset, width, operation and order.
All sizes compare every first-hit event with the existing explicit resident-LRU
oracle and every batch/streaming event (including exact CSRD) with each other.
Micro cases additionally use the existing quadratic exact-distance oracle.
The actual public CLI result must equal independently sourced batch results.
Repeat/diagnostic runs and all measured results must have identical RESULT bytes.

All four budgets are checked at their inclusive success boundary and one below;
failed evaluators publish no RESULT and failed CLI runs preserve existing output.
Additional gates cover missing SPARC ELF symbols, dynamic loop bounds, rejection
of physical no-write-allocation, and a deliberately mutated LAT operation that
must fail the independent source oracle even though the CLI accepts it.

Artifacts include prepared inputs, rebuilds, ELF headers/maps/disassembly, GNU nm
output, commands and exit codes, source/binary/BSP hashes, copied executable tools,
raw samples and C++-aggregated median/min/max/IQR. `completion.json` is written
only after all static gates and measurement checks pass. A source/input/binary
change during a run invalidates it. Host numerical checks validate the C program;
they are not SPARC execution or instruction-trace validation.

## Hardware model boundary

[GR740 documentation](https://www.gaisler.com/doc/gr740/GR740-UM-DS.pdf), §6.3,
describes a 16 KiB, four-way L1 data cache with 32-byte lines and **write-through,
no allocation on a store miss**. The
[board quick start guide](https://www.gaisler.com/doc/gr740/qsg_gr740.pdf)
also lists a four-way, 2 MiB L2 cache. `cache-model.yaml` uses those geometries
with YARDA's `exact-two-level-lru-demand-v1` assumptions: equal line sizes, LRU,
allocation on all demand misses, and only L1 misses propagated to LLC.

Consequently the model's FHC/AMC/CSRD are **not physical GR740 cache counters**.
In particular, the current analyzer rejects `write_allocate: false`; silently
using a geometry-only match as hardware parity would be incorrect. There is no
instruction fetch, RTEMS scheduling traffic, multicore interference, cache timing
calibration or GCC instruction-order equivalence in this experiment. The source
access order is Clang's selected `-O0` region order bound to GCC's real linked
global addresses. Target timings, when available, must be reported separately
from host analyzer timings and may start from warm cache state.
