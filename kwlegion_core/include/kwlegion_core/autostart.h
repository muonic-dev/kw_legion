/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Muonic
 */

#pragma once

#include <memory>

namespace KWLegionCore {

/**
 * The platform specific autostart mechanism
 */
class AutostartMechanism {  // Defined once per autostart_platform
   public:
    virtual ~AutostartMechanism() = default;

    AutostartMechanism(const AutostartMechanism&) = delete;
    AutostartMechanism(AutostartMechanism&&) = delete;
    AutostartMechanism& operator=(const AutostartMechanism&) = delete;
    AutostartMechanism& operator=(AutostartMechanism&&) = delete;

    [[nodiscard]] virtual bool shouldAutostart() const = 0;
    virtual void setAutostart(bool) = 0;

   protected:
    AutostartMechanism() = default;
};

std::unique_ptr<AutostartMechanism> createPlatformAutostartMechanism();

}  // namespace KWLegionCore