/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Muonic
 */
#pragma once

#include <QtQml/qqmlregistration.h>
#include <kwlegion_core/asyncvalue.h>

// Register AsyncState with QML so .qml files can compare against named
// constants (e.g. AsyncState.Pending) instead of magic numbers.
//
// moc's enum scanner only finds enumerators from a literal `enum`
// declaration in scope - it can't see through a `using AsyncState = ...`
// alias to KWLegionCore::AsyncState (confirmed: a class-scope Q_ENUM built on
// such an alias compiles and links cleanly, but silently registers zero
// enumerators, with no build error), so the enumerators are redeclared here,
// with values pinned to the real enum. Keep the enumerator names in sync
// with KWLegionCore::AsyncState if it ever changes. Same pattern as
// FactionQml in faction_registration.h.
namespace KWLegionCore::AsyncStateQml {
Q_NAMESPACE
QML_NAMED_ELEMENT(AsyncState)

enum class AsyncState : std::uint8_t {
    Pending = static_cast<std::uint8_t>(KWLegionCore::AsyncState::Pending),
    Complete = static_cast<std::uint8_t>(KWLegionCore::AsyncState::Complete),
    Failed = static_cast<std::uint8_t>(KWLegionCore::AsyncState::Failed),
};
Q_ENUM_NS(AsyncState)

}  // namespace KWLegionCore::AsyncStateQml
