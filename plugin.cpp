#include "src/Config.h"
#include "src/Logging.h"
#include "src/CombatObserver.h"

#include <Windows.h>

#include <array>

namespace {
    ResponsiveCombat::Settings settings;
    bool fileLoggingAvailable{false};

    std::filesystem::path ConfigPath()
    {
        std::array<wchar_t, 32768> executable{};
        const auto length = GetModuleFileNameW(nullptr, executable.data(), static_cast<DWORD>(executable.size()));
        if (length == 0 || length >= executable.size()) {
            throw std::runtime_error("Cannot resolve the Skyrim executable directory.");
        }
        return std::filesystem::path(executable.data()).parent_path() /
               L"Data/SKSE/Plugins/ResponsiveCombat.ini";
    }

    void OnMessage(SKSE::MessagingInterface::Message* message) noexcept
    {
        if (!message) {
            return;
        }
        try {
            ResponsiveCombat::HandleObservationMessage(*message, settings);
            if (message->type != SKSE::MessagingInterface::kDataLoaded) {
                return;
            }
            spdlog::info("Game data loaded. Enabled={}. No gameplay hooks are installed in this milestone.", settings.enabled);
            spdlog::default_logger()->flush();
            if (auto* console = RE::ConsoleLog::GetSingleton()) {
                console->Print("[ResponsiveCombat] Plugin loaded successfully.");
                if (!fileLoggingAvailable) {
                    console->Print("[ResponsiveCombat] File logging unavailable; diagnostics use debugger output.");
                }
                if (!settings.enabled) {
                    console->Print("[ResponsiveCombat] Disabled by configuration. Diagnostics remain active.");
                }
            }
        } catch (...) {
            OutputDebugStringA("[ResponsiveCombat] Data-loaded diagnostics failed.\n");
        }
    }
}

SKSEPluginLoad(const SKSE::LoadInterface* skse)
{
    try {
        // Establish a fallback before SKSE resolves its runtime-specific log path.
        spdlog::set_default_logger(ResponsiveCombat::InitializeLogging(std::nullopt).logger);
        SKSE::Init(skse);
        const auto resolvedDirectory = SKSE::log::log_directory();
        auto logDirectory = resolvedDirectory;
        if (skse->RuntimeVersion() == REL::Version{1, 6, 1170, 0}) {
            logDirectory = ResponsiveCombat::CorrectSteamLogDirectory(std::move(logDirectory));
        }
        const auto logging = ResponsiveCombat::InitializeLogging(logDirectory);
        spdlog::set_default_logger(logging.logger);
        fileLoggingAvailable = logging.file.has_value();
        spdlog::info("ResponsiveCombat {} initializing; Skyrim runtime {}.",
            RESPONSIVE_COMBAT_VERSION, skse->RuntimeVersion().string());
        if (logDirectory != resolvedDirectory) {
            spdlog::info("Corrected CommonLibSSE log directory for Skyrim 1.6.1170.");
        }
        if (!logging.diagnostic.empty()) {
            spdlog::warn("{} Diagnostics use debugger output.", logging.diagnostic);
        }
        if (logging.file) {
            spdlog::info("Log file: {}", logging.file->string());
        }

        try {
            const auto path = ConfigPath();
            const auto configuration = ResponsiveCombat::LoadConfig(path);
            settings = configuration.settings;
            spdlog::info("Configuration: {}", path.string());
            switch (configuration.status) {
            case ResponsiveCombat::ConfigStatus::missing:
                spdlog::info("Configuration absent; using defaults. No file will be created.");
                break;
            case ResponsiveCombat::ConfigStatus::loaded:
                spdlog::info("Configuration loaded (schema 1).");
                break;
            case ResponsiveCombat::ConfigStatus::rejected:
                spdlog::warn("Configuration rejected; using defaults.");
                break;
            }
            for (const auto& diagnostic : configuration.diagnostics) {
                spdlog::warn("{}", diagnostic);
            }
        } catch (const std::exception& error) {
            settings = {};
            spdlog::warn("Configuration unavailable: {}; using defaults.", error.what());
        }

        spdlog::info("Settings: SchemaVersion=1, Enabled={}, LogLevel={}, TraceCombat={}.",
            settings.enabled, ResponsiveCombat::LogLevelName(settings.logLevel), settings.traceCombat);
        spdlog::info("SKSE version (packed): {:08X}.", skse->SKSEVersion());
        if (settings.traceCombat && settings.logLevel > ResponsiveCombat::LogLevel::info) {
            spdlog::warn("Combat observation requires LogLevel=info, debug, or trace; observers will remain inactive.");
        }
        spdlog::info("Gameplay behavior is unchanged; no gameplay hooks are installed.");
        const auto* messaging = SKSE::GetMessagingInterface();
        if (!messaging || !messaging->RegisterListener(OnMessage)) {
            spdlog::error("Could not register the SKSE message listener; plugin initialization failed.");
            return false;
        }
        spdlog::info("Initialization complete; waiting for game data.");
        spdlog::default_logger()->flush();
        // Always retain startup diagnostics, even with LogLevel=off.
        ResponsiveCombat::SetLogLevel(*spdlog::default_logger(), settings.logLevel);
        return true;
    } catch (const std::exception& error) {
        OutputDebugStringA("[ResponsiveCombat] Initialization failed: ");
        OutputDebugStringA(error.what());
        OutputDebugStringA("\n");
        return false;
    } catch (...) {
        OutputDebugStringA("[ResponsiveCombat] Initialization failed.\n");
        return false;
    }
}
