/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Muonic
 */

#include <kwlegion_core/appinfo.h>

#include <QFileInfo>
#include <QStandardPaths>

#include "version.h"

namespace KWLegionCore {

namespace {
inline constexpr bool DEBUG_BUILD =
#ifdef QT_DEBUG
    true;
#else
    false;
#endif
}  // namespace

AppInfo::AppInfo(QObject* parent) : QObject(parent) {}

QString AppInfo::defaultLogFilePath() {
    return DEBUG_BUILD ? QStringLiteral("./kw_legion.log")
                       : QStandardPaths::writableLocation(
                             QStandardPaths::CacheLocation) +
                             QStringLiteral("/kw_legion.log");
}

// Q_PROPERTY READ accessors must be non-static for Qt's meta-object system to
// invoke them, even though these don't touch instance state.
// NOLINTNEXTLINE(readability-convert-member-functions-to-static)
QString AppInfo::version() const { return {KW_LEGION_VERSION_SEMVER}; }

// NOLINTNEXTLINE(readability-convert-member-functions-to-static)
QString AppInfo::buildHash() const { return {KW_LEGION_GIT_HASH}; }

// NOLINTNEXTLINE(readability-convert-member-functions-to-static)
QString AppInfo::logFilePath() const { return defaultLogFilePath(); }

// NOLINTNEXTLINE(readability-convert-member-functions-to-static)
QUrl AppInfo::logDirectoryUrl() const {
    return QUrl::fromLocalFile(QFileInfo(logFilePath()).absolutePath());
}

void AppInfo::setStartMinimized(bool startMinimized) {
    AppInfo::mStartMinimized = startMinimized;
}

// NOLINTNEXTLINE(readability-convert-member-functions-to-static)
bool AppInfo::shouldStartMinimized() const { return mStartMinimized; }

}  // namespace KWLegionCore
