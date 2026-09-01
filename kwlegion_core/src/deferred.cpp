/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Muonic
 */

#include "deferred.h"

#include <QFileDevice>
#include <QFileInfo>

namespace KWLegionCore {
namespace {
const long TIMER_MS = 1000;
const long WAIT_FOR_SETTLED_MS = 15000;

bool isReady(const QDateTime& currentTimeUTC, const QFileInfo& pathInfo) {
    // A file might ready for parsing when it has size > 0 and it has been
    // written more than 5s in the past. It may still not be ready depending on
    // how fast KW is writting.
    // We use UTC because there are no time changes in UTC
    const QDateTime cutoffTime = currentTimeUTC.addMSecs(-WAIT_FOR_SETTLED_MS);
    return pathInfo.size() > 0 &&
           pathInfo.fileTime(QFileDevice::FileModificationTime,
                             QTimeZone::UTC) < cutoffTime;
}
}  // namespace

Deferred::Deferred(QObject* parent)
    : QObject(parent),
      m_recheckIntervalMs(TIMER_MS),
      m_trigger(new QTimer(this)) {
    m_trigger->setSingleShot(true);
    m_trigger->callOnTimeout(this, &Deferred::timerFired);
}

bool Deferred::readyForParsing(const QString& path) {
    const QFileInfo pathInfo(path);
    return isReady(QDateTime::currentDateTimeUtc(), pathInfo);
}

void Deferred::setRecheckIntervalMs(long ms) { m_recheckIntervalMs = ms; }

void Deferred::waitForReady(const QString& path) {
    m_deferred.insert(path);
    if (!m_trigger->isActive()) {
        m_trigger->start(m_recheckIntervalMs);
    }
}

void Deferred::removeWaitForReady(const QString& path) {
    m_deferred.remove(path);
    if (m_deferred.isEmpty()) {
        m_trigger->stop();
    }
}

void Deferred::timerFired() {
    const QSet<QString> toCheck = std::move(m_deferred);

    for (const auto& path : toCheck) {
        if (readyForParsing(path)) {
            emit pathReady(path);
        } else {
            waitForReady(path);
        }
    }
}

QSet<QString> Deferred::currentPaths() { return m_deferred; }

void Deferred::stop() { m_trigger->stop(); }

}  // namespace KWLegionCore