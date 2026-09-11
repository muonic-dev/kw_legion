/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Muonic
 */

#include "apmanalyzer.h"

namespace KWLegionCore {
ApmAnalyzer::ApmAnalyzer(quint32 engineTicksPerSec, quint32 secWindow,
                         quint32 players)
    : m_ticksPerSec(engineTicksPerSec), m_windowSec(secWindow) {
    m_leadingActionCounts.fill(0, players);
    m_trailingActionCounts.fill(0, players);
    m_commited.fill(QList<QPointF>(), players);
}

void ApmAnalyzer::ingest(quint32 timecode,
                         const QSpan<LegionParser::Command> commands) {
    /* Recieve a chunk of commands, here's how we process
     * We have a ticks/s and a window duration in seconds
     * We take the current timecode and determine it is in the current window.
     * If it is, we increment leadingActionCount
     * If it is not, we close the window and generate a new QPoint per player
     * by the rate of m_leading - m_trailing as an accumulation
     * If multiple windows close due to this timecode we generate multiple
     * points */

    // Which window are we intending to fill with these commands. ticksPerSec
    // converts timecode to elapsed seconds; dividing that by windowSec turns
    // elapsed seconds into an actual window index (done as one division,
    // rather than two, to avoid compounding truncation).
    const quint32 timecodeWindow = timecode / (m_ticksPerSec * m_windowSec);
    // Commit as many as we need to commit because we advanced out of the
    // current window
    // m_commited is the set of windows that we are finished with
    // m_trailingActionCounts is the total at window entry
    // m_leadingActionCounts is what we are accumulating
    // If the timecode window is greater than the number of windows already
    // committed we have caught up
    // If nothing happens for many seconds this must loop
    while (timecodeWindow > m_committedWindows) {
        // What is the delta between start and end for each player
        for (int player = 0; player < m_trailingActionCounts.size(); player++) {
            const quint32 delta = m_leadingActionCounts.at(player) -
                                  m_trailingActionCounts.at(player);
            const double apm = (static_cast<double>(delta) /
                                static_cast<double>(m_windowSec)) *
                               60.0;
            const double point =
                static_cast<double>(m_committedWindows * m_windowSec) +
                // Center the point in the middle of the window
                (static_cast<double>(m_windowSec) / 2.0);
            m_commited[player].emplace_back(point, apm);
        }
        m_trailingActionCounts = m_leadingActionCounts;
        m_committedWindows++;
    }

    for (const auto& cmd : commands) {
        const std::optional<uint> playerIdx =
            LegionParser::unmanglePlayerIdx(cmd.mangledPlayerIdx);
        // Invalid player cases
        if (!playerIdx.has_value()) {
            continue;
        }
        if (*playerIdx >= m_leadingActionCounts.size()) {
            continue;
        }
        m_leadingActionCounts[*playerIdx]++;
    }
}

QList<QList<QPointF>> ApmAnalyzer::plot() const { return m_commited; }
}  // namespace KWLegionCore