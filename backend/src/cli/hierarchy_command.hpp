#pragma once

#include "artifact_output.hpp"
#include "options.hpp"
#include "yarda/cache/analysis_telemetry.hpp"

namespace yarda::cli
{

/**
 * @brief Analyze one invocation and publish only complete hierarchy artifacts.
 * @param options Borrowed validated hierarchy arguments; input files must
 * remain unchanged during this invocation.
 * @param providers Optional borrowed measurement providers, alive throughout
 * this call. Null selects system measurements; ignored without --telemetry.
 * @param rename Optional borrowed publication callback, following
 * ArtifactRename's contract; called synchronously. Empty uses filesystem rename.
 * @return Nothing; all requested artifacts are published on success.
 * @throws std::exception for input, analysis, serialization or output failure;
 * invocation-local diagnostic state is discarded during stack unwinding.
 */
void run_hierarchy_command(
    const Options & options,
    const AnalysisTelemetryProviders * providers = nullptr,
    const ArtifactRename & rename = {});

} // namespace yarda::cli
