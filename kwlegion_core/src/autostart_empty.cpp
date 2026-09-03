/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Muonic
 */

#include <kwlegion_core/autostart.h>

#include <memory>

#pragma message( \
    "kw_legion: no autostart mechanism for this platform - using EmptyAutostartMechanism")

namespace KWLegionCore {

namespace {

class EmptyAutostartMechanism : public AutostartMechanism {
   public:
    EmptyAutostartMechanism() = default;

    [[nodiscard]] bool shouldAutostart() const override { return false; }
    void setAutostart(bool shouldAutostart) override {}
};
}  // namespace

std::unique_ptr<AutostartMechanism> createPlatformAutostartMechanism() {
    return std::make_unique<EmptyAutostartMechanism>();
}
}  // namespace KWLegionCore