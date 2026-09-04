// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Muonic

#pragma once

#include <legionparser/exception.h>
#include <legionparser/replay.h>

#include <QByteArrayView>
#include <QDataStream>
#include <QDateTime>
#include <QIODevice>
#include <QString>
#include <concepts>
#include <type_traits>

namespace LegionParser {

class Reader;

class SynopsisParser {
   public:
    SynopsisParser(const SynopsisParser&) = delete;
    SynopsisParser& operator=(const SynopsisParser&) = delete;
    SynopsisParser(SynopsisParser&&) = delete;
    SynopsisParser& operator=(SynopsisParser&&) = delete;

    virtual ~SynopsisParser();

    /**
     * @brief Parse a replay file from the given QIODevice and record the
     * metadata.
     *
     * The replay file will not be closed. In the event that an exception is
     * thrown
     *
     * @param replayFile the file to parse from
     * @return The parsed replay metadata
     * @throws ReplayParseException describing the parsing failure
     *
     */
    static ReplaySynopsis parse(QIODevice& replayFile);

    /**
     * @brief Cheaply test whether replayFile ends in a replay footer.
     *
     * Reads only the file's tail, so callers can skip a full parse - which
     * fingerprints the entire payload - on a replay the game is still
     * writing. This is a one-sided test: false means the file definitely
     * isn't finished, true only means it wasn't ruled out, and parse() stays
     * the authority on whether it is actually valid. A device that can't be
     * inspected (sequential, unseekable, changing underneath us) answers
     * true so the caller falls through to parse().
     *
     * The device's position is restored before returning.
     *
     * @param replayFile the file to inspect
     * @return false when the file definitely has no footer yet
     */
    static bool looksComplete(QIODevice& replayFile);

   private:
    explicit SynopsisParser(QIODevice&);

    [[nodiscard]] const ReplaySynopsis& metadata() const { return m_metadata; }

    void parse();

    void checkMagic();
    void parseHeader();

    void parseGameType();

    void parseVersions();

    void parseCommentaryFlag();

    void parseMatchStrings();

    void parsePlayers();

    Player parseOnePlayer();

    void parseOffsetAndMagic();

    void parseHeaderTail();

    // Extracts the "M=" map reference from the start of the header's
    // GameInfo text - a path-like reference into the game's .big archives -
    // and stores it in m_metadata.mapReference.
    void parseMapReference(QStringView header);

    // Extracts the ";S=" player-slot list from the header text and assigns
    // each slot's name (where present) and faction onto the corresponding
    // entry in m_metadata.players, matched positionally in slot order.
    void parsePlayerSlots(QStringView header);

    void parseBody();

    // Validates that lastChunk - the final chunk read while fingerprinting
    // the body - ends with a semantically valid "C&C3 REPLAY FOOTER"
    // structure. This lets us distinguish a torn read (e.g. parsing a
    // replay the game is still actively writing, which truncates the file
    // before the footer is appended) from other forms of corruption.
    void verifyFooter(QByteArrayView lastChunk) const;

    std::unique_ptr<Reader> m_reader;
    ReplaySynopsis m_metadata;

    // Used during parsing to describe the length of the header starting at the
    // magic string CNC3RPL\0
    size_t m_offset;
};
}  // namespace LegionParser