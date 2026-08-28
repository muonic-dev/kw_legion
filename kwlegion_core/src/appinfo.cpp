/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Muonic
 */

#include <kwlegion_core/appinfo.h>

#include "version.h"

namespace KWLegionCore {

AppInfo::AppInfo(QObject* parent) : QObject(parent) {}

QString AppInfo::version() const {
    return QStringLiteral(KW_LEGION_VERSION_SEMVER);
}

QString AppInfo::buildHash() const {
    return QStringLiteral(KW_LEGION_GIT_HASH);
}

}  // namespace KWLegionCore
