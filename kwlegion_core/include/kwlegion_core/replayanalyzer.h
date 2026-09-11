/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Muonic
 */

#pragma once

#include <QList>
#include <QObject>
#include <QPointF>

namespace KWLegionCore {

class ReplayAnalysisTargetProvider;

struct ReplayAnalysis {
    QList<QList<QPointF>> apmPlot;
};

enum class AnalysisFailure : std::uint8_t {
    MissingReplay,
    InvalidReplay,
    Unknown
};

using AnalysisResult = std::variant<AnalysisFailure, ReplayAnalysis>;

/**
 * The task that performs replay analysis
 *
 * This exists as a distinct class to avoid putting even more functionality into
 * the replaystore. It just need a ReplayAnalysisTargetProvider& (of which
 * replaystore is one) to find the replay to do analysis on.
 */
class ReplayAnalyzer : public QObject {
   public:
    ReplayAnalyzer(const ReplayAnalysisTargetProvider& targetProvider,
                   QObject* parent = nullptr);

    AnalysisResult analyze(const QByteArray& checksum);

   private:
    const ReplayAnalysisTargetProvider& m_targetProvider;
};

};  // namespace KWLegionCore