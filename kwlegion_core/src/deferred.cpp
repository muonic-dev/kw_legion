/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Muonic
 */

#include "deferred.h"

#include <QFileDevice>
#include <QFileInfo>
#include <utility>

namespace KWLegionCore {
namespace {
const long TIMER_MS = 10000;

// If nothing about a file has moved for this long we make one more attempt
// anyway. The final write of a match can land, be read as torn (the writer
// hasn't flushed or closed yet), and then never change size or mtime again -
// without this the path would sit in the deferred set until the next launch.
// The clock restarts each time the caller re-enqueues, so a genuinely
// hopeless file degrades to one cheap attempt per interval rather than
// spinning or giving up permanently.
const qint64 LONG_STOP_MS = 5 * 60 * 1000;
}  // namespace

bool Watermark::differsFrom(const Watermark& other) const {
    return size != other.size || modifiedAt != other.modifiedAt;
}

Deferred::Deferred(QObject* parent)
    : QObject(parent),
      m_recheckIntervalMs(TIMER_MS),
      m_longStopMs(LONG_STOP_MS),
      m_trigger(new QTimer(this)) {
    m_trigger->setSingleShot(true);
    m_trigger->callOnTimeout(this, &Deferred::timerFired);
}

Watermark Deferred::sample(const QString& path) {
    // Constructed fresh on every call deliberately - QFileInfo caches its
    // stat results, so a retained instance would never observe the change we
    // are waiting for.
    const QFileInfo pathInfo(path);
    return Watermark{
        .size = pathInfo.size(),
        .modifiedAt = pathInfo.fileTime(QFileDevice::FileModificationTime,
                                        QTimeZone::UTC),
        .sampledAt = QDateTime::currentDateTimeUtc(),
    };
}

void Deferred::setRecheckIntervalMs(long ms) { m_recheckIntervalMs = ms; }

void Deferred::setLongStopMs(qint64 ms) { m_longStopMs = ms; }

void Deferred::waitForChange(const QString& path, const Watermark& observed,
                             bool allowLongStop) {
    m_deferred.insert(path, WatermarkRecord{.watermark = observed,
                                            .allowLongStop = allowLongStop});
    if (!m_trigger->isActive()) {
        m_trigger->start(m_recheckIntervalMs);
    }
}

void Deferred::removeWaitForChange(const QString& path) {
    m_deferred.remove(path);
    if (m_deferred.isEmpty()) {
        m_trigger->stop();
    }
}

void Deferred::timerFired() {
    // Drained up front because emitting pathChanged re-enters us: the
    // handler will typically remove the path and, if it still can't parse
    // it, enqueue it again with a fresh watermark.
    const QHash<QString, WatermarkRecord> toCheck =
        std::exchange(m_deferred, {});
    const QDateTime now = QDateTime::currentDateTimeUtc();

    for (auto it = toCheck.cbegin(); it != toCheck.cend(); ++it) {
        const Watermark current = sample(it.key());
        const bool longStopped =
            it.value().watermark.sampledAt.msecsTo(now) >= m_longStopMs;
        if (current.differsFrom(it.value().watermark) ||
            // Known good records are polled forever in case they are
            // overwritten
            (longStopped && it.value().allowLongStop)) {
            // If the file hasn't changed in considerable time then give it one
            // more chance.
            emit pathChanged(it.key());
        } else {
            // Re-enqueued against the *original* watermark so neither the
            // comparison baseline nor the long-stop clock resets every poll.
            waitForChange(it.key(), it.value().watermark,
                          it.value().allowLongStop);
        }
    }
}

void Deferred::stop() { m_trigger->stop(); }

}  // namespace KWLegionCore
