#pragma once

#include <legionparser/exception.h>
#include <legionparser/legionparser_export.h>
#include <legionparser/replay.h>

#include <QDataStream>
#include <QDateTime>
#include <QIODevice>
#include <QString>
#include <concepts>
#include <type_traits>

namespace LegionParser {

class Reader;

class Parser {
   public:
    Parser(const Parser&) = delete;
    Parser& operator=(const Parser&) = delete;
    Parser(Parser&&) = delete;
    Parser& operator=(Parser&&) = delete;

    virtual ~Parser();

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
    static LEGIONPARSER_EXPORT ReplayMetadata parse(QIODevice& replayFile);

   private:
    explicit Parser(QIODevice&);

    [[nodiscard]] const ReplayMetadata& metadata() const { return m_metadata; }

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

    static Faction factionFromRaw(int raw);

    void parseBody();

    std::unique_ptr<Reader> m_reader;
    ReplayMetadata m_metadata;

    // Used during parsing to describe the length of the header starting at the
    // magic string CNC3RPL\0
    size_t m_offset;
};
}  // namespace LegionParser