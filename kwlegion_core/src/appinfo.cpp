/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Muonic
 */

#include <kwlegion_core/appinfo.h>

#include "version.h"

namespace KWLegionCore {

AppInfo::AppInfo(QObject* parent) : QObject(parent) {}

// Q_PROPERTY READ accessors must be non-static for Qt's meta-object system to
// invoke them, even though these don't touch instance state.
// NOLINTNEXTLINE(readability-convert-member-functions-to-static)
QString AppInfo::version() const { return {KW_LEGION_VERSION_SEMVER}; }

// NOLINTNEXTLINE(readability-convert-member-functions-to-static)
QString AppInfo::buildHash() const { return {KW_LEGION_GIT_HASH}; }

}  // namespace KWLegionCore
