#pragma once

#include <core/engine/worker-configuration.h>

#include <filesystem>
#include <istream>
#include <string_view>

namespace CE::Engine {
    // Version 1.0 settings; no SDK types or discovery, threads, group creation or
    // affinity changes. Runtime availability is resolved by WorkerPool/make_group.
    [[nodiscard]] WorkerConfiguration parse_worker_configuration(std::string_view document, const std::filesystem::path& source = {});
    [[nodiscard]] WorkerConfiguration load_worker_configuration(std::istream& input, const std::filesystem::path& source = {});
    [[nodiscard]] WorkerConfiguration load_worker_configuration(const std::filesystem::path& source);
}
