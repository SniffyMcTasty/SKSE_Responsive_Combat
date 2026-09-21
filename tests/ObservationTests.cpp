#include "ObservationTrace.h"

#include <spdlog/sinks/ostream_sink.h>

#include <iostream>
#include <sstream>
#include <stdexcept>
#include <thread>
#include <vector>

using namespace ResponsiveCombat;
using namespace std::chrono_literals;

namespace {
    void Require(bool condition, std::string_view message)
    {
        if (!condition) {
            throw std::runtime_error(std::string(message));
        }
    }

    void Test(std::string_view name)
    {
        std::ostringstream output;
        auto logger = std::make_shared<spdlog::logger>("test", std::make_shared<spdlog::sinks::ostream_sink_mt>(output));
        logger->set_pattern("%v");
        ObservationTrace trace(logger);
        const ObservationTrace::Time start{};

        if (name == "lifecycle") {
            Require(!trace.Active() && !trace.Record(TraceChannel::input, "ignored", start), "Trace must start inactive");
            trace.Begin("new-game", start);
            Require(trace.Active(), "Begin should activate recording");
            Require(trace.Record(TraceChannel::input, "edge=press", start + 10ms), "Active recording failed");
            trace.End("pre-load-game", start + 20ms);
            Require(!trace.Active() && !trace.Record(TraceChannel::animation, "stale", start + 30ms), "End must gate late callbacks");
            trace.End("duplicate", start + 40ms);
            trace.Begin("post-load-game", start + 50ms);
            trace.Record(TraceChannel::state, "attack=none", start + 60ms);
            const auto text = output.str();
            Require(text.find("session=1 seq=1 t_ms=0 source=lifecycle begin reason=new-game") != std::string::npos, "First session header missing");
            Require(text.find("session=2 seq=1 t_ms=0 source=lifecycle begin reason=post-load-game") != std::string::npos, "Load must reset session time and sequence");
            Require(text.find("stale") == std::string::npos && text.find("duplicate") == std::string::npos, "Inactive records must be ignored");
            trace.Begin("replacement", start + 70ms);
            Require(output.str().find("end reason=replaced-session") != std::string::npos, "Replaced sessions must be explicit");
        } else if (name == "budget") {
            trace.Begin("test", start);
            for (std::uint32_t i = 0; i < ObservationTrace::eventsPerSecond; ++i) {
                Require(trace.Record(TraceChannel::animation, "tag=event", start), "Budget rejected too early");
            }
            Require(!trace.Record(TraceChannel::animation, "dropped-one", start + 999ms), "Budget must limit excess events");
            Require(!trace.Record(TraceChannel::animation, "dropped-two", start + 999ms), "Budget must count excess events");
            trace.Tick(start + 1s);
            Require(output.str().find("rate-limit channel=animation dropped=2") != std::string::npos, "Suppression summary missing");
            Require(trace.Record(TraceChannel::animation, "new-window", start + 1s), "Budget did not recover");
            trace.End("done", start + 2s);
        } else if (name == "channels") {
            trace.Begin("test", start);
            for (std::uint32_t i = 0; i < ObservationTrace::eventsPerSecond + 1; ++i) {
                trace.Record(TraceChannel::animation, "tag=spam", start);
            }
            Require(trace.Record(TraceChannel::input, "edge=release", start), "Animation spam must not starve input");
            Require(trace.Record(TraceChannel::hit, "direction=outgoing", start), "Animation spam must not starve hits");
            trace.End("pre-load", start);
            Require(output.str().find("dropped=1") != std::string::npos, "End must flush pending suppression counts");
            trace.Begin("reload", start);
            Require(trace.Record(TraceChannel::animation, "fresh", start), "Reload must clear budgets");
        } else if (name == "timestamps") {
            trace.Begin("test", start);
            trace.Record(TraceChannel::action, "first", start + 20ms);
            trace.Record(TraceChannel::animation, "late-callback", start + 10ms);
            Require(output.str().find("seq=3 t_ms=20 source=animation late-callback") != std::string::npos, "Recorder time must not go backwards");
        } else if (name == "text") {
            Require(TraceText("tag\n\r\t\"value") == "tag...'value", "Control characters and quotes must be escaped");
            Require(TraceText(std::string(200, 'x')) == std::string(96, 'x') + "...", "Untrusted event text must be bounded");
            Require(TraceText("").empty(), "Empty payload should be valid");
        } else if (name == "states") {
            Require(AttackStateName(0) == "none" && AttackStateName(5) == "follow-through", "Native melee state labels changed");
            Require(AttackStateName(8) == "bow-draw" && AttackStateName(17) == "fired", "Native ranged state labels changed");
            Require(AttackStateName(7) == "unknown" && AttackStateName(999) == "unknown", "Unknown states must not be guessed");
        } else if (name == "concurrent") {
            trace.Begin("test", start);
            std::vector<std::thread> workers;
            for (int i = 0; i < 4; ++i) {
                workers.emplace_back([&] {
                    for (int j = 0; j < 20; ++j) {
                        trace.Record(TraceChannel::animation, "concurrent", start);
                    }
                });
            }
            for (auto& worker : workers) {
                worker.join();
            }
            trace.End("done", start);
            std::istringstream lines(output.str());
            std::string line;
            std::uint64_t sequence = 0;
            while (std::getline(lines, line)) {
                Require(line.find("seq=" + std::to_string(++sequence) + " ") != std::string::npos, "Concurrent sequence numbers must stay ordered");
            }
            Require(sequence == 82, "Concurrent trace lost records");
        } else {
            throw std::runtime_error("Unknown observation test.");
        }
    }
}

int main(int argc, char** argv)
{
    try {
        Require(argc == 2, "Test case required");
        Test(argv[1]);
        std::cout << argv[1] << ": passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
