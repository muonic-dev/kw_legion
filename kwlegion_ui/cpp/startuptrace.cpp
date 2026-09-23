// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Muonic

#include "startuptrace.h"

#include <QDebug>
#include <QtLogging>
#ifdef Q_OS_WIN
#include <qt_windows.h>
#endif
#include <cstdint>

namespace {

std::optional<qint64> processAgeMilliseconds() {
#ifdef Q_OS_WIN
    FILETIME creationTime{};
    FILETIME exitTime{};
    FILETIME kernelTime{};
    FILETIME userTime{};
    if (!GetProcessTimes(GetCurrentProcess(), &creationTime, &exitTime,
                         &kernelTime, &userTime)) {
        return std::nullopt;
    }

    FILETIME currentTime{};
    GetSystemTimePreciseAsFileTime(&currentTime);

    const auto ticks = [](const FILETIME& time) {
        return (static_cast<std::uint64_t>(time.dwHighDateTime) << 32U) |
               time.dwLowDateTime;
    };
    const std::uint64_t creationTicks = ticks(creationTime);
    const std::uint64_t currentTicks = ticks(currentTime);
    if (currentTicks < creationTicks) {
        return std::nullopt;
    }

    // FILETIME is expressed in 100 ns ticks.
    return static_cast<qint64>((currentTicks - creationTicks) / 10'000U);
#else
    return std::nullopt;
#endif
}

}  // namespace

namespace KWLegionUI {

StartupTrace::StartupTrace()
    : m_started(std::chrono::steady_clock::now()),
      m_processToMainMs(processAgeMilliseconds()) {}

void StartupTrace::mark(const QString& milestone) {
    const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                             std::chrono::steady_clock::now() - m_started)
                             .count();

    const QMutexLocker locker(&m_guard);
    const Entry entry{milestone, elapsed, elapsed - m_lastElapsed};
    m_lastElapsed = elapsed;
    if (m_loggingReady) {
        log(entry);
    } else {
        m_pending.append(entry);
    }
}

void StartupTrace::enableLogging() {
    const QMutexLocker locker(&m_guard);
    m_loggingReady = true;

    if (m_processToMainMs.has_value()) {
        qInfo().nospace() << "startup phase=process_to_main duration_ms="
                          << *m_processToMainMs;
    }
    for (const auto& entry : m_pending) {
        log(entry);
    }
    m_pending.clear();
}

void StartupTrace::log(const Entry& entry) {
    qInfo().noquote().nospace()
        << "startup milestone=" << entry.milestone
        << " total_ms=" << entry.elapsedMs << " delta_ms=" << entry.deltaMs;
}

}  // namespace KWLegionUI
