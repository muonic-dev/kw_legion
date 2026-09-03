/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Muonic
 */

#include <kwlegion_core/autostart.h>

#include <QCoreApplication>
#include <QDir>
#include <QSettings>
#include <QString>
#include <memory>

namespace KWLegionCore {
namespace {
class WindowsAutostartMechanism : public AutostartMechanism {
   public:
    WindowsAutostartMechanism()
        // TODO: This is in theory a fallible operation and we should surface
        // that.
        : m_runKey(
              QStringLiteral(
                  R"(HKEY_CURRENT_USER\Software\Microsoft\Windows\CurrentVersion\Run)"),
              QSettings::NativeFormat) {}

    [[nodiscard]] bool shouldAutostart() const override {
        const QString name = QCoreApplication::applicationName();
        return m_runKey.contains(name);
    }

    void setAutostart(bool autostart) override {
        const QString name = QCoreApplication::applicationName();
        if (autostart) {
            // Unsure if the registry autostart mechanism can tolerate /
            const QString path = QDir::toNativeSeparators(
                QCoreApplication::applicationFilePath());
            m_runKey.setValue(name, path);
        } else {
            m_runKey.remove(name);
        }
    }

   private:
    QSettings m_runKey;
};
}  // namespace

std::unique_ptr<AutostartMechanism> createPlatformAutostartMechanism() {
    return std::make_unique<WindowsAutostartMechanism>();
}
}  // namespace KWLegionCore