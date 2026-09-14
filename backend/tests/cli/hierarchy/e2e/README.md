# Generated hierarchy CLI validation

Build from the repository root, then run:

```sh
cmake --build build
ctest --test-dir build -R yarda_cpp_hierarchy_e2e --output-on-failure
```

Seven tests run with the default frontend. Enabling
`YARDA_BUILD_REGION_FRONTEND=ON` adds `regions` and `region_rejections`.
Each test generates its own LAT/ET_EXEC inputs before invoking `yarda_cpp`.
Inputs, outputs and `commands.log` stay in the test's build directory.
Each new case removes its previous RESULT/EVENTS/TELEMETRY before execution.
Negative cases separately verify absent outputs and preservation of old files.

| Test | Evidence |
| --- | --- |
| `positive` | Fixed addresses, hand CSRD/FSL/counts, cross-line source provenance, independent cold/empty tasks |
| `affine` | Existing H2-B fixture: 25 tasks / 95 sources in both debug-information modes |
| `affine_rejections` | Unsupported source expressions fail before LAT publication |
| `diagnostics` | RESULT byte determinism, independent input hashes/ID, bounded EVENTS, separate TELEMETRY |
| `limits` | Exact/higher allowances, each lower allowance, cumulative tasks, cross-line charges, empty-loop and structural expansion guards |
| `rejections` | Invalid inputs/cache/output, storage/index failures, skipped-body and error-order regression; Unix file-write failure |
| `tasks` | The three supported `tasks/*.c` entries in `fixtures/tasks.json` |
| `regions` | Whole-function/region parity, outside exclusion, inline pointer binding, external global values, fixed 2-by-3 static ATAX |
| `region_rejections` | Source selection/pipeline restrictions, malformed LAT scope, legacy unroll rejection, retained opaque calls |

The legacy producer uses Clang 14 O0 with `optnone` disabled, followed by
`function(mem2reg),loop-simplify,loop-annotated-trace`. Region cases use
`yarda_region_lat`'s fixed `clang14-o0-region-v1` pipeline. ELF generation uses
the same source, target and preprocessing flags, with `-fno-pie -no-pie`.
Only the ELF compiler ignores the region pragmas.

The legacy producer intentionally omits non-inline calls; its LAT alone cannot
prove their absence in the source. Opaque-call rejection is consequently tested
with the strict frontend, which preserves these call sites in function and
region tasks. Legacy affine support does not extend strict region index syntax;
scaled and runtime-parameter indices remain rejected by that frontend.

`fixtures/*.json` source sequences are specified from C independently of LAT.
The oracle GTest runner first checks actual resolved sources against those
sequences, including object/offset/width/operation/ordinal. It then feeds the
independently specified sources to the existing exact-distance and resident-LRU
oracles, and compares published RESULT/EVENTS bytes against batch serialization.
No actual LAT-derived source stream supplies the oracle's expected trace.
For non-fixed globals only the ELF symbol base is shared; expected offsets are
independent. `hierarchy.json` additionally fixes ELF bases, and
`observations.json` fixes every hand-trace address/set/tag/CSRD/outcome/FSL.
The shell/CMake drivers contain no cache simulation or reuse-distance algorithm.

In `hierarchy.c`, `lines` starts at `0x600000`; offsets 0, 64, 128, 256, 384
conflict at L1, while offset 32 is other-set noise. The finite L1 distances
include 3 and 4 despite two-way associativity. `crossing` starts at `0x60021c`:
each 8-byte access emits references at offsets 28 and 0 of consecutive lines.
The hand totals are 14 source accesses and 16 L1 references, with first-service
counts L1=4, LLC=2 and Memory=10 across four independent tasks.

Marker loss and source/IR descriptor mismatches remain covered by the existing
`YardaRegionIrTests` and `YardaRegionFrontendTests`, run by the full CTest gate.
Handled rename/rollback behavior remains covered by `yarda_cli_tests`; the
generated rejection test additionally forces an actual write error using the
existing file-size-limit launcher. Crash/concurrent-writer transactions are
outside the publication contract.

Compare bytes only within the same binary, raw inputs and semantic options.
Function/region scope changes and different compiler/debug inputs change
identity; compare their expected semantic fields instead. TELEMETRY durations
are not golden values. Timing/RSS checks here establish the artifact contract,
not B12 performance evidence or a PolyBench suite-support claim.
