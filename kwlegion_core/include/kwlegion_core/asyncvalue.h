/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Muonic
 */

#pragma once

#include <cstdint>
#include <optional>

namespace KWLegionCore {

enum class AsyncState : std::uint8_t { Pending, Complete, Failed };

template <typename T>
class AsyncValue {
   public:
    void finish(T value) {
        m_state = AsyncState::Complete;
        m_value = std::move(value);
    }

    void error() { m_state = AsyncState::Failed; }

    [[nodiscard]] const T& value() const { return *m_value; }

    [[nodiscard]] AsyncState state() const { return m_state; }

   private:
    AsyncState m_state = AsyncState::Pending;
    std::optional<T> m_value;
};
}  // namespace KWLegionCore
