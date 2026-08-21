/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Muonic
 */
#include <QtQml/qqmlregistration.h>
#include <legionparser/replay.h>

// Register the Faction enum with QML so that we can use the constants
// in the .qml files for things like image lookup
namespace KWLegionCore {
// Wire this up for QML
Q_NAMESPACE
using Faction = LegionParser::Faction;
Q_ENUM_NS(Faction)
QML_NAMED_ELEMENT(Faction)

}  // namespace KWLegionCore