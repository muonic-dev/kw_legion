/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Muonic
 */

#pragma once

#include <QList>
#include <QtTypes>
#include <memory>

class QIODevice;

namespace LegionParser {

class Reader;
class ChunkAnalyzer;

/**
 * An AnalysisParser drives an Analyzer of the chunk/framing
 * structure of a replay
 */
class AnalysisParser {
   public:
    AnalysisParser(const AnalysisParser&) = delete;
    AnalysisParser& operator=(const AnalysisParser&) = delete;
    AnalysisParser(AnalysisParser&&) = delete;
    AnalysisParser& operator=(AnalysisParser&&) = delete;

    virtual ~AnalysisParser();

    /**
     * @brief Perform an analysis of the replayFile by passing to the given
     * listener
     */
    static void parse(QIODevice& replayFile, qsizetype offset,
                      ChunkAnalyzer& analyzer);

   private:
    AnalysisParser(QIODevice& replayFile, qsizetype offset,
                   ChunkAnalyzer& analyzer);

    std::unique_ptr<Reader> m_reader;
    qsizetype m_offset;
    AnalysisParser& m_analyzer;
};

}  // namespace LegionParser