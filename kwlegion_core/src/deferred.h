/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Muonic
 */

#pragma once

#include <QObject>
#include <QSet>
#include <QString>
#include <QTimer>

namespace KWLegionCore {
// When replay files are being actively written there is a gap where we should
// not attempt to parse them. This is a small helper utility that tracks these
// paths
class Deferred : public QObject {
    Q_OBJECT

   public:
    Deferred(QObject* parent = nullptr);

    /**
     * Determine if a path is ready for parsing.
     *
     * If it is ready (size > 0 and modification time far enough in the past)
     * this will return true. If it is not ready this will return false.
     */
    static bool readyForParsing(const QString& path);

    /**
     * Alter the recheck interval. Only triggers the next time the timer fires
     */
    void setRecheckIntervalMs(long ms);

    /**
     * Enqueue this path to wait until ready
     */
    void waitForReady(const QString& path);

    void removeWaitForReady(const QString& path);

    // Stop the internal timer (only important to prevent warnings)
    void stop();

    QSet<QString> currentPaths();

   signals:
    /**
     * Emitted when a path queried for readiness via
     * readyForParsing(const QString&) is finally ready for parsing
     */
    void pathReady(const QString& path);

   private:
    void timerFired();

    long m_recheckIntervalMs;
    QTimer* m_trigger;
    QSet<QString> m_deferred;
};
}  // namespace KWLegionCore