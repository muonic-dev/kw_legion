/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Muonic
 */

#include "replayanalyzerbridge.h"

#include <QFuture>
#include <QMetaObject>
#include <QPromise>
#include <utility>

#include "replayanalyzer.h"

namespace KWLegionCore {
ReplayAnalyzerBridge::ReplayAnalyzerBridge(ReplayAnalyzer* analyzer,
                                           QObject* parent)
    : QObject(parent), m_analyzer(analyzer) {}

QFuture<AnalysisResult> ReplayAnalyzerBridge::analyze(
    const QByteArray& checksum) {
    QPromise<AnalysisResult> p;
    QFuture<AnalysisResult> fut = p.future();

    p.start();
    QMetaObject::invokeMethod(
        m_analyzer,
        [analyzer = m_analyzer, p = std::move(p),
         checksum = QByteArray(checksum)]() mutable {
            try {
                AnalysisResult result = analyzer->analyze(checksum);
                p.addResult(std::move(result));
            } catch (std::runtime_error& err) {
                p.addResult(AnalysisFailure::Unknown);
            }
            p.finish();
        },
        Qt::QueuedConnection);
    return fut;
}
}  // namespace KWLegionCore