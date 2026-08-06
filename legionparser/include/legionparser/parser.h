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

/**
 * @brief Base class for listening to events that occur during the parsing of
 * the replay
 *
 */
class LEGIONPARSER_EXPORT ParserEventListener {
   public:
    ParserEventListener() = default;
    ParserEventListener(const ParserEventListener&) = delete;
    ParserEventListener(ParserEventListener&&) = delete;

    ParserEventListener& operator=(const ParserEventListener&) = delete;
    ParserEventListener& operator=(ParserEventListener&&) = delete;

    virtual ~ParserEventListener() = default;

    virtual void onError(const ReplayParseException& exc);

    virtual void onHeaderParsed(const ReplayMetadata& exc);
};

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
     * @param eventListener a listener as parse events occur
     * @return whether the parsing event was successful. the precise error will
     * be sent to the listener
     */
    static LEGIONPARSER_EXPORT void parse(QIODevice& replayFile,
                                          ParserEventListener& eventListener);

   private:
    Parser(QIODevice&, ParserEventListener&);

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

    // Extracts the ";S=" player-slot list from the header text and assigns
    // each slot's name (where present) and faction onto the corresponding
    // entry in m_metadata.players, matched positionally in slot order.
    void parsePlayerSlots(const QString& header);

    static Faction factionFromRaw(int raw);

    void parseBody();

    std::unique_ptr<Reader> m_reader;
    ParserEventListener& m_evListener;
    ReplayMetadata m_metadata;

    // Used during parsing to describe the length of the header starting at the
    // magic string CNC3RPL\0
    size_t m_offset;
};
}  // namespace LegionParser