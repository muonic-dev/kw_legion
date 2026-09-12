/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Muonic
 */

#pragma once

#include <legionparser/chunkanalyzer.h>

#include <QList>
#include <QPointF>
#include <QtTypes>

namespace KWLegionCore {
class ApmAnalyzer {
   public:
    ApmAnalyzer(quint32 engineTicksPerSec, quint32 secWindow, quint32 players);

    void ingest(quint32 timecode, QSpan<LegionParser::Command> commands);

    [[nodiscard]] QList<QList<QPointF>> plot() const;

   private:
    quint32 m_ticksPerSec;
    quint32 m_windowSec;
    // How many windows have been closed so far - independent of
    // m_commited's own size, which is fixed at construction time (one
    // per-player entry, not one per window).
    quint32 m_committedWindows = 0;

    QList<quint32> m_trailingActionCounts;
    QList<quint32> m_leadingActionCounts;
    QList<QList<QPointF>> m_commited;
};
}  // namespace KWLegionCore