/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Muonic
 */

#pragma once

namespace KWLegionCore {
class ActionScope;

/**
 * A RAII utility for determining if a critical section is running
 *
 * Not thread safe
 */
class ActionScope {
   public:
    class Guard {
       public:
        Guard(ActionScope& scope) : m_scope(scope) { m_scope.m_active = true; }

        Guard(const Guard&) = delete;
        Guard(Guard&&) = delete;
        Guard& operator=(const Guard&) = delete;
        Guard& operator=(Guard&&) = delete;

        ~Guard() { m_scope.m_active = false; }

       private:
        ActionScope& m_scope;
    };

    Guard enter() { return {*this}; }

    [[nodiscard]] bool isActive() const { return m_active; }

   private:
    bool m_active = false;
};
}  // namespace KWLegionCore