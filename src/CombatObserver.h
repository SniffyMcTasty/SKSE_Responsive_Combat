#pragma once

#include "Config.h"

#include "SKSE/SKSE.h"

namespace ResponsiveCombat {
    void HandleObservationMessage(const SKSE::MessagingInterface::Message& message, const Settings& settings);
}
