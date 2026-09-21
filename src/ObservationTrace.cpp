#include "ObservationTrace.h"

#include <algorithm>
#include <stdexcept>

namespace ResponsiveCombat {
    namespace {
        constexpr std::array channelNames{"input", "animation", "state", "action", "hit", "menu", "lifecycle"};
    }

    std::string TraceText(std::string_view text)
    {
        constexpr std::size_t limit = 96;
        std::string result(text.substr(0, limit));
        for (char& ch : result) {
            if (static_cast<unsigned char>(ch) < 32 || ch == 127) {
                ch = '.';
            } else if (ch == '"') {
                ch = '\'';
            }
        }
        if (text.size() > limit) {
            result += "...";
        }
        return result;
    }

    std::string_view AttackStateName(std::uint32_t state)
    {
        constexpr std::array names{"none", "draw", "swing", "hit", "next-attack", "follow-through", "bash",
            "unknown", "bow-draw", "bow-attached", "bow-drawn", "bow-releasing", "bow-released",
            "bow-next-attack", "bow-follow-through", "fire", "firing", "fired"};
        return state < names.size() ? names[state] : "unknown";
    }

    ObservationTrace::ObservationTrace(std::shared_ptr<spdlog::logger> logger) : logger_(std::move(logger))
    {
        if (!logger_) {
            throw std::invalid_argument("ObservationTrace requires a logger.");
        }
    }

    void ObservationTrace::Write(TraceChannel channel, std::string_view detail, Time now)
    {
        // Callbacks from different sources may race; timestamps follow recorder order.
        lastTime_ = std::max(lastTime_, now);
        const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(lastTime_ - origin_).count();
        logger_->info("[observe] session={} seq={} t_ms={} source={} {}", session_, ++sequence_, elapsed,
            channelNames.at(static_cast<std::size_t>(channel)), detail);
    }

    void ObservationTrace::FlushDropped(Time now, bool force)
    {
        for (std::size_t i = 0; i < budgets_.size(); ++i) {
            auto& budget = budgets_[i];
            if (force || now - budget.start >= std::chrono::seconds(1)) {
                if (budget.dropped != 0) {
                    Write(TraceChannel::lifecycle, fmt::format("rate-limit channel={} dropped={}",
                        channelNames[i], budget.dropped), now);
                }
                budget = {now, 0, 0};
            }
        }
    }

    void ObservationTrace::Begin(std::string_view reason, Time now)
    {
        std::scoped_lock lock(mutex_);
        if (active_) {
            FlushDropped(now, true);
            Write(TraceChannel::lifecycle, "end reason=replaced-session", now);
        }
        ++session_;
        sequence_ = 0;
        origin_ = lastTime_ = lastFlush_ = now;
        for (auto& budget : budgets_) {
            budget = {now, 0, 0};
        }
        active_ = true;
        Write(TraceChannel::lifecycle, "begin reason=" + TraceText(reason), now);
        logger_->flush();
    }

    void ObservationTrace::End(std::string_view reason, Time now)
    {
        std::scoped_lock lock(mutex_);
        if (active_) {
            FlushDropped(now, true);
            Write(TraceChannel::lifecycle, "end reason=" + TraceText(reason), now);
            active_ = false;
            logger_->flush();
        }
    }

    bool ObservationTrace::Record(TraceChannel channel, std::string_view detail, Time now)
    {
        std::scoped_lock lock(mutex_);
        if (!active_ || !logger_->should_log(spdlog::level::info)) {
            return false;
        }
        FlushDropped(now, false);
        auto& budget = budgets_.at(static_cast<std::size_t>(channel));
        if (budget.accepted >= eventsPerSecond) {
            ++budget.dropped;
            return false;
        }
        ++budget.accepted;
        Write(channel, detail, now);
        return true;
    }

    void ObservationTrace::Tick(Time now)
    {
        std::scoped_lock lock(mutex_);
        if (active_) {
            FlushDropped(now, false);
            if (now - lastFlush_ >= std::chrono::seconds(1)) {
                logger_->flush();
                lastFlush_ = now;
            }
        }
    }

    bool ObservationTrace::Active() const
    {
        std::scoped_lock lock(mutex_);
        return active_;
    }
}
