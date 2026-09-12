/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Muonic
 */

#include <kwlegion_core/replayanalyzer.h>
#include <kwlegion_core/replaystore.h>
#include <legionparser/analysisparser.h>
#include <legionparser/chunkanalyzer.h>
#include <legionparser/exception.h>

#include <QDebug>
#include <QLoggingCategory>
#include <ranges>

#include "apmanalyzer.h"

Q_LOGGING_CATEGORY(logAnalyzer, "kwlegion.analyzer");

namespace KWLegionCore {
ReplayAnalyzer::ReplayAnalyzer(
    const ReplayAnalysisTargetProvider& targetProvider, QObject* parent)
    : QObject(parent), m_targetProvider(targetProvider) {}

AnalysisResult ReplayAnalyzer::analyze(const QByteArray& checksum) {
    std::optional<ReplayAnalysisTarget> target =
        m_targetProvider.lookupReplay(checksum);
    if (!target) {
        return AnalysisFailure::MissingReplay;
    }

    // No players: maybe happens when the game crashes during startup
    if (target->replay.players.isEmpty()) {
        return ReplayAnalysis{.apmPlot = QList<QList<QPointF>>()};
    }

    // Same logic here that we have in the ReplayModel population
    // The commentator player gets a slot at the end but has no
    if (target->replay.players.back().name.isEmpty()) {
        target->replay.players.pop_back();
    }

    ApmAnalyzer apmAnalyzer{
        15, 5, static_cast<quint32>(target->replay.players.size())};

    LegionParser::CommandFrameAnalyzer commandFramer{
        [&apmAnalyzer](quint32 timecode,
                       QSpan<LegionParser::Command> commands) -> bool {
            apmAnalyzer.ingest(timecode, commands);
            return true;
        },
        [checksum](LegionParser::DesyncDetails details) {
            qCWarning(logAnalyzer)
                << "Desync detected analyzing replay" << checksum.toHex()
                << "cause:" << static_cast<int>(details.cause)
                << "chunkOffset:" << details.chunkOffset
                << "relativeCommandOffset:" << details.relativeCommandOffset
                << "command:" << static_cast<int>(details.command);
        }};

    QFile replayFile(target->path);
    if (!replayFile.open(QIODevice::ReadOnly)) {
        qCWarning(logAnalyzer) << "Unable to open replay: " << target->path
                               << " " << replayFile.errorString();
        return AnalysisFailure::MissingReplay;  // or invalid?
    }
    try {
        LegionParser::analyzeReplay(replayFile, target->replay.bodyOffset,
                                    commandFramer);

        return ReplayAnalysis{.apmPlot = apmAnalyzer.plot()};
    } catch (LegionParser::ReplayParseException& ex) {
        qCWarning(logAnalyzer)
            << "Replay parsing failure: " << target->path << " " << ex.what();
        return AnalysisFailure::InvalidReplay;
    }
}

}  // namespace KWLegionCore
