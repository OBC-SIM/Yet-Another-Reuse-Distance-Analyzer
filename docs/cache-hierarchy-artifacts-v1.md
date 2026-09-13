# Hierarchy artifacts v1

This document preserves the historical v1 contract. Current exports follow
[RESULT v2](cache-hierarchy-artifacts-v2.md), which renames task metrics and
updates analysis identity while retaining EVENTS and TELEMETRY schema 1.

This document specifies the artifact boundary of
[`exact-two-level-lru-demand-v1`](cache-hierarchy-rd-model-v1.md).
The library exposes pure JSON serializers and optional measurement support.
The `yarda_cpp --analysis hierarchy-rd` CLI binds inputs, executes the streaming
analyzer and publishes these artifacts.

## 1. Common rules

- Only complete, successful analyses can produce normal artifacts. Any
  exception invalidates the whole invocation, including retained events and
  measurements from previously completed tasks.
- A module must contain at least one selected task. A task with no accesses is
  retained. Function/region identity and LAT order follow
  [analysis-regions-v1.md](analysis-regions-v1.md).
- `RESULT.json` contains semantic data. `EVENTS.json` contains a bounded
  diagnostic prefix. `TELEMETRY.json` contains process measurements.
- Each artifact has its own `schema_version: 1` and the same `analysis_id`.
- Serializers return `nlohmann::ordered_json`. Publish `dump(2)` followed by one
  LF, without converting through ordinary `nlohmann::json`, which sorts keys.
  Strings use the library's UTF-8 JSON escaping, with `ensure_ascii=false`.
- DOM construction and dumping are separate failure boundaries. Non-identity
  text from summaries, selected-path names, events and host providers is copied
  without UTF-8 validation. Callers must supply valid UTF-8 for publication:
  strict `dump()` throws `nlohmann::json::type_error` for invalid bytes, even
  when the DOM builder returned successfully. Complete dumping in memory before
  opening the corresponding output file. Identity preimage dumping happens
  inside `hierarchy_analysis_id()`, which reports invalid text as
  `std::invalid_argument`.
- The CMake dependency floor is nlohmann_json 3.10.5, the validated baseline
  for ordered serialization and the checked-in byte-format examples.
- Counts are unsigned 64-bit integers. Absent ratios/distances are explicit
  `null`, not omitted. Histogram keys are decimal distances in numeric order.
- Clock, RSS, timestamp, host, paths and event arrays never enter RESULT.

## 2. Input identity

`lat_sha256`, `elf_sha256`, and `cache_config_sha256` hash exact raw file bytes.
Digests use lowercase hexadecimal with exactly 64 characters. File hashing
streams bounded buffers and fails on open/read errors.

The analysis ID is SHA-256 over compact UTF-8 JSON with lexicographically sorted
object keys, preserving array order. Its complete preimage contains:

```text
schema_version = 1
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

The caller supplies a build-fixed commit/release identifier as `tool_version`.
Identity options must be an object whose nested leaves are integers, booleans
or strings; floating-point, null and binary values are rejected. Every
effective result-changing option, including defaults, must be supplied.
The current analyzer has no such options, so the effective object is `{}`.
Work allowances govern successful completion, not successful-result semantics,
and are excluded, like output paths, event limits and telemetry settings.
Input selection is already represented by raw LAT bytes and task identities.

## 3. RESULT

Top-level fields occur in this order:

```text
schema_version, analysis_mode, model_id, csrd_mode, address_basis,
tool_version, analysis_id, inputs, selected_path, cache_hierarchy, tasks
```

`inputs` contains the three raw hashes followed by `lat_schema_version` (2),
`cache_schema_version` (1), `elf_class` (`ELF32` or `ELF64`) and `elf_machine`.
The caller binds metadata to the same ET_EXEC inputs used by the analyzer.
`selected_path` contains `core_id`, `l1_name`, `llc_name`, `memory_name`.

`cache_hierarchy` contains `levels` in L1/LLC order, then:

```text
allocation: all-demand-misses
lower_level_requests: l1-misses-only
inclusion: independent-no-back-invalidation-or-victim-insertion
task_initial_state: cold
```

Each level has `name`, `role`, `size_bytes`, `line_size_bytes`, `associativity`,
`set_count`, `replacement` (`LRU`). Unmodeled configured write policy and
latency are omitted. Capacity arithmetic is checked for overflow.

Each task contains, in order:

```text
task_id, source_accesses, ma, ehc_l1, ehc_llc, amc, hr_l1, hr_llc, mr,
l1, llc, coverage, invariants
```

Each cache summary contains `lookups`, `hits`, `misses`, `cold_misses`,
`replacement_misses`, `unique_lines`, `csrd_histogram`. Cold references do not
appear in the histogram, and finite distances above associativity stay exact.

Coverage contains `source_accesses`, `resolved_accesses`, `rejected_accesses`,
`emitted_line_references`, `excluded_opaque_call_sites`, `complete`.
Successful hierarchy analysis has rejected opaque calls before task execution,
so `excluded_opaque_call_sites` is necessarily zero; this is a model guarantee,
not a missing measurement. Complete source coverage and module/task totals are
validated at serialization.

The five invariant flags are `level_conservation_l1`,
`level_conservation_llc`, `llc_input_matches_l1_misses`,
`first_service_conservation`, `all_passed`. All must be true, and the serializer
also validates the underlying counts and histogram partitions. Ratios are
emitted unchanged and checked through the analyzer's existing finalizer, so
there is no separate rounding implementation. For `ma=0` all three are null.
Negative-zero ratios are rejected to preserve their canonical encoding.

The complete byte-format example is checked in as
[`hierarchy_result_v1.json`](../backend/tests/cache/output/fixtures/hierarchy_result_v1.json).

## 4. EVENTS

Top-level fields are `schema_version`, `analysis_id`, `event_limit`,
`events_truncated`, `events`. An enabled sink retains exactly
`min(event_limit, total_line_references)` events, across all tasks.
Truncation is true exactly when the total is greater than the limit; zero is
a real limit. Disabling events creates no event artifact.

The borrowed B8 callback arguments must both be copied for retention.
The serializer consumes those owned records and delivery metadata from the
same successful run. It neither rebuilds the trace nor reruns analysis.
Delivery-count and ordering checks cannot prove which run supplied a record;
discarding records after failure remains the caller's responsibility.

Each event contains:

```text
task_id, source_access_ordinal, line_span_ordinal, object_id,
linked_address, access_size, operation,
l1_set, l1_tag, l1_csrd, l1_outcome,
llc_set, llc_tag, llc_csrd, llc_outcome, first_service_level
```

`linked_address` is the first touched byte in that line; `access_size` is the
original source access's total byte count. Operations are `load`/`store`;
outcomes are `hit`, `cold-miss`, `replacement-miss`; first service is `L1`,
`LLC`, or `Memory`. Cold distances and every LLC field on an L1 hit are null.
Task/source/span order and consistent cache observations are required.

## 5. TELEMETRY

Fields occur in this order:

```text
schema_version, analysis_id, total_time_ns, stage_time_ns, peak_rss_bytes,
source_accesses_emitted, line_references_emitted, maximum_inline_depth,
loop_iterations_expanded, host, measured_at_utc
```

Stable stage names are `parse_lat`, `parse_cache`, `parse_elf`,
`resolve_and_stream`, `hierarchy_analysis`, `serialize_result`.
Every stage must have an actual recorded interval, including zero-duration
intervals. Missing measurements are rejected, not filled with zero.

The collector starts before input reading/hashing. Finalize it after RESULT
JSON construction and dumping, before file I/O and optional-artifact
serialization. Thus total includes hashing, parsing, structural expansion,
lazy preparation, streaming, validation, identity and RESULT serialization.
It excludes the measurement snapshot itself and output file publication.

`resolve_and_stream` encloses the streaming analyzer, including preparation.
`hierarchy_analysis` accumulates the task consumer callbacks (state creation,
line mapping, cache analysis, diagnostic delivery and summary finalization).
It is a subset of `resolve_and_stream` and must not be added to it. Per-source
clock overhead exists only with telemetry enabled and must be distinguished
in performance experiments. Parse stages and RESULT serialization are measured
by the caller (CLI integration in B10).

Source/line counts come from successful coverage. `loop_iterations_expanded`
is the sum of loop trip counts reserved at each dynamic entry, including empty
loops and repeated inner-loop entries. `maximum_inline_depth` is structural
expansion depth, with roots at zero; it can include calls inside zero-trip
loops. The producer returns these measured counters after success.

Linux peak RSS uses `getrusage(RUSAGE_SELF).ru_maxrss` in KiB, checked and
converted to bytes. It is a process high-water mark, not an invocation delta
and not child compiler RSS. Host keys are `hostname`, `os`, `release`,
`machine`. The timestamp format is `YYYY-MM-DDTHH:MM:SSZ` in UTC.
Clock/RSS/host/time providers are injectable; system-call adapters stay private.
No duration is compared to an exact real-time value in tests.

## 6. CLI binding and publication

The hierarchy command requires a LAT file with explicit `schema_version: 2`,
`--elf ET_EXEC`, `--cache YAML`, and `--export RESULT`. It uses the same
invocation's input paths for raw hashes and parsing, and supplies parsed schema
versions, ELF class/machine and the selected hierarchy to RESULT. Input files
must remain unchanged throughout the invocation. No legacy schema version is
invented for an unversioned LAT. Legacy CLI modes keep their existing reader
compatibility.

`--analysis mapping` requires ELF/cache and exposes the existing mapping path.
With no `--analysis`, dispatch and legacy stdout/file behavior are unchanged.
No core or region-selection option is added. The model selects core 0; task
scope/identity comes from the LAT.

The following options are exclusive to hierarchy mode:

| Option | Default / contract |
| --- | --- |
| `--max-single-loop-iterations N` | 1,000,000 per dynamic loop entry |
| `--max-cumulative-loop-iterations N` | 1,000,000 across all tasks |
| `--max-source-accesses N` | 1,000,000 across all tasks |
| `--max-line-references N` | 10,000,000 source-to-L1 references |
| `--export-events FILE` | Disabled when omitted |
| `--event-limit N` | 0; requires event export, including explicit zero |
| `--telemetry FILE` | Disabled when omitted |

Numbers must be complete unsigned decimal `uint64_t` values; signs, whitespace,
suffixes and overflow are rejected. Zero is a real allowance, never unlimited.
Recognized repeated options keep their final value. Work-budget exhaustion
invalidates the whole invocation; event truncation still yields a complete
summary. Successive command calls own fresh events and measurements.

The CMake build embeds `PROJECT_VERSION+git.COMMIT[-dirty]` when the source root
is a Git repository, or `PROJECT_VERSION` for a source archive. A nonempty
`YARDA_TOOL_VERSION` overrides either default. Identifiers start with an ASCII
letter/digit and contain only letters, digits, `.`, `_`, `+`, `:`, `/`, `@`, `-`.
Every build refreshes the header if its content changes. Runtime execution
does not query Git, paths or clocks to construct the semantic identity.

The CLI constructs the collector before hashing/reading inputs, measures the
three parse stages, lets the streaming API measure its two stages, and records
RESULT DOM/dump in `serialize_result`. Snapshot occurs before optional JSON
serialization and all file publication. It passes no collector when telemetry
is disabled. Borrowed callback arguments are copied only for enabled events.

All requested documents must finish strict `dump(2)` before any output is
opened. Output paths must be distinct regular files or new files, with existing
parent directories. Empty paths, stdout `-`, symlinks and special files are
rejected. Normalized path and hard-link aliases among outputs or with inputs
are rejected. Each file gets exactly one final LF.

Publication stages all files in private sibling directories and checks writes
and close. Existing outputs have sibling hard-link backups until publication
finishes. EVENTS and TELEMETRY are replaced before RESULT. A handled staging
failure leaves old outputs untouched; a handled rename failure restores prior
files and removes new outputs from this invocation. Failures exit with code 1
and identify the affected path. Analysis, measurement and dump failures publish
nothing and preserve existing files.

Rollback assumes exclusive ownership of these paths during the command; it is
not a transaction across process crashes or concurrent writers. If restoration
itself fails, the diagnostic identifies the retained backup directory. Cleanup
failure after successful publication reports that the complete artifact set
was published and identifies the remaining staging directory; it also exits 1.
