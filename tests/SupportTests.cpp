#include "Config.h"
#include "Logging.h"

#include <array>
#include <chrono>
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>

using namespace ResponsiveCombat;
namespace fs = std::filesystem;

namespace {
    void Require(bool condition, std::string_view message)
    {
        if (!condition) {
            throw std::runtime_error(std::string(message));
        }
    }

    void Defaults(const ConfigResult& result)
    {
        Require(result.settings.enabled, "Expected Enabled=true");
        Require(result.settings.logLevel == LogLevel::info, "Expected LogLevel=info");
    }

    struct TemporaryDirectory {
        fs::path path;

        TemporaryDirectory()
        {
            const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
            for (int attempt = 0; attempt < 100; ++attempt) {
                path = fs::temp_directory_path() / ("ResponsiveCombatTests-" +
                    std::to_string(stamp) + "-" + std::to_string(attempt));
                if (fs::create_directory(path)) {
                    return;
                }
            }
            throw std::runtime_error("Could not create a unique test directory.");
        }

        ~TemporaryDirectory()
        {
            std::error_code ignored;
            fs::remove_all(path, ignored);
        }
    };

    void Write(const fs::path& path, std::string_view text)
    {
        std::ofstream file(path, std::ios::binary);
        file << text;
        file.close();
        Require(!file.fail(), "Fixture write failed");
    }

    std::string Read(const fs::path& path)
    {
        std::ifstream file(path, std::ios::binary);
        Require(file.good(), "Fixture read failed");
        return {std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
    }

    void Test(std::string_view name, const char* example)
    {
        if (name == "defaults") {
            for (const auto text : {"", "; comment\n# comment\n", "[General]\nSchemaVersion=1\n"}) {
                const auto result = ParseConfig(text);
                Defaults(result);
                Require(result.status == ConfigStatus::loaded && result.diagnostics.empty(), "Defaults should load quietly");
            }
        } else if (name == "valid") {
            const auto result = ParseConfig("\xEF\xBB\xBF[GENERAL]\r\nSchemaVersion = 1\r\nEnabled = FALSE ; comment\r\n[Logging]\r\nLogLevel=DEBUG\r\n");
            Require(!result.settings.enabled && result.settings.logLevel == LogLevel::debug, "BOM, case, whitespace, CRLF or comments failed");
            Require(result.status == ConfigStatus::loaded && result.diagnostics.empty(), "Valid file should load quietly");
            Require(!ParseConfig("[General]\nEnabled=0").settings.enabled, "Numeric false failed");
            Require(ParseConfig("[General]\nEnabled=1").settings.enabled, "Numeric true failed");
        } else if (name == "levels") {
            constexpr std::array levels{LogLevel::trace, LogLevel::debug, LogLevel::info, LogLevel::warn,
                LogLevel::error, LogLevel::critical, LogLevel::off};
            constexpr std::array nativeLevels{spdlog::level::trace, spdlog::level::debug, spdlog::level::info,
                spdlog::level::warn, spdlog::level::err, spdlog::level::critical, spdlog::level::off};
            auto logging = InitializeLogging(std::nullopt);
            for (std::size_t i = 0; i < levels.size(); ++i) {
                const auto result = ParseConfig("[Logging]\nLogLevel=" + std::string(LogLevelName(levels[i])));
                Require(result.settings.logLevel == levels[i] && result.diagnostics.empty(), "Log level parse failed");
                SetLogLevel(*logging.logger, levels[i]);
                Require(logging.logger->level() == nativeLevels[i], "Log level mapping failed");
            }
        } else if (name == "invalid-values") {
            const auto result = ParseConfig("[General]\nEnabled=perhaps\n[Logging]\nLogLevel=verbose\n");
            Defaults(result);
            Require(result.diagnostics.size() == 2, "Each invalid value should have one diagnostic");
            Require(result.status == ConfigStatus::loaded, "Invalid individual values should not reject the file");
            const auto partial = ParseConfig("[General]\nEnabled=false\n[Logging]\nLogLevel=invalid");
            Require(!partial.settings.enabled && partial.settings.logLevel == LogLevel::info, "Valid fields must survive invalid neighbors");
            Require(ParseConfig("[General]\nEnabled=\n[Logging]\nLogLevel=\n").diagnostics.size() == 2, "Empty values must be diagnosed");
        } else if (name == "schema") {
            for (const auto value : {"0", "2", "-1", "1.5", "1garbage", ""}) {
                const auto result = ParseConfig(std::string("[General]\nEnabled=false\nSchemaVersion=") + value);
                Defaults(result);
                Require(result.status == ConfigStatus::rejected && result.diagnostics.size() == 1, "Unsupported schema must reject all settings");
            }
        } else if (name == "syntax") {
            for (const auto text : {"[General]\nEnabled=false\n[broken", "[General]\nEnabled=false\nbroken",
                     "[General]\nEnabled=false\nEnabled=true", "[General]\nEnabled=false\n enabled=true",
                     "[Logging]\nLogLevel=debug\n continuation"}) {
                const auto result = ParseConfig(text);
                Defaults(result);
                Require(result.status == ConfigStatus::rejected && result.diagnostics.size() == 1, "Malformed or ambiguous file must reject all settings");
            }
        } else if (name == "bounds") {
            const std::array texts{std::string(65537, '\n'), ";" + std::string(190, 'x'), std::string("a\0b", 3)};
            for (const auto& text : texts) {
                const auto result = ParseConfig(text);
                Defaults(result);
                Require(result.status == ConfigStatus::rejected, "Unsafe input must be rejected");
            }
            Require(ParseConfig(std::string(65536, '\n')).status == ConfigStatus::loaded, "Exact file bound should load");
            Require(ParseConfig(";" + std::string(189, 'x')).status == ConfigStatus::loaded, "Exact line bound should load");
        } else if (name == "unknown") {
            const auto result = ParseConfig("[General]\nEnabled=false\nUnused=1\n[Future]\nValue=2");
            Require(!result.settings.enabled && result.status == ConfigStatus::loaded, "Unknown keys must not reject valid settings");
            Require(result.diagnostics.size() == 2, "Unknown keys should each be diagnosed once");
        } else if (name == "files") {
            TemporaryDirectory temp;
            const auto path = temp.path / "ResponsiveCombat.ini";
            const auto missing = LoadConfig(path);
            Defaults(missing);
            Require(missing.status == ConfigStatus::missing && !fs::exists(path), "Missing file must not be created");
            Require(LoadConfig(temp.path).status == ConfigStatus::rejected, "Directory is not a configuration file");
            for (const std::string text : {"[General]\nEnabled=false\n", "[General]\nEnabled=invalid\n", "[broken"}) {
                Write(path, text);
                const auto before = fs::last_write_time(path);
                const auto result = LoadConfig(path);
                Require(Read(path) == text && fs::last_write_time(path) == before, "Loading must not rewrite configuration");
                if (text.find("Enabled=false") != std::string::npos) {
                    Require(!result.settings.enabled, "File settings not loaded");
                }
            }
            Write(path, std::string(65537, '\n'));
            Require(LoadConfig(path).status == ConfigStatus::rejected, "Oversized file must be rejected");
        } else if (name == "logging-directory") {
            Require(!CorrectSteamLogDirectory(std::nullopt), "Missing directory must preserve debugger fallback");
            const fs::path documents = L"C:/Example/OneDrive/Redirected Documents/\u00e9/My Games";
            const auto incorrect = documents / L"Skyrim.INI" / L"SKSE";
            const auto expected = documents / L"Skyrim Special Edition" / L"SKSE";
            Require(CorrectSteamLogDirectory(incorrect) == expected, "Incorrect INI directory must be corrected without changing Documents");
            for (const auto folder : {L"Skyrim Special Edition", L"Skyrim Special Edition GOG", L"Skyrim VR", L"Custom"}) {
                const auto directory = documents / folder / L"SKSE";
                Require(CorrectSteamLogDirectory(directory) == directory, "Unrelated directory must not be changed");
            }
            const auto unrelated = documents / L"Skyrim.INI" / L"Other";
            Require(CorrectSteamLogDirectory(unrelated) == unrelated, "Only the exact legacy SKSE path should change");
            TemporaryDirectory temp;
            auto logging = InitializeLogging(CorrectSteamLogDirectory(temp.path / L"Skyrim.INI" / L"SKSE"));
            Require(logging.file == temp.path / L"Skyrim Special Edition" / L"SKSE" / L"ResponsiveCombat.log", "Logger must use corrected directory");
            logging.logger->info("corrected-directory-marker");
            Require(Read(*logging.file).find("corrected-directory-marker") != std::string::npos, "Corrected log must contain output");
            Require(!fs::exists(temp.path / L"Skyrim.INI"), "Incorrect directory must not be created");
        } else if (name == "logging") {
            TemporaryDirectory temp;
            auto logging = InitializeLogging(temp.path / "SKSE");
            Require(logging.file.has_value() && logging.diagnostic.empty(), "File logger failed");
            logging.logger->info("startup-marker");
            Require(Read(*logging.file).find("startup-marker") != std::string::npos, "Startup must be flushed immediately");
            SetLogLevel(*logging.logger, LogLevel::warn);
            logging.logger->info("filtered-marker");
            logging.logger->warn("warning-marker");
            SetLogLevel(*logging.logger, LogLevel::off);
            logging.logger->critical("disabled-marker");
            logging.logger->flush();
            const auto text = Read(*logging.file);
            Require(text.find("warning-marker") != std::string::npos, "Warning missing");
            Require(text.find("filtered-marker") == std::string::npos && text.find("disabled-marker") == std::string::npos, "Log filter failed");
        } else if (name == "logging-fallback") {
            TemporaryDirectory temp;
            const auto blocker = temp.path / "not-a-directory";
            Write(blocker, "fixture");
            for (const auto directory : {std::optional<fs::path>{}, std::optional<fs::path>{blocker}}) {
                auto logging = InitializeLogging(directory);
                Require(logging.logger && !logging.file && !logging.diagnostic.empty(), "Missing or unusable directory must use fallback");
                logging.logger->warn("fallback-marker");
                logging.logger->flush();
            }
            Require(Read(blocker) == "fixture", "Fallback must not modify the blocking file");
        } else if (name == "logging-rotation") {
            TemporaryDirectory temp;
            for (int session = 0; session < 6; ++session) {
                auto logging = InitializeLogging(temp.path);
                Require(logging.file.has_value(), "Rotation logger failed");
                logging.logger->info("session-{}", session);
            }
            Require(Read(temp.path / "ResponsiveCombat.log").find("session-5") != std::string::npos, "Newest session missing");
            Require(Read(temp.path / "ResponsiveCombat.1.log").find("session-4") != std::string::npos, "Previous session missing");
            Require(Read(temp.path / "ResponsiveCombat.3.log").find("session-2") != std::string::npos, "Oldest retained session missing");
            Require(!fs::exists(temp.path / "ResponsiveCombat.4.log"), "Too many backups retained");
            auto logging = InitializeLogging(temp.path);
            for (int i = 0; i < 6; ++i) {
                logging.logger->info("{}", std::string(600000, 'x'));
            }
            logging.logger->flush();
            std::size_t count = 0;
            for (const auto& file : fs::directory_iterator(temp.path)) {
                Require(file.file_size() <= 1024 * 1024, "Log file exceeded rotation bound");
                ++count;
            }
            Require(count == 4, "Rotation should retain current file and three backups");
        } else if (name == "example") {
            Require(example != nullptr, "Example path required");
            const auto result = LoadConfig(example);
            Defaults(result);
            Require(result.status == ConfigStatus::loaded && result.diagnostics.empty(), "Shipped configuration must be valid");
        } else {
            throw std::runtime_error("Unknown test case.");
        }
    }
}

int main(int argc, char** argv)
{
    try {
        Require(argc >= 2, "Test case required");
        Test(argv[1], argc >= 3 ? argv[2] : nullptr);
        std::cout << argv[1] << ": passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
