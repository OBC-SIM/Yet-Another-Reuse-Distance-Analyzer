# Analysis regions v1 — R1 contract

Accepted for R2 implementation on 2026-09-09. R1's boundary experiment is
implemented; production region extraction, task integration and PolyBench
validation are **not implemented**. The existing product still selects function
tasks. This document defines the input-selection extension to
[the cache hierarchy model](cache-hierarchy-rd-model-v1.md), not a new cache model.
The M1–M4 review follow-up on 2026-09-10 clarifies the pipeline gate,
normalization descriptors and external-global access semantics below.

## 1. Source selection

The first version accepts literal, argument-free `#pragma APE_ANALYZE_BEGIN` and
`#pragma APE_ANALYZE_END` directives in C source, following the uppercase snake
case of `APE_ANALYZE` and `APE_INLINE`. The directives must surround consecutive
complete statements at the top level of the same function body. There is at most
one pair per function; different functions may each have a pair. The fixed region
name is `APE_ANALYZE`.

| Function declarations | Selected task |
| --- | --- |
| `APE_ANALYZE`, no region | Existing whole-function task |
| Valid region, no annotation | One region task; frontend adds `ape.analyze` to LAT |
| `APE_ANALYZE` and valid region | One region task; no whole-function duplicate |
| `APE_INLINE`, no region | Existing inline helper; not an independent root |
| `APE_INLINE` and any region | Reject, including the combination with `APE_ANALYZE` |
| No annotation and no region | Preserve existing frontend/legacy behavior; no new hierarchy root |

Region selection is automatic from the pair in the region-enabled frontend. No
backend region-selection flag, custom pragma spelling or public marker macro is
introduced. Whole-function annotations in the same module remain selectable.

The initial source domain is C11, main-file directives, straight-line accesses
and complete finite `for` loops, including complete nested loops. Empty regions
are valid tasks. Constant values defined outside a region remain available for
bound, index and address resolution. Scope selection does not delete outside
statements before compilation or memory-to-register promotion.

Reject missing, reversed, duplicate, nested or cross-function pairs; pragmas in
headers, `_Pragma`/macro-generated boundaries, directives with arguments,
boundaries inside loops or expressions, and partial statements/loops. Reject
conditional/path selection, `switch`, `goto`/labels, `break`, `continue`, or
`return` inside the region, and control flow that can bypass or reenter a region
boundary. A normal function return after the region is permitted. Inline assembly
and custom assembler symbol labels are outside the initial source domain.

AST validation and LLVM dominance/post-dominance checks are complementary.
Dominance alone does not prove that a boundary is at a source statement boundary.
Unsupported selected accesses/calls and unresolved external dependencies fail the
whole analysis. Existing address and finite-loop restrictions still apply.

## 2. Compiler boundary

The only initial region pipeline is identified as `clang14-o0-region-v1`:

```text
C11 + preprocessing flags
  -> Clang 14 at O0, debug information, O0 optnone disabled
  -> registered C++ APE_ANALYZE_BEGIN/APE_ANALYZE_END pragma handlers
  -> C AST scope validation and expected-region manifest
  -> fresh LLVM IR in the same frontend invocation
  -> validate each expected pair, record access/loop selection, erase markers
  -> function(mem2reg),loop-simplify
  -> validate selected complete loop/access structure and export APE/LAT
```

Use `__builtin_annotation(0, "yarda.region.begin.v1")` and the corresponding
`yarda.region.end.v1` only as private transport inserted by the pragma handler.
Capture these `llvm.annotation` calls **before optimization**, and erase them
before normalizing or building LAT. They are not source accesses or opaque calls.
Reject direct user use of the reserved transport names. Preserve a function-level
selection descriptor independently of memory instructions so empty selections
remain identifiable. The R1 prototype uses `yarda.region` metadata/attributes;
these are private transport, not a public input format.

Instruction membership excludes blocks unreachable from function entry. It is a
snapshot before normalization, not a complete classification of all instructions
after it. In the frozen R1 Clang/LLVM 14.0.0 experiment, `mem2reg` creates an
untagged induction `phi`; loop normalization can introduce control-flow
instructions. Missing tags on these new scalar/control instructions do not by
themselves mean that a selected loop or access was lost.

R2 must record selected complete-loop headers and expected LAT memory/call sites
before removing markers, alongside the source manifest and empty-region
descriptor. After normalization, resolve those headers in the rebuilt `LoopInfo`
and construct loop bounds/indices from the whole function, including untagged
`phi` nodes and outside value definitions. Do not require every instruction or
operand of a selected loop to carry a region tag. Reject missing or inconsistent
header identities and lost, additional or misclassified LAT memory/call sites;
ordinary local-scalar promotion and new scalar/control instructions are permitted.
This descriptor validation is R2 work; R1 observes the untagged `phi` while
checking preserved global accesses and whole-function loop reconstruction.
R1's assertion that the new phi has no tag records that experiment's behavior.
It is not an R2 semantic requirement: R2 may propagate tags, and its tests must
validate selected loop/access semantics without requiring tag absence.

R2 must provide a C++ frontend executable, `yarda_region_lat`, using a Clang
`EmitLLVMOnlyAction` and the shared LLVM LAT builder. It owns compilation, capture,
the fixed normalization sequence and export. Region IR is consumed in memory;
arbitrary `.ll` input and user-defined optimization pipelines are not accepted by
this entry point. The existing LLVM plugin remains the legacy function frontend
and must reject recognizable region transport instead of interpreting it as a
whole function. A previously discarded pragma cannot be recovered from plain IR.

The source-side manifest must match the generated region functions and pairs,
including empty scopes. Any missing, extra or inconsistent boundary fails before
LAT output. Validate all source/IR scopes before opening the output file. Merely
counting surviving marker calls is insufficient when every call can be lost.

Reject input `-O1`, `-O2`, `-O3`, size/fast optimization variants, LTO, compiler/pass
plugins, sanitizers, coverage/profiling instrumentation and alternative pass
pipelines. R2 must use an explicit allowlist for supported preprocessing/driver
arguments instead of forwarding arbitrary flags to Clang. R1's optional argument
accepts only known optimization test options: `-O0`, `-O1`, `-O2`, `-O3`, `-Os`,
`-Oz`, `-Ofast`, `-flto`, `-flto=full`, `-flto=thin`. Only `-O0` succeeds; the
others exercise the compiler's optimization/LTO guard. Every other optional
argument is rejected before entering Clang.
The supported evaluation uses the same canonical C-to-LAT path;
an **O2 build of the analyzer itself is permitted** and recorded separately.
Compile the ET_EXEC ELF from the same original C, target/data layout, preprocessing
flags and object definitions, without analysis markers. The model uses that ELF's
actual global addresses; it does not promise a machine-execution O2 access trace.

Evidence in the [standalone R1 experiment](../frontend/experiments/analysis_regions/README.md):

- Native Clang discards the two pragmas while preserving `ape.analyze`.
- Pragma injection, capture and fixed normalization preserve the fixture's
  memory accesses, constant bound and external index offset. Removing debug
  information does not change instruction membership.
- Input O2 merges a selected load with an earlier outside load. Applying O2
  after capture also loses the selected load despite instruction metadata.
- Ordinary opaque marker calls prevent that load merging, changing the
  optimized visible-memory access count themselves.
- The in-memory C++ prototype rejects optimization/LTO and arbitrary additional
  options, including pass plugins and instrumentation, before producing output.
- Normalization creates an untagged induction `phi`; the original loop remains
  reconstructible with the expected bound/index using the whole function.
- The global-value fixture preserves an outside-loaded value without selecting
  that load, and selects a separate global load performed inside the region.

These observations establish a feasible restricted pipeline, not preservation
under arbitrary compiler transformations or completion of R2's AST validator.

## 3. LAT and task identity

Retain APE/LAT `schema_version: 2`. A region root has the existing function entry
plus exactly this optional extension:

```json
{
  "function": "region_probe",
  "params": [],
  "annotations": ["ape.analyze"],
  "analysis_scope": {"kind": "region", "name": "APE_ANALYZE"},
  "body": []
}
```

Here `body` stands for the **selected** statements; it is empty only for an empty
selection. Absence of `analysis_scope` retains whole-function semantics. Reject
null/malformed scopes, unknown keys, kinds or names, and scopes on inline or
unselected entries. Consumers predating this extension are not supported region
consumers; keeping the v2 envelope is not a claim of backward reader support.

The `function` field remains the original LLVM function name. Preserve `params`,
`arg_objects`, canonical object IDs, layout metadata and original function scope
through inline expansion. Do not invent a renamed function to represent a task.
Region roots cannot be inline helpers. Non-inline calls retain existing opaque
call behavior and fail hierarchy analysis if they occur in a selected task.
Legacy whole-module expansion/unroll must explicitly reject region LAT rather
than expanding a selected body as the complete callee body.

Derive the result/event task ID after preserving the original function binding:

- Function task: the existing function name, unchanged.
- Region task: `region:<N>:<function>:APE_ANALYZE`, where `N` is the decimal UTF-8 byte
  length of the function name, without leading zeros.
- Example: `region:12:region_probe:APE_ANALYZE`.

Validate nonempty and unique function identities and final task IDs. If a literal
legacy function name equals a generated region ID, reject that mixed module;
never overwrite, merge or silently rename either task. Length-prefix encoding
avoids ambiguity between generated IDs without changing existing function IDs.

No public `TaskAccessSink` or B8 cache API change is needed: emit the derived task
ID through the existing callbacks. Task-local source ordinals restart at zero,
and line-span ordinals and all address/operation provenance are preserved.

The LAT already records selection in `analysis_scope` and the selected body.
Its raw hash and the task ID identify the selection in B9; no additional region
hash, repeated RESULT scope object or separate selection option is required.

## 4. Selected access semantics and expected fixture

Each selected region starts with cold L1, LLC and full-exact history. Outside
initialization/cleanup accesses contribute neither references nor warm state.
Empty regions remain tasks with zero counts, empty histograms and null ratios.
Multiple functions' regions remain independent, even when addresses overlap.

References follow retained supported load/store operations in the fixed canonical
IR and their selected execution order. An object's definition outside the region
does not exempt a load performed inside it, including a load used in an index or
bound expression. Conversely, using an SSA value loaded before the region does
not select that earlier load or create an extra reference. Loads eliminated by
constant folding or permitted local-scalar promotion produce no references.

Source availability of a global initializer or its ELF address is not proof of
the global's value when the function runs. The initial version adds no runtime
global-value propagation. R2 must reject runtime-loaded loop bounds and unresolved
indices before LAT output; it must not omit their loads, flatten an unknown loop
or report zero-count success to force an otherwise unsupported input through.

For `fixtures/boundary.c`, the independent selected source sequence is:

| Source ordinal | Operation | Object | Element / byte offset |
| --- | --- | --- | --- |
| 0 | load | `global::inside` | 1 / 4 |
| 1 | store | `global::inside` | 1 / 4 |
| 2 | load | `global::inside` | 2 / 8 |
| 3 | store | `global::inside` | 2 / 8 |
| 4 | load | `global::inside` | 3 / 12 |
| 5 | store | `global::inside` | 3 / 12 |

Each access has width 4 on the recorded x86-64 fixture target. The outside stores
to `before[0]` and `after[0]` are excluded. R1 verifies IR membership and the
unchanged whole-function LAT's loop `start=0`, `bound=3`, `step=1`, `index=i+1`.
**R2 must still verify this selected dynamic sequence through the task producer
and ELF resolver**, plus batch/streaming and independent cache oracles.

For `fixtures/global_values.c`, the recorded Clang/LLVM 14.0.0 pipeline folds the
global `const` bound/offset to 3/1 and produces no loads in canonical IR. A
file-scope `const int` is not a C integer constant expression; this folding is
observed compiler behavior, not a C language guarantee. The sequence below is
an expectation for that fixed pipeline and target and must be revalidated if
the toolchain changes. An outside load of `external_value` supplies
`cached`, which remains available inside the loop but contributes no selected
reference. Each iteration separately loads `external_value` into `sampled`.
For `k = 0, 1, 2`, the selected sequence is exactly:

| Source ordinal | Operation | Object | Element / byte offset |
| --- | --- | --- | --- |
| `3k` | load | `global::external_value` | 0 / 0 |
| `3k+1` | load | `global::inside` | `k+1` / `4(k+1)` |
| `3k+2` | store | `global::inside` | `k+1` / `4(k+1)` |

All nine accesses have width 4 on the recorded target. The outside read and
`before`/`after` stores are excluded. R1 verifies membership, operation order,
bound/index and whole-function LAT parity; R2 must verify the nine dynamic events
through the selected-task producer and ELF resolver.

`fixtures/dynamic_bound.c` instead reads an unresolved global loop bound. R1 only
observes that this load is inside the region and emits experimental IR. It does
not export LAT for this fixture. R2 must use it as a negative fixture and reject
the whole analysis before LAT output, even though boundary capture succeeds.

The region feature does not lift emission/loop limits, add stack/heap/TLS support,
resolve dynamic bounds, change full-exact CSRD, or validate PolyBench-scale time
and memory. Those remain the separate R2/L/P1/P2/B11/B12 gates.
