/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Muonic
 */

#pragma once

#include <QDateTime>
#include <QHash>
#include <QObject>
#include <QString>
#include <QTimer>

namespace KWLegionCore {

/**
 * A snapshot of the on-disk state of a path at the moment we last looked at
 * it. Deferred re-checks a path by resampling and comparing against this.
 */
struct Watermark {
    // Size in bytes. Zero for a path that does not exist.
    qint64 size = 0;
    // Last modification time, invalid for a path that does not exist.
    // Windows does not reliably update this while a writer holds the file
    // open, but it does get updated when the handle is closed - which makes
    // a change here a decent "the game finally let go of the file" signal,
    // and one that catches a final write that didn't change the size.
    QDateTime modifiedAt;
    // When this watermark was taken. Drives the long-stop retry.
    QDateTime sampledAt;

    // Whether other describes a different on-disk state than this one.
    [[nodiscard]] bool differsFrom(const Watermark& other) const;
};

// While a replay file is being actively written there is a window where
// parsing it cannot succeed. This tracks those paths and reports when the
// bytes on disk have actually moved - grown, or merely been touched - since
// we last looked, which is the only point at which another parse attempt can
// tell us anything new.
class Deferred : public QObject {
    Q_OBJECT

   public:
    Deferred(QObject* parent = nullptr);

    /**
     * Take a snapshot of the current on-disk state of path.
     *
     * Callers should sample *before* attempting a parse: if the file grows
     * while it is being read, a sample taken afterwards would record bytes
     * that were never actually parsed, and we would then sit waiting for a
     * change that had already happened.
     */
    static Watermark sample(const QString& path);

    /**
     * Alter the recheck interval. Only triggers the next time the timer fires
     */
    void setRecheckIntervalMs(long ms);

    /**
     * Alter how long a path may sit completely unchanged before we retry it
     * anyway. Only applies to entries enqueued after this call.
     */
    void setLongStopMs(qint64 ms);

    /**
     * Enqueue path, waiting until the file on disk differs from observed.
     *
     * observed should be the state sampled before the parse attempt that
     * failed.
     */
    void waitForChange(const QString& path, const Watermark& observed,
                       bool allowLongStop = true);

    void removeWaitForChange(const QString& path);

    // Stop the internal timer (only important to prevent warnings)
    void stop();

   signals:
    /**
     * Emitted when a path enqueued via waitForChange has moved on disk (or
     * hit the long stop) and is worth another parse attempt. The path is
     * dropped from the deferred set as this fires - a caller that still
     * can't parse it is expected to enqueue it again.
     */
    void pathChanged(const QString& path);

   private:
    struct WatermarkRecord {
        Watermark watermark;
        // We must be able to continue watching some paths forever
        bool allowLongStop;
    };

    void timerFired();

    long m_recheckIntervalMs;
    qint64 m_longStopMs;
    QTimer* m_trigger;
    QHash<QString, WatermarkRecord> m_deferred;
};
}  // namespace KWLegionCore
