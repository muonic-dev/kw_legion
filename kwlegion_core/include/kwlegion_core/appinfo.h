/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Muonic
 */

#pragma once

#include <QObject>
#include <QQmlEngine>
#include <QString>
#include <QUrl>

namespace KWLegionCore {

// Read-only, build-time version/build info surfaced to QML for display (the
// About page). Values are baked in by CMake - see version.h.in.
class AppInfo : public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON
    Q_PROPERTY(QString version READ version CONSTANT)
    Q_PROPERTY(QString buildHash READ buildHash CONSTANT)
    Q_PROPERTY(QString logFilePath READ logFilePath CONSTANT)
    Q_PROPERTY(QUrl logDirectoryUrl READ logDirectoryUrl CONSTANT)
    Q_PROPERTY(bool startMinimized READ shouldStartMinimized CONSTANT)

   public:
    explicit AppInfo(QObject* parent = nullptr);

    // Where main.cpp's log message handler writes to - shared here so it and
    // the About page's "attach your log" link can't drift apart.
    static QString defaultLogFilePath();

    // This must never be called after the engine starts
    static void setStartMinimized(bool);

    [[nodiscard]] bool shouldStartMinimized() const;

    // NOLINTNEXTLINE(readability-convert-member-functions-to-static)
    [[nodiscard]] QString version() const;
    // NOLINTNEXTLINE(readability-convert-member-functions-to-static)
    [[nodiscard]] QString buildHash() const;
    // NOLINTNEXTLINE(readability-convert-member-functions-to-static)
    [[nodiscard]] QString logFilePath() const;
    // NOLINTNEXTLINE(readability-convert-member-functions-to-static)
    [[nodiscard]] QUrl logDirectoryUrl() const;

   private:
    inline static bool mStartMinimized = false;
};

}  // namespace KWLegionCore
