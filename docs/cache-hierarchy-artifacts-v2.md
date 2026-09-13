# Hierarchy RESULT v2

This is the current contract for `yarda_cpp --analysis hierarchy-rd` exports.
RESULT v2 names task metrics using the CHASER glossary. It preserves the
analysis model, accounting units, counts, ratios, ordering and publication
behavior of [v1](cache-hierarchy-artifacts-v1.md).

## 1. Artifact and input versions

Each artifact has an independent schema version:

| Artifact | `schema_version` | Change from v1 |
| --- | ---: | --- |
| RESULT | 2 | Seven task metric keys are renamed |
| EVENTS | 1 | Fields and cache observations are unchanged |
| TELEMETRY | 1 | Fields and measurements are unchanged |

All three artifacts from one run share the same `analysis_id`, computed with
RESULT schema version 2. The EVENTS and TELEMETRY schemas retain the
[v1 field contracts](cache-hierarchy-artifacts-v1.md#4-events); their own schema
versions do not select the analysis identity preimage.

LAT schema 2, cache configuration schema 1, and the independent experiment
manifest/measurement schemas are unchanged. The model remains
`exact-two-level-lru-demand-v1`, `full-exact`, `linked_absolute`: these identify
analysis semantics, which this terminology migration does not change.

## 2. Task terminology and accounting

The task metric keys change as follows. Old aliases are not emitted.

| RESULT v1 key | RESULT v2 key | Meaning |
| --- | --- | --- |
| `ma` | `modeled_accesses` | Total L1 input cache-line references |
| `ehc_l1` | `l1_first_hit_count` | L1 First-Hit Count |
| `ehc_llc` | `llc_first_hit_count` | LLC First-Hit Count |
| `amc` | `all_cache_miss_count` | All-Cache Miss Count |
| `hr_l1` | `l1_first_hit_ratio` | L1 First-Hit Ratio |
| `hr_llc` | `llc_first_hit_ratio` | LLC First-Hit Ratio |
| `mr` | `all_cache_miss_ratio` | All-Cache Miss Ratio |

`source_accesses` still counts expanded source load/store accesses.
`modeled_accesses` counts their normalized L1 cache-line references. A source
access spanning two lines contributes one source access and two modeled
accesses. It is not renamed to a source memory-access count.

All three ratios use `modeled_accesses` as their denominator, including the
LLC ratio. LLC lookups include only L1 misses through Miss-Stream Propagation.
For every complete task:

```text
modeled_accesses = l1_first_hit_count + llc_first_hit_count + all_cache_miss_count
llc.lookups = llc_first_hit_count + all_cache_miss_count
```

The Cache-Level Profile is the ordered vector
`[l1_first_hit_ratio, llc_first_hit_ratio, all_cache_miss_ratio]` represented by
the three existing flat ratio fields. Integer counts remain authoritative.
For `modeled_accesses = 0`, all three ratios are explicit JSON `null`.
The C++ `TaskHierarchySummary::all_cache_misses` member supplies the JSON
`all_cache_miss_count` value.

Each task contains these fields in order:

```text
task_id, source_accesses, modeled_accesses,
l1_first_hit_count, llc_first_hit_count, all_cache_miss_count,
l1_first_hit_ratio, llc_first_hit_ratio, all_cache_miss_ratio,
l1, llc, coverage, invariants
```

All other RESULT fields follow [v1 section 3](cache-hierarchy-artifacts-v1.md#3-result),
including cache-level summaries, full exact CSRD histograms, coverage and
invariants. The complete ordered byte-format example is
[`hierarchy_result_v2.json`](../backend/tests/cache/output/fixtures/hierarchy_result_v2.json).

## 3. Analysis identity

`hierarchy_analysis_id()` hashes the same compact, lexicographically sorted
UTF-8 JSON preimage as v1, with `schema_version` now identifying RESULT v2:

```text
schema_version = 2
analysis_mode = hierarchy-rd
model_id = exact-two-level-lru-demand-v1
csrd_mode = full-exact
address_basis = linked_absolute
tool_version
analysis_core_id = 0
lat_sha256
elf_sha256
cache_config_sha256
semantic_analysis_options
```

Consequently, v1 and v2 results have different analysis IDs even when tool
version, raw inputs and semantic options match. EVENTS and TELEMETRY use the
v2 result's ID so artifacts remain joinable within the same run.

## 4. Publication and migration

The CLI and library emit RESULT v2 by default. Consumers must check
`RESULT.schema_version` and use the corresponding metric keys. No v1 export
option or duplicate legacy keys are added. Existing CLI flags are unchanged.

The v1 rules for unsigned counts, nulls, numeric histogram order, UTF-8,
`dump(2)` plus one final LF, complete-analysis validation and rollback-safe
publication still apply. Diagnostic options do not change RESULT bytes for
the same tool version, inputs and semantic options.

Existing v1 golden files and historical B12 measurement artifacts retain
their original keys and identity. They are historical evidence, not v2 output
examples, and must not be rewritten or combined with v2 as identical artifacts.
