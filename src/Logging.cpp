#include "Logging.h"

#include <spdlog/sinks/msvc_sink.h>
#include <spdlog/sinks/rotating_file_sink.h>

namespace ResponsiveCombat {
    std::optional<std::filesystem::path> CorrectSteamLogDirectory(std::optional<std::filesystem::path> directory)
    {
        // CommonLibSSE-NG issue #98: the 1.6.1170 lookup returns an INI filename.
        if (directory && directory->filename() == L"SKSE" &&
            directory->parent_path().filename() == L"Skyrim.INI") {
            return directory->parent_path().parent_path() / L"Skyrim Special Edition" / L"SKSE";
        }
        return directory;
    }

    LoggingResult InitializeLogging(const std::optional<std::filesystem::path>& directory)
    {
        LoggingResult result;
        std::shared_ptr<spdlog::sinks::sink> sink;
        if (directory) {
            try {
                auto path = *directory / "ResponsiveCombat.log";
                sink = std::make_shared<spdlog::sinks::rotating_file_sink_mt>(
                    path.string(), 1024 * 1024, 3, true);
                result.file = std::move(path);
            } catch (const std::exception& error) {
                result.diagnostic = "File logging unavailable: " + std::string(error.what());
            }
        } else {
            result.diagnostic = "SKSE log directory unavailable.";
        }
        if (!sink) {
            sink = std::make_shared<spdlog::sinks::msvc_sink_mt>(false);
        }
        result.logger = std::make_shared<spdlog::logger>("ResponsiveCombat", std::move(sink));
        result.logger->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%l] %v");
        result.logger->set_level(spdlog::level::info);
        result.logger->flush_on(spdlog::level::info);
        result.logger->set_error_handler([](const std::string& message) {
            OutputDebugStringA("[ResponsiveCombat] Logging error: ");
            OutputDebugStringA(message.c_str());
            OutputDebugStringA("\n");
        });
        return result;
    }

    void SetLogLevel(spdlog::logger& logger, LogLevel level)
    {
        switch (level) {
        case LogLevel::trace: logger.set_level(spdlog::level::trace); break;
        case LogLevel::debug: logger.set_level(spdlog::level::debug); break;
        case LogLevel::info: logger.set_level(spdlog::level::info); break;
        case LogLevel::warn: logger.set_level(spdlog::level::warn); break;
        case LogLevel::error: logger.set_level(spdlog::level::err); break;
        case LogLevel::critical: logger.set_level(spdlog::level::critical); break;
        case LogLevel::off: logger.set_level(spdlog::level::off); break;
        }
    }
}
