/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Muonic
 */

#pragma once

#include <QObject>
#include <QQmlEngine>
#include <QString>

namespace KWLegionCore {

// Read-only, build-time version/build info surfaced to QML for display (the
// About page). Values are baked in by CMake - see version.h.in.
class AppInfo : public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON
    Q_PROPERTY(QString version READ version CONSTANT)
    Q_PROPERTY(QString buildHash READ buildHash CONSTANT)

   public:
    explicit AppInfo(QObject* parent = nullptr);

    [[nodiscard]] QString version() const;
    [[nodiscard]] QString buildHash() const;
};

}  // namespace KWLegionCore
