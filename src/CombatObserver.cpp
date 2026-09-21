#include "CombatObserver.h"
#include "ObservationTrace.h"

#include <atomic>

namespace ResponsiveCombat {
    namespace {
        using Result = RE::BSEventNotifyControl;
        using Clock = ObservationTrace::Clock;

        template <class Function>
        void Guard(Function&& function) noexcept
        {
            try {
                function();
            } catch (...) {
                static std::atomic_flag reported{};
                if (!reported.test_and_set()) {
                    try {
                        spdlog::error("Combat observation callback failed; trace may be incomplete.");
                    } catch (...) {}
                }
            }
        }

        class CombatObserver final :
            public RE::BSTEventSink<RE::InputEvent*>,
            public RE::BSTEventSink<RE::BSAnimationGraphEvent>,
            public RE::BSTEventSink<RE::MenuOpenCloseEvent>,
            public RE::BSTEventSink<RE::TESHitEvent>,
            public RE::BSTEventSink<RE::TESDeathEvent>,
            public RE::BSTEventSink<SKSE::ActionEvent> {
        public:
            static CombatObserver& Get()
            {
                // Event sources can outlive static destruction during game shutdown.
                static auto* observer = new CombatObserver;
                return *observer;
            }

            void Install()
            {
                if (installed_) {
                    return;
                }
                auto* input = RE::BSInputDeviceManager::GetSingleton();
                auto* ui = RE::UI::GetSingleton();
                auto* scripts = RE::ScriptEventSourceHolder::GetSingleton();
                auto* actions = SKSE::GetActionEventSource();
                if (!input || !ui || !scripts || !actions) {
                    spdlog::error("Combat observation unavailable: a required event source is missing.");
                    return;
                }
                input->AddEventSink<RE::InputEvent*>(this);
                ui->AddEventSink<RE::MenuOpenCloseEvent>(this);
                scripts->AddEventSink<RE::TESHitEvent>(this);
                scripts->AddEventSink<RE::TESDeathEvent>(this);
                actions->AddEventSink(this);
                installed_ = true;
                // Buffer high-volume observation output; input polling flushes once per second.
                spdlog::default_logger()->flush_on(spdlog::level::warn);
                spdlog::info("Combat observation enabled; waiting for a new game or loaded save. Gameplay is unchanged.");
            }

            void Begin(std::string_view reason)
            {
                if (!installed_) {
                    return;
                }
                trace_.Begin(reason);
                lastSnapshot_.clear();
                nextGraphCheck_ = {};
                nextStateCheck_ = {};
                graphSeen_ = false;
                trace_.Record(TraceChannel::lifecycle, "animation-graph status=waiting");
                // New-game notification can precede player 3D; input polling retries.
                PollPlayer();
            }

            void End(std::string_view reason)
            {
                trace_.End(reason);
            }

            Result ProcessEvent(RE::InputEvent* const* events, RE::BSTEventSource<RE::InputEvent*>*) override
            {
                Guard([&] {
                    if (!trace_.Active()) {
                        return;
                    }
                    trace_.Tick();
                    PollPlayer();
                    auto* player = RE::PlayerCharacter::GetSingleton();
                    auto* ui = RE::UI::GetSingleton();
                    auto* controls = RE::ControlMap::GetSingleton();
                    if (!events || !player || !player->Is3DLoaded() || player->IsDead() || !ui || !controls ||
                        ui->GameIsPaused() || ui->IsApplicationMenuOpen() || ui->IsModalMenuOpen() ||
                        ui->IsItemMenuOpen() || controls->textEntryCount != 0 ||
                        ui->IsMenuOpen(RE::Console::MENU_NAME) || ui->IsMenuOpen(RE::MainMenu::MENU_NAME) ||
                        ui->IsMenuOpen(RE::LoadingMenu::MENU_NAME)) {
                        return;
                    }
                    for (auto* event = *events; event; event = event->next) {
                        const auto* button = event->AsButtonEvent();
                        if (!button || (!button->IsDown() && !button->IsUp())) {
                            continue;
                        }
                        trace_.Record(TraceChannel::input, fmt::format(
                            "edge={} device={} code={} event=\"{}\" held_s={:.3f}",
                            button->IsDown() ? "press" : "release", static_cast<std::uint32_t>(button->GetDevice()),
                            button->GetIDCode(), TraceText(button->QUserEvent().c_str()), button->HeldDuration()));
                    }
                });
                return Result::kContinue;
            }

            Result ProcessEvent(const RE::BSAnimationGraphEvent* event, RE::BSTEventSource<RE::BSAnimationGraphEvent>*) override
            {
                Guard([&] {
                    if (event && event->holder && event->holder == RE::PlayerCharacter::GetSingleton() && trace_.Active()) {
                        // Copy event text only; do not inspect or alter graph state on this callback thread.
                        trace_.Record(TraceChannel::animation, fmt::format("tag=\"{}\" payload=\"{}\"",
                            TraceText(event->tag.c_str()), TraceText(event->payload.c_str())));
                    }
                });
                return Result::kContinue;
            }

            Result ProcessEvent(const RE::MenuOpenCloseEvent* event, RE::BSTEventSource<RE::MenuOpenCloseEvent>*) override
            {
                Guard([&] {
                    if (!event || !trace_.Active()) {
                        return;
                    }
                    trace_.Record(TraceChannel::menu, fmt::format("name=\"{}\" opening={}",
                        TraceText(event->menuName.c_str()), event->opening));
                    if (event->opening && event->menuName == RE::MainMenu::MENU_NAME) {
                        End("main-menu");
                    }
                });
                return Result::kContinue;
            }

            Result ProcessEvent(const RE::TESHitEvent* event, RE::BSTEventSource<RE::TESHitEvent>*) override
            {
                Guard([&] {
                    auto* player = RE::PlayerCharacter::GetSingleton();
                    if (!event || !player || !trace_.Active() ||
                        (event->cause.get() != player && event->target.get() != player)) {
                        return;
                    }
                    trace_.Record(TraceChannel::hit, fmt::format(
                        "direction={} source={:08X} projectile={:08X} power={} bash={} blocked={}",
                        event->cause.get() == player ? "outgoing" : "incoming", event->source, event->projectile,
                        event->flags.any(RE::TESHitEvent::Flag::kPowerAttack),
                        event->flags.any(RE::TESHitEvent::Flag::kBashAttack),
                        event->flags.any(RE::TESHitEvent::Flag::kHitBlocked)));
                });
                return Result::kContinue;
            }

            Result ProcessEvent(const RE::TESDeathEvent* event, RE::BSTEventSource<RE::TESDeathEvent>*) override
            {
                Guard([&] {
                    if (event && event->actorDying && event->actorDying.get() == RE::PlayerCharacter::GetSingleton()) {
                        trace_.Record(TraceChannel::lifecycle, fmt::format("player-death dead={}", event->dead));
                    }
                });
                return Result::kContinue;
            }

            Result ProcessEvent(const SKSE::ActionEvent* event, RE::BSTEventSource<SKSE::ActionEvent>*) override
            {
                Guard([&] {
                    if (event && event->actor && event->actor == RE::PlayerCharacter::GetSingleton() && trace_.Active()) {
                        trace_.Record(TraceChannel::action, fmt::format("type={} slot={} source={:08X}",
                            static_cast<std::uint32_t>(*event->type), static_cast<std::uint32_t>(*event->slot),
                            event->sourceForm ? event->sourceForm->GetFormID() : 0));
                    }
                });
                return Result::kContinue;
            }

        private:
            CombatObserver() : trace_(spdlog::default_logger()) {}

            void PollPlayer()
            {
                auto* player = RE::PlayerCharacter::GetSingleton();
                auto* ui = RE::UI::GetSingleton();
                const auto now = Clock::now();
                if (!player || !ui) {
                    return;
                }
                if (now >= nextGraphCheck_) {
                    nextGraphCheck_ = now + std::chrono::seconds(1);
                    RE::BSAnimationGraphManagerPtr manager;
                    player->GetAnimationGraphManager(manager);
                    bool available = false;
                    if (manager) {
                        for (const auto& graph : manager->graphs) {
                            if (graph) {
                                // Source registration is locked and idempotent, including after graph replacement.
                                graph->GetEventSource<RE::BSAnimationGraphEvent>()->AddEventSink(this);
                                available = true;
                            }
                        }
                    }
                    if (available != graphSeen_) {
                        trace_.Record(TraceChannel::lifecycle, fmt::format("animation-graph available={}", available));
                        graphSeen_ = available;
                    }
                }
                if (now < nextStateCheck_) {
                    return;
                }
                nextStateCheck_ = now + std::chrono::milliseconds(50);
                if (!player->Is3DLoaded()) {
                    return;
                }
                const auto* actorState = player->AsActorState();
                const auto attack = static_cast<std::uint32_t>(actorState->GetAttackState());
                const auto* camera = RE::PlayerCamera::GetSingleton();
                const auto* left = player->GetEquippedObject(true);
                const auto* right = player->GetEquippedObject(false);
                const auto snapshot = fmt::format(
                    "attack={}({}) life={} weapon_state={} left={:08X} right={:08X} blocking={} sprinting={} sneaking={} first_person={} paused={}",
                    AttackStateName(attack), attack, static_cast<std::uint32_t>(actorState->GetLifeState()),
                    static_cast<std::uint32_t>(actorState->GetWeaponState()), left ? left->GetFormID() : 0,
                    right ? right->GetFormID() : 0, player->IsBlocking(), actorState->IsSprinting(),
                    actorState->IsSneaking(), camera && camera->IsInFirstPerson(), ui->GameIsPaused());
                if (snapshot != lastSnapshot_ && trace_.Record(TraceChannel::state, snapshot, now)) {
                    lastSnapshot_ = snapshot;
                }
            }

            ObservationTrace trace_;
            bool installed_{false};
            bool graphSeen_{false};
            Clock::time_point nextGraphCheck_{};
            Clock::time_point nextStateCheck_{};
            std::string lastSnapshot_;
        };
    }

    void HandleObservationMessage(const SKSE::MessagingInterface::Message& message, const Settings& settings)
    {
        if (!settings.enabled || !settings.traceCombat) {
            return;
        }
        if (!spdlog::default_logger()->should_log(spdlog::level::info)) {
            return;
        }
        auto& observer = CombatObserver::Get();
        switch (message.type) {
        case SKSE::MessagingInterface::kDataLoaded: observer.Install(); break;
        case SKSE::MessagingInterface::kPreLoadGame: observer.End("pre-load-game"); break;
        case SKSE::MessagingInterface::kPostLoadGame:
            if (message.data) {
                observer.Begin("post-load-game");
            } else {
                observer.End("load-failed");
                spdlog::warn("Combat observation remains inactive after a failed save load.");
            }
            break;
        case SKSE::MessagingInterface::kNewGame: observer.Begin("new-game"); break;
        default: break;
        }
    }
}
