#pragma once

#include <string>
#include <vector>

#include "yarda/cache/hierarchy_result_json.hpp"

namespace yarda::evaluation
{

/** @brief Inputs shared by batch, streaming and correctness verification. */
struct Inputs
{
  nlohmann::json raw;
  ObjectAddressModel objects;
  HierarchyResultMetadata metadata;
};

/**
 * @brief Read immutable raw inputs and bind their identity to this binary.
 * @param row Prepared manifest case with absolute MAP/ELF/cache paths.
 * @return Validated inputs; throws on schema or address-model errors.
 */
Inputs read_inputs(const nlohmann::json & row);

/**
 * @brief Parse inclusive uint64 work allowances from a manifest case.
 * @param row Prepared case containing all four effective work limits.
 * @return Options with diagnostics disabled; invalid values throw.
 */
StreamingHierarchyOptions work_options(const nlohmann::json & row);

/**
 * @brief Collect budgeted sources, then invoke the existing batch core.
 * @param raw Borrowed immutable MAP.
 * @param objects Borrowed absolute object addresses.
 * @param hierarchy Borrowed selected hierarchy.
 * @param options Borrowed loop and emission limits; diagnostic fields ignored.
 * @return Complete materialized batch result or an exception, never partial.
 * @note Line-budget validation maps sources once before the batch core maps
 * them again. This extra work is included in measured batch execution.
 */
BatchHierarchyResult run_batch(const nlohmann::json & raw,
  const ObjectAddressModel & objects, const AnalysisHierarchy & hierarchy,
  const StreamingHierarchyOptions & options);

/**
 * @brief Adapt already-computed batch summaries for the common serializer.
 * @param batch Borrowed complete result.
 * @return Summary copies; no analysis or event copies.
 */
StreamingHierarchyResult batch_summary(const BatchHierarchyResult & batch);

/**
 * @brief Check every streaming event and RESULT against batch execution.
 * @param inputs Borrowed immutable case inputs.
 * @param options Borrowed work limits.
 * @return Nothing; a mismatch throws and invalidates verification.
 */
void verify_paths(const Inputs & inputs,
                  const StreamingHierarchyOptions & options);

/**
 * @brief Check generated sources against a fixture-derived access sequence.
 * @param inputs Borrowed raw inputs with linked symbol addresses.
 * @param row Prepared case identifying the fixture and its dimensions.
 * @return Nothing; mismatched order, offset, width or operation throws.
 */
void verify_sources(const Inputs & inputs, const nlohmann::json & row);

/**
 * @brief Summarize successful samples with linearly interpolated quartiles.
 * @param values Nonempty scalar samples, copied for sorting.
 * @return Median, min, max, IQR and count; throws for empty input.
 */
nlohmann::json distribution(std::vector<std::uint64_t> values);

/**
 * @brief Classify observed exceptions without treating arbitrary errors as success.
 * @param error Borrowed exception from one execution.
 * @return Stable failure category; unknown exceptions remain errors.
 */
std::string failure_status(const std::exception & error);

/**
 * @brief Serialize storage counts separately from deterministic RESULT.
 * @param statistics Borrowed measured snapshot.
 * @return Experiment-specific JSON, not the public telemetry schema.
 */
nlohmann::json statistics_json(const CsrdStatistics & statistics);

/**
 * @brief Read a JSON file or throw with its path.
 * @param path Existing ordinary JSON input file.
 * @return Parsed document.
 */
nlohmann::json read_json(const std::string & path);

/**
 * @brief Aggregate sample files recorded by the orchestration script.
 * @param index Borrowed array of measurement file paths.
 * @return Groups with failure counts and successful-sample distributions.
 */
nlohmann::json summarize_samples(const nlohmann::json & index);

} // namespace yarda::evaluation
