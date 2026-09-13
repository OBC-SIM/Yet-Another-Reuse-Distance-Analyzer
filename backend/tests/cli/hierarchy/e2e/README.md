# Generated hierarchy CLI validation

Build from the repository root, then run:

```sh
cmake --build build
ctest --test-dir build -R yarda_cpp_hierarchy_e2e --output-on-failure
```

Three tests run with either frontend configuration.
Each test generates its own LAT/ET_EXEC inputs before invoking `yarda_cpp`.
Inputs, outputs and `commands.log` stay in the test's build directory.

| Test | Evidence |
| --- | --- |
| `positive` | Fixed addresses, hand CSRD/FSL/counts, cross-line source provenance, independent cold/empty tasks |
| `affine` | Existing H2-B fixture: 25 tasks / 95 sources in both debug-information modes |
| `tasks` | The three supported `tasks/*.c` entries in `fixtures/tasks.json` |

The legacy producer uses Clang 14 O0 with `optnone` disabled, followed by
`function(mem2reg),loop-simplify,loop-annotated-trace`. ELF generation uses
the same source, target and preprocessing flags, with `-fno-pie -no-pie`.

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
