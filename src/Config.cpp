#include "Config.h"

#include <ini.h>

#include <array>
#include <fstream>
#include <map>
#include <optional>

namespace ResponsiveCombat {
    namespace {
        constexpr std::size_t maxFileBytes = 64 * 1024;
        // Stay below inih r56's fixed 200-byte line buffer, including CRLF/NUL.
        constexpr std::size_t maxLineBytes = 190;
        constexpr std::array levelNames{"trace", "debug", "info", "warn", "error", "critical", "off"};

        std::string Lower(std::string_view value)
        {
            std::string result(value);
            for (char& ch : result) {
                if (ch >= 'A' && ch <= 'Z') {
                    ch = static_cast<char>(ch + ('a' - 'A'));
                }
            }
            return result;
        }

        ConfigResult Reject(std::string message)
        {
            return {{}, ConfigStatus::rejected, {std::move(message)}};
        }

        struct ParseContext {
            std::map<std::string, std::string> entries;
            bool duplicate{false};
            bool failed{false};
        };

        int ReadEntry(void* context, const char* section, const char* name, const char* value) noexcept
        {
            auto& parsed = *static_cast<ParseContext*>(context);
            try {
                auto key = Lower(section) + "." + Lower(name);
                if (!parsed.entries.emplace(std::move(key), value ? value : "").second) {
                    parsed.duplicate = true;
                }
                return 1;
            } catch (...) {
                // Never unwind a C++ exception through the C parser.
                parsed.failed = true;
                return 0;
            }
        }
    }

    std::string_view LogLevelName(LogLevel level)
    {
        return levelNames.at(static_cast<std::size_t>(level));
    }

    ConfigResult ParseConfig(std::string_view text)
    {
        if (text.size() > maxFileBytes) {
            return Reject("Configuration exceeds 64 KiB; using defaults.");
        }
        if (text.find('\0') != std::string_view::npos) {
            return Reject("Configuration contains NUL bytes; use UTF-8 text. Using defaults.");
        }
        for (std::size_t start = 0; start < text.size();) {
            const auto end = text.find('\n', start);
            const auto length = (end == std::string_view::npos ? text.size() : end) - start;
            if (length > maxLineBytes) {
                return Reject("Configuration line exceeds 190 bytes; using defaults.");
            }
            if (end == std::string_view::npos) {
                break;
            }
            start = end + 1;
        }

        ParseContext parsed;
        const std::string terminated(text);
        const int error = ini_parse_string(terminated.c_str(), ReadEntry, &parsed);
        if (parsed.failed || error < 0) {
            return Reject("Configuration parser failed; using defaults.");
        }
        if (error > 0) {
            return Reject("Invalid INI syntax at line " + std::to_string(error) + "; using defaults.");
        }
        if (parsed.duplicate) {
            return Reject("Duplicate keys or multiline values are not supported; using defaults.");
        }

        ConfigResult result;
        if (const auto schema = parsed.entries.find("general.schemaversion");
            schema != parsed.entries.end() && schema->second != "1") {
            return Reject("Unsupported General.SchemaVersion; expected 1. Using defaults.");
        }
        for (const auto& [key, value] : parsed.entries) {
            const auto normalized = Lower(value);
            if (key == "general.schemaversion") {
                continue;
            }
            if (key == "general.enabled") {
                if (normalized == "true" || normalized == "1") {
                    result.settings.enabled = true;
                } else if (normalized == "false" || normalized == "0") {
                    result.settings.enabled = false;
                } else {
                    result.diagnostics.emplace_back("Invalid General.Enabled; using true.");
                }
            } else if (key == "logging.loglevel") {
                std::optional<LogLevel> level;
                for (std::size_t i = 0; i < levelNames.size(); ++i) {
                    if (normalized == levelNames[i]) {
                        level = static_cast<LogLevel>(i);
                        break;
                    }
                }
                if (level) {
                    result.settings.logLevel = *level;
                } else {
                    result.diagnostics.emplace_back("Invalid Logging.LogLevel; using info.");
                }
            } else {
                result.diagnostics.emplace_back("Unknown setting " + key + "; ignored.");
            }
        }
        return result;
    }

    ConfigResult LoadConfig(const std::filesystem::path& path)
    {
        std::error_code error;
        const bool exists = std::filesystem::exists(path, error);
        if (error) {
            return Reject("Cannot inspect configuration: " + error.message() + "; using defaults.");
        }
        if (!exists) {
            return {{}, ConfigStatus::missing, {}};
        }
        if (!std::filesystem::is_regular_file(path, error) || error) {
            return Reject("Configuration is not a readable regular file; using defaults.");
        }
        std::ifstream file(path, std::ios::binary);
        if (!file) {
            return Reject("Cannot open configuration; using defaults.");
        }
        std::string text(maxFileBytes + 1, '\0');
        file.read(text.data(), static_cast<std::streamsize>(text.size()));
        if (file.bad() || (file.fail() && !file.eof())) {
            return Reject("Cannot read configuration; using defaults.");
        }
        text.resize(static_cast<std::size_t>(file.gcount()));
        return ParseConfig(text);
    }
}
