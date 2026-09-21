#pragma once

#include "Config.h"

#include <spdlog/spdlog.h>

#include <memory>
#include <optional>

namespace ResponsiveCombat {
    struct LoggingResult {
        std::shared_ptr<spdlog::logger> logger;
        std::optional<std::filesystem::path> file;
        std::string diagnostic;
    };

    std::optional<std::filesystem::path> CorrectSteamLogDirectory(std::optional<std::filesystem::path> directory);
    LoggingResult InitializeLogging(const std::optional<std::filesystem::path>& directory);
    void SetLogLevel(spdlog::logger& logger, LogLevel level);
}
