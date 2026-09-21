#pragma once

#include <spdlog/spdlog.h>

#include <array>
#include <chrono>
#include <cstdint>
#include <mutex>
#include <string_view>

namespace ResponsiveCombat {
    enum class TraceChannel { input, animation, state, action, hit, menu, lifecycle, count };

    std::string TraceText(std::string_view text);
    std::string_view AttackStateName(std::uint32_t state);

    class ObservationTrace {
    public:
        using Clock = std::chrono::steady_clock;
        using Time = Clock::time_point;
        static constexpr std::uint32_t eventsPerSecond = 120;

        explicit ObservationTrace(std::shared_ptr<spdlog::logger> logger);
        void Begin(std::string_view reason, Time now = Clock::now());
        void End(std::string_view reason, Time now = Clock::now());
        bool Record(TraceChannel channel, std::string_view detail, Time now = Clock::now());
        void Tick(Time now = Clock::now());
        bool Active() const;

    private:
        struct Budget {
            Time start{};
            std::uint32_t accepted{0};
            std::uint64_t dropped{0};
        };

        void Write(TraceChannel channel, std::string_view detail, Time now);
        void FlushDropped(Time now, bool force);

        std::shared_ptr<spdlog::logger> logger_;
        mutable std::mutex mutex_;
        std::array<Budget, static_cast<std::size_t>(TraceChannel::count)> budgets_{};
        Time origin_{};
        Time lastTime_{};
        Time lastFlush_{};
        std::uint64_t session_{0};
        std::uint64_t sequence_{0};
        bool active_{false};
    };
}
