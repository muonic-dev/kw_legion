/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Muonic
 */

#pragma once

#include <QList>
#include <QtTypes>

class QIODevice;

namespace LegionParser {

class ChunkAnalyzer;

/**
 * Analyze the replay stored in replayFile using the given chunk analyzer
 *
 * This wil repeatedly read chunks from the replayFile and feed them to the
 * analyzer. Assumes that replayFile is positioned before the start of the first
 * body chunk
 *
 * Will throw any additional exceptions that analyzer does, however, well
 * formed analyzers won't throw exceptions given the command framing is in flux.
 *
 * @param replayFile the replay file
 * @param bodyOffset the position of the first body chunk after the replay
 * header
 * @param analyzer the chunk analyzer passed in
 *
 * @throws ReplayParseException if there is a parsing failure of the body
 * framing
 *
 *
 */
void analyzeReplay(QIODevice& replayFile,
                   // We need to manually pass offset to align the internal
                   // offset in the Reader object
                   qsizetype bodyOffset, ChunkAnalyzer& analyzer);

}  // namespace LegionParser