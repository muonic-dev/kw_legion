// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Muonic

#pragma once

#include <QList>
#include <QMutex>
#include <QString>
#include <QtTypes>
#include <chrono>
#include <optional>

namespace KWLegionUI {

class StartupTrace {
   public:
    StartupTrace();

    void mark(const QString& milestone);
    void enableLogging();

   private:
    struct Entry {
        QString milestone;
        qint64 elapsedMs;
        qint64 deltaMs;
    };

    static void log(const Entry& entry);

    std::chrono::steady_clock::time_point m_started;
    std::optional<qint64> m_processToMainMs;
    QMutex m_guard;
    QList<Entry> m_pending;
    qint64 m_lastElapsed = 0;
    bool m_loggingReady = false;
};

}  // namespace KWLegionUI
