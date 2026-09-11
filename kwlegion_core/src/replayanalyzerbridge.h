/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Muonic
 */

#pragma once

#include <kwlegion_core/replayanalyzer.h>

#include <QFuture>
#include <QObject>


namespace KWLegionCore {

/**
 * The expectation is that the ReplayAnalyzer lives a different thread from the
 * replaystoremodel
 *
 * So, we solve this by a utility here that is responsible for proxying a call
 * onto the other object.
 * The bridge does not own the replayanalyzer
 */
class ReplayAnalyzerBridge : public QObject {
   public:
    ReplayAnalyzerBridge(ReplayAnalyzer* analyzer, QObject* parent = nullptr);

    QFuture<AnalysisResult> analyze(const QByteArray& checksum);

   private:
    ReplayAnalyzer* m_analyzer;
};
}  // namespace KWLegionCore