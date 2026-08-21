/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Muonic
 */
#include <QtQml/qqmlregistration.h>
#include <legionparser/replay.h>

// Register the Faction enum with QML so that we can use the constants
// in the .qml files for things like image lookup.
//
// moc's Q_ENUM_NS scanner only finds enumerators from a literal `enum`
// declaration in scope - it can't see through a `using Faction = ...` alias
// to LegionParser::Faction, so the enumerators are redeclared here, with
// values pinned to the real enum. Keep the enumerator names in sync with
// LegionParser::Faction if it ever changes.
namespace KWLegionCore::FactionQml {
Q_NAMESPACE
QML_NAMED_ELEMENT(Faction)

enum class Faction : std::uint8_t {
    GDI = LegionParser::toUInt8(LegionParser::Faction::GDI),
    ST = LegionParser::toUInt8(LegionParser::Faction::ST),
    ZOCOM = LegionParser::toUInt8(LegionParser::Faction::ZOCOM),
    Nod = LegionParser::toUInt8(LegionParser::Faction::Nod),
    BH = LegionParser::toUInt8(LegionParser::Faction::BH),
    MoK = LegionParser::toUInt8(LegionParser::Faction::MoK),
    Scrin = LegionParser::toUInt8(LegionParser::Faction::Scrin),
    Reaper = LegionParser::toUInt8(LegionParser::Faction::Reaper),
    Traveler = LegionParser::toUInt8(LegionParser::Faction::Traveler),
    Unknown = LegionParser::toUInt8(LegionParser::Faction::Unknown),
};
Q_ENUM_NS(Faction)

}  // namespace KWLegionCore::FactionQml