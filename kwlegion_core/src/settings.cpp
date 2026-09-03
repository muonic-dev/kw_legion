/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Muonic
 */

#include "settings.h"

#include <QObject>
#include <QString>
#include <memory>
#include <utility>

namespace KWLegionCore {
Settings::Settings(QObject* parent) : QObject(parent) {}

bool Settings::shouldAutostart() const {
    if (!m_autostarter) {
        return false;
    }
    return m_autostarter->shouldAutostart();
}

Settings* Settings::create(QQmlEngine* /*qmlEngine*/, QJSEngine* /*jsEngine*/) {
    // Signature is Qt's QML_SINGLETON factory contract - must return T*, not
    // gsl::owner<T*>. Ownership transfers to the QML engine at the call site.
    return new Settings();  // NOLINT(cppcoreguidelines-owning-memory)
}

void Settings::setAutostart(bool v) {
    m_autostarter->setAutostart(v);
    emit autostartChanged();
}

constexpr const char* START_MINIMIZED = "start/minimized";

bool Settings::startMinimized() const {
    return m_settings.value(START_MINIMIZED).toBool();
}

void Settings::setStartMinimized(bool minimized) {
    m_settings.setValue(START_MINIMIZED, minimized);
    m_settings.sync();
    emit startMinimizedChanged();
}

constexpr const char* CLOSE_TO_TRAY = "close/toTray";
bool Settings::closeToTray() const {
    return m_settings.value(CLOSE_TO_TRAY, true).toBool();
}

void Settings::setCloseToTray(bool closeToTray) {
    m_settings.setValue(CLOSE_TO_TRAY, closeToTray);
    m_settings.sync();
    emit closeToTrayChanged();
}

void Settings::setAutostartMechanism(
    std::unique_ptr<AutostartMechanism> autostarter) {
    m_autostarter = std::move(autostarter);
    emit autostartChanged();
}
}  // namespace KWLegionCore