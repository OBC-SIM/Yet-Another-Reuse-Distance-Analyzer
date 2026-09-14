# Cache Hierarchy Reuse-Distance Model v1

## 1. Status and authority

This document is the repository authority for the cache hierarchy analysis
model identified by `exact-two-level-lru-demand-v1`. Implementations and
serialized results that use this model identifier must follow this contract.

The model is an abstract, deterministic demand-cache model. It is not a
cycle-accurate hardware simulation and does not claim to reproduce a specific
processor's cache policy.

R1's [analysis-region contract](analysis-regions-v1.md) was fixed on 2026-09-09
with a standalone compiler-boundary experiment. Production region extraction
and task integration remain R2 work; the current implementation supports the
existing function roots. The input extension below does not change cache semantics.

## 2. Model identity

| Field | Required value |
| --- | --- |
| Model name | `Exact Two-Level LRU Demand Model` |
| `model_id` | `exact-two-level-lru-demand-v1` |
| `csrd_mode` | `full-exact` |
| `address_basis` | `linked_absolute` |
| `analysis_core_id` | `0` |

Changing any semantic field above defines a different analysis model or result
identity. A result produced under another model must not be combined with this
model as though their counters had the same meaning.

## 3. Required inputs

One analysis consumes:

1. an APE/LAT schema-v2 JSON module;
2. an ET_EXEC ELF image that supplies absolute linked object addresses; and
3. a cache-hierarchy schema-v1 YAML configuration.

The address used for mapping is the ELF-linked byte address. The model does not
describe it as a physical address unless a separate platform transformation has
been validated.

### 3.1 Supported access domain

The model supports accesses for which the analyzer can determine all of the
following statically:

- a canonical global object with complete metadata;
- an unambiguous ELF object symbol and absolute linked base address;
- an object-relative byte offset and positive access size;
- a load or store operation;
- a finite expansion of an `APE_ANALYZE` root (or the selected region root
  specified in section 8.1) and any supported `APE_INLINE` helpers; and
- cache-line references within the configured expansion limits.

Global arrays, scalars, and structured or static global objects are supported
when they meet those conditions.

### 3.2 Unsupported inputs

The hierarchy analysis must reject, rather than partially analyze:

- PIE or an ET_DYN image without an explicit supported load-bias model;
- stack, TLS, or heap objects;
- a runtime-dependent pointer target, byte offset, or final index;
- a missing or ambiguous ELF symbol;
- an unknown call target or recursive inline expansion;
- malformed input, incomplete resolution, or incomplete access coverage; and
- expansion, address, or counter overflow.

Known opaque call sites are not assumed to be memory-access-free. Until such a
property is represented and validated explicitly, any task set with
`excluded_opaque_call_sites > 0` is unsupported by this model.

## 4. Selected hierarchy

The analyzer selects exactly this path from the validated configuration:

```text
core 0
  -> private L1 data cache
  -> shared LLC
  -> configured Memory
```

The selected path must satisfy all of these conditions:

- core 0 maps to a cache with `role: L1` and `private_to: 0`;
- the L1 `next` target is one cache with `role: LLC` and no `private_to`;
- the LLC `next` target is the configured terminal Memory;
- the path contains exactly the L1 and LLC cache levels;
- both cache geometries are positive and valid power-of-two geometries;
- both levels use `replacement: LRU`;
- both levels have the same cache-line size; and
- both levels have `write_allocate: true`.

Valid caches outside the selected core-0 path are permitted but do not
participate in analysis. The result records the selected core ID and the
configured L1, LLC, and Memory names.

The equal-line-size restriction is part of this model. A 32-byte line is a
supported experiment configuration, not a hard-coded model constant; equal
64-byte lines, for example, are also valid.

Configured write-policy and delay values do not affect CSRD, residency,
first-service classification, or hit/miss counts. They are omitted from the
effective modeled hierarchy, while the raw configuration hash preserves input
provenance.

## 5. Analysis reference and mapping

The accounting unit is one cache-line reference, not one source instruction,
source access, object, byte, or distinct address.

A source access that crosses a cache-line boundary produces one reference for
each touched line in increasing linked-address order. Every reference retains:

- `task_id`;
- task-local `source_access_ordinal`;
- zero-based `line_span_ordinal`;
- canonical object identity;
- linked address and source byte range; and
- load/store operation.

Each cache level maps the original linked address using its own geometry. An L1
set or tag is never reused as the LLC set or tag. With equal line sizes, one L1
miss reference corresponds to exactly one LLC input reference.

## 6. Full exact Cache-Set Reuse Distance

For a reference to cache block `b` at one cache level, Cache-Set Reuse Distance
(CSRD) is defined as follows:

- if `b` has never appeared earlier in that level's input stream, the reference
  is a Cold Miss and has no finite CSRD;
- otherwise, CSRD is the number of distinct cache blocks mapped to the same set
  that appeared after the previous reference to `b` and before the current
  reference.

For associativity `W`, the outcome is:

```text
CSRD < W   -> Hit
CSRD >= W  -> Replacement Miss
```

`full-exact` requires the exact finite distance for every reused reference and
the complete finite-distance histogram. Distances at or above associativity are
not collapsed into a `>= W` bucket. For example, a distance of 17 is retained as
17 even when `W` is smaller.

An implementation must retain enough last-position and recency history to
distinguish an evicted line from a line never seen before. Resident lines alone
are insufficient for this contract.

## 7. Demand and hierarchy transitions

Every cache-line reference first looks up the L1. The transition is determined
only by cache residency and exact LRU recency:

| L1 outcome | LLC action | Allocation order | First-Service Level |
| --- | --- | --- | --- |
| Hit | No lookup | None | L1 |
| Miss, LLC hit | Lookup and touch | Fill L1 after LLC service | LLC |
| Miss, LLC miss | Lookup and fill | Fill LLC, then fill L1 | Memory |

Load and store references have identical residency semantics. The operation is
retained as provenance but does not change lookup, allocation, or eviction.

L1 and LLC maintain independent cache-level state without enforced inclusion
or exclusion:

- an L1 eviction does not touch the LLC or insert a victim there;
- an LLC eviction does not invalidate a matching L1 line; and
- only an L1 miss generates an LLC request.

## 8. Task state and coverage

One selected root is one independent task. Without the region extension in
section 8.1, the root is the existing whole `APE_ANALYZE` function. At each task start, the analyzer
creates cold L1, LLC, and CSRD-history state. No state is shared across tasks,
even though the selected LLC is structurally shared in the configuration.

Before hierarchy analysis, the input must satisfy:

```text
coverage.complete() == true
rejected_accesses == 0
excluded_opaque_call_sites == 0
task IDs are non-empty and unique
every resolved address uses AddressBasis::Absolute
```

Any violation fails the complete hierarchy analysis. A partial task or module
result is not a valid result under this model.

### 8.1 Analysis-region input extension (R2)

The [region contract](analysis-regions-v1.md) defines one complete, non-nested
`APE_ANALYZE_BEGIN`/`APE_ANALYZE_END` region per function, selected automatically
as one independent cold task. A region takes precedence over `APE_ANALYZE` on the same function;
combining it with `APE_INLINE` is unsupported. Empty regions are retained.
Outside accesses do not count or warm caches, while resolvable outside value
dependencies are preserved through compilation.

Retained canonical-IR loads inside a region count even when their global objects
are defined outside it. Reusing an outside-loaded value does not count that load
again; constant-folded values add no references. Runtime-loaded loop bounds and
unresolved indices are rejected before region LAT output. Normalization-created
scalar/control instructions need not carry region tags: R2 validates selected
access sites and complete-loop descriptors separately using the whole function.

APE/LAT v2 retains the original `function`, parameters and object identities,
adds `analysis_scope: {"kind":"region","name":"APE_ANALYZE"}`, and contains only the
selected body. Function task IDs stay unchanged. A region task ID is
`region:<UTF-8-byte-length>:<function>:APE_ANALYZE`; collisions fail the complete module.
The existing source/line provenance and callback interfaces remain applicable.

Region source uses the fixed `clang14-o0-region-v1` C++ frontend pipeline:
capture and remove compiler-inserted annotations before `mem2reg` and
`loop-simplify`. Optimized input pipelines and arbitrary imported region IR are
unsupported. Missing or inconsistent source/IR boundary information must fail,
not select the whole function. The ordinary function input path stays unchanged.

## 9. First-service metrics and invariants

For task `i`, let:

- `MA_i` be the number of L1 input cache-line references;
- `N_i^LLC` be the number of LLC input references;
- `H_i^L1` and `M_i^L1` be L1 hits and misses;
- `H_i^LLC` and `M_i^LLC` be LLC hits and misses;
- `EHC_L1` and `EHC_LLC` count references first serviced by those levels; and
- `AMC` count references missed by both caches and first serviced by Memory.

Every valid task result must satisfy:

```text
MA_i = N_i^L1
N_i^LLC = M_i^L1
N_i^L1 = H_i^L1 + CM_i^L1 + RM_i^L1
N_i^LLC = H_i^LLC + CM_i^LLC + RM_i^LLC
M_i^L1 = CM_i^L1 + RM_i^L1
M_i^LLC = CM_i^LLC + RM_i^LLC
EHC_L1 = H_i^L1
EHC_LLC = H_i^LLC
AMC = M_i^LLC
EHC_L1 + EHC_LLC + AMC = MA_i
N_i^LLC = EHC_LLC + AMC
```

An invariant failure invalidates the complete result. When `MA_i` is zero, the
L1-hit, LLC-hit, and all-cache-miss ratios are present as JSON `null`. Otherwise,
they are `EHC_L1 / MA_i`, `EHC_LLC / MA_i`, and `AMC / MA_i`; they must sum to
one and are derived from the authoritative integer counts.

## 10. Artifact boundary

One successful hierarchy run may produce three separate artifacts:

| Artifact | Contract |
| --- | --- |
| `RESULT.json` | Deterministic semantic hierarchy summary |
| `EVENTS.json` | Optional, bounded per-reference diagnostics |
| `TELEMETRY.json` | Runtime, peak-RSS, stage timing, and host measurements |

`RESULT.json` carries the fixed model identity, effective selected hierarchy,
input identity, complete task summaries, coverage, and invariant status. It
must not contain runtime, RSS, timestamps, hostname, output paths, or an event
array.

An event limit controls only how many diagnostic events are retained. It never
shortens analysis or changes summary counters. Telemetry is not part of
deterministic semantic comparison. Enabling or changing either optional
artifact must not change the semantic result bytes for the same tool version,
raw inputs, and semantic options.

Normal artifacts are written only after complete analysis and invariant
validation succeed. Unsupported input or an analysis failure must not leave a
partial artifact that appears successful.

## 11. Unsupported and deferred policy behavior

This model supersedes the earlier GR740-oriented `write-through`,
`no-write-allocate`, and `all-store forwarding` proposal for this branch. Those
behaviors, along with writeback traffic, dirty state, prefetch, coherence, DMA,
inter-task warm state, and concurrent interference, are unsupported here and
may be introduced only under a different model identifier.

An associativity-censored CSRD mode is also deferred. It must use a distinct
model or result identity and must not be mixed with `full-exact` results.
