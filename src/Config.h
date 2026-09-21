#pragma once

#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace ResponsiveCombat {
    enum class LogLevel { trace, debug, info, warn, error, critical, off };

    struct Settings {
        bool enabled{true};
        LogLevel logLevel{LogLevel::info};
    };

    enum class ConfigStatus { missing, loaded, rejected };

    struct ConfigResult {
        Settings settings;
        ConfigStatus status{ConfigStatus::loaded};
        std::vector<std::string> diagnostics;
    };

    ConfigResult ParseConfig(std::string_view text);
    ConfigResult LoadConfig(const std::filesystem::path& path);
    std::string_view LogLevelName(LogLevel level);
}
