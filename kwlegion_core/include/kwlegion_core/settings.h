/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Muonic
 */

#pragma once

#include <QtQml/qqmlregistration.h>

#include <QObject>
#include <QSettings>

#include "autostart.h"

class QQmlEngine;
class QJSEngine;

namespace KWLegionCore {
class Settings : public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

    Q_PROPERTY(bool shouldAutostart READ shouldAutostart WRITE setAutostart
                   NOTIFY autostartChanged)
    Q_PROPERTY(bool startMinimized READ startMinimized WRITE setStartMinimized
                   NOTIFY startMinimizedChanged)
   public:
    Settings(QObject* parent = nullptr);

    [[nodiscard]] bool shouldAutostart() const;
    void setAutostart(bool);

    [[nodiscard]] bool startMinimized() const;
    void setStartMinimized(bool startMinimized);

    void setAutostartMechanism(std::unique_ptr<AutostartMechanism> autostarter);

    static Settings* create(QQmlEngine* qmlEngine, QJSEngine* jsEngine);

   signals:
    void autostartChanged();
    void startMinimizedChanged();

   private:
    QSettings m_settings;
    std::unique_ptr<AutostartMechanism> m_autostarter;
};
}  // namespace KWLegionCore