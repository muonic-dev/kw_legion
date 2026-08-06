#include <legionparser/exception.h>
#include <legionparser/parser.h>

#include <QDataStream>
#include <QString>
#include <QStringList>
#include <QTimeZone>
#include <bit>
#include <cstdint>
#include <utility>

#include "reader.h"

namespace LegionParser {

constinit const char* const CNC_MAGIC = "C&C3 REPLAY HEADER";
constexpr std::size_t MAGIC_SIZE = 18;
constexpr std::size_t U1_SIZE = 33;
constexpr std::size_t U2_SIZE = 19;
constexpr std::size_t DATETIME_STRING_LENGTH = 8;
constexpr char MAX_PLAYERS = 8;

constexpr const char* const REPL_MAGIC = "CNC3RPL";
constexpr std::size_t REPL_MAGIC_SIZE = 8;

Parser::Parser(QIODevice& replayFile, ParserEventListener& listener)
    : m_reader(std::make_unique<Reader>(replayFile)),
      m_evListener(listener),
      m_metadata{},
      m_offset{} {}

Parser::~Parser() = default;

void Parser::parse() {
    checkMagic();
    parseHeader();
    m_evListener.onHeaderParsed(m_metadata);
    parseBody();
}

void Parser::checkMagic() {
    const QByteArray magic = m_reader->readBlock(MAGIC_SIZE);
    if (magic != QByteArray(CNC_MAGIC, MAGIC_SIZE)) {
        throw CorruptDataException(QString("replay magic"),
                                   m_reader->offset() - MAGIC_SIZE);
    }
}

void Parser::parseHeader() {
    parseGameType();
    parseVersions();
    parseCommentaryFlag();
    parseMatchStrings();
    parsePlayers();
    parseOffsetAndMagic();
    parseHeaderTail();

    const size_t actualOffset = m_reader->offset() - m_reader->mark();
    // Validate that we read the correct length
    if (m_offset != actualOffset) {
        throw CorruptDataException(
            QString("header length did not match recorded offset: was %1 "
                    "expected %2")
                .arg(actualOffset)
                .arg(m_offset),
            m_reader->offset());
    }
}

void Parser::parseGameType() {
    // Immediately following the magic is a header that appears to signify
    // skirmish (0x04) or multiplayer (0x05)
    switch (m_reader->readByte<GameType>()) {
        case GameType::Skirmish:
            m_metadata.gameType = GameType::Skirmish;
            break;
        case GameType::Multiplayer:
            m_metadata.gameType = GameType::Multiplayer;
            break;
        default:
            m_metadata.gameType = GameType::Unknown;
    }
}

void Parser::parseVersions() {
    // Following the match type there are a series of build sequences
    // These are all basically the same always, but worth tracking
    m_reader->withDataStream([this](QDataStream& stream) {
        m_metadata.versionMajor = m_reader->readIntegral<std::uint32_t>(stream);
        m_metadata.versionMinor = m_reader->readIntegral<std::uint32_t>(stream);
        m_metadata.buildMajor = m_reader->readIntegral<std::uint32_t>(stream);
        m_metadata.buildMinor = m_reader->readIntegral<std::uint32_t>(stream);
    });
}

void Parser::parseCommentaryFlag() {
    // Following the build numbers is a byte indicating whether the replay
    // includes commentary (0x06 no commentary, 0x1E with commentary),
    // followed by a reserved byte that is always 0x00
    const auto hnum2 = m_reader->readByte<uint8_t>();
    switch (hnum2) {
        case 0x06:
            m_metadata.hasCommentary = false;
            break;
        case 0x1E:
            m_metadata.hasCommentary = true;
            break;
        default:
            // TODO: signal this as a warning once ParserEventListener grows
            // a warning-level callback; an unrecognized value here isn't
            // fatal enough to treat as an error via onError.
            m_metadata.hasCommentary = false;
    }

    const auto zero1 = m_reader->readByte<uint8_t>();
    if (zero1 != 0x0) {
        throw CorruptDataException(QString("reserved byte"),
                                   m_reader->lastOffset());
    }
}

void Parser::parseMatchStrings() {
    m_metadata.matchTitle = m_reader->readUtf16String();
    m_metadata.matchDescription = m_reader->readUtf16String();
    m_metadata.mapName = m_reader->readUtf16String();
    m_metadata.mapId = m_reader->readUtf16String();
}

void Parser::parsePlayers() {
    const auto playerCount = m_reader->readByte<uint8_t>();
    if (playerCount == 0) {
        throw CorruptDataException(QString("Player count is 0"),
                                   m_reader->lastOffset());
    }
    if (playerCount > MAX_PLAYERS) {
        throw CorruptDataException(QString("Player count is improbably large"),
                                   m_reader->lastOffset());
    }
    for (uint8_t i = 0; i < playerCount; i++) {
        m_metadata.players.append(parseOnePlayer());
    }
    // Parse the final commentator player that always appears to exist
    m_metadata.players.append(parseOnePlayer());
}

Player Parser::parseOnePlayer() {
    const auto playerId = m_reader->readIntegral<std::uint32_t>();

    const QString playerName = m_reader->readUtf16String();
    int8_t teamNumber = 0;
    if (m_metadata.gameType == GameType::Multiplayer) {
        teamNumber = m_reader->readByte<std::int8_t>();
    }
    return Player{.playerId = playerId,
                  .playerName = playerName,
                  .teamNumber = teamNumber};
}

Faction Parser::factionFromRaw(int raw) {
    constexpr auto FACTION_MIN = static_cast<int>(Faction::GDI);
    constexpr auto FACTION_MAX = static_cast<int>(Faction::Traveler);
    if (raw < FACTION_MIN || raw > FACTION_MAX) {
        return Faction::Unknown;
    }
    return static_cast<Faction>(raw);
}

void Parser::parsePlayerSlots(const QString& header) {
    // The header's ";S=" key holds a colon-separated list of player slots,
    // e.g. "S=HMuonic,0,0,TT,4,10,-1,-1,0,1,-1,:CB,-1,11,-1,-1,0,4:X:X:...;"
    // Each slot starts with a type letter: H (human), C (computer), or X
    // (empty/unused, skipped). Search for ";S=" rather than "S=" alone,
    // since other keys like "MS=0" contain "S=" as a substring.
    const QLatin1String slotMarker(";S=");
    const auto slotsStart = header.indexOf(slotMarker);
    if (slotsStart < 0) {
        throw CorruptDataException(QString("header missing player slot list"),
                                   m_reader->lastOffset());
    }

    QString slotsText = header.mid(slotsStart + slotMarker.size());
    if (slotsText.endsWith(QLatin1Char(';'))) {
        slotsText.chop(1);
    }

    struct SlotInfo {
        QString name;
        Faction faction;
    };
    QList<SlotInfo> slotInfos;

    const QStringList slotTokens = slotsText.split(QLatin1Char(':'));
    for (const QString& slot : slotTokens) {
        if (slot.isEmpty()) {
            continue;
        }
        const QChar slotType = slot.at(0);
        const QStringList fields = slot.mid(1).split(QLatin1Char(','));

        // The faction field's position differs by slot type: field 5 for a
        // human slot (field 0 is the player name), field 2 for a computer
        // slot (field 0 is a difficulty code, no name present).
        std::size_t factionField = 0;
        QString name;
        if (slotType == QLatin1Char('H')) {
            factionField = 5;
            name = fields.value(0);
        } else if (slotType == QLatin1Char('C')) {
            factionField = 2;
        } else {
            // X (empty) or any other/unrecognized slot type
            continue;
        }

        if (static_cast<std::size_t>(fields.size()) <= factionField) {
            throw CorruptDataException(
                QString("player slot missing expected fields"),
                m_reader->lastOffset());
        }

        bool ok = false;
        const int raw = fields.at(static_cast<qsizetype>(factionField)).toInt(&ok);
        if (!ok) {
            throw CorruptDataException(QString("player slot faction not numeric"),
                                       m_reader->lastOffset());
        }

        slotInfos.append(SlotInfo{.name = name, .faction = factionFromRaw(raw)});
    }

    if (slotInfos.size() != m_metadata.players.size()) {
        throw CorruptDataException(
            QString("player slot count did not match parsed player count"),
            m_reader->lastOffset());
    }

    for (qsizetype i = 0; i < slotInfos.size(); i++) {
        Player& player = m_metadata.players[i];
        player.faction = slotInfos.at(i).faction;
        if (!slotInfos.at(i).name.isEmpty()) {
            player.playerName = slotInfos.at(i).name;
        }
    }
}

void Parser::parseOffsetAndMagic() {
    m_offset = m_reader->readIntegral<uint32_t>();
    // Read this manually since we want to mark mid-read so we track it
    const auto strReplLength = m_reader->readIntegral<uint32_t>();
    if (strReplLength != REPL_MAGIC_SIZE) {
        throw CorruptDataException(QString("CNC3RPL magic incorrect length"),
                                   m_reader->lastOffset());
    }

    // We mark now because the offset is from the start of the magic string to
    // the first data block
    m_reader->setMark();

    const QByteArray strReplMagic = m_reader->readBlock(REPL_MAGIC_SIZE);

    if (strReplMagic != QByteArray(REPL_MAGIC, REPL_MAGIC_SIZE)) {
        throw CorruptDataException(QString("CNC3RPL magic incorrect value"),
                                   m_reader->lastOffset());
    }
}

void Parser::parseHeaderTail() {
    // no mod_info in kw
    const auto ts = m_reader->readIntegral<uint32_t>();
    m_metadata.replayTimestamp = QDateTime::fromSecsSinceEpoch(
        static_cast<qint64>(ts), QTimeZone(QTimeZone::UTC));

    // read and discard the unknown1 block
    m_reader->readBlock(U1_SIZE);

    // read the header
    const QString header = m_reader->readFixedCharString<uint32_t>();
    parsePlayerSlots(header);

    const auto replaySaver = m_reader->readByte<uint8_t>();
    if (replaySaver < m_metadata.players.size()) {
        (m_metadata.players.begin() + replaySaver)->isReplaySaver = true;
    }
    // TODO: else warn here, not worth throwing on corruption

    // read 2 zeroes
    m_reader->discardZero<uint32_t>();
    m_reader->discardZero<uint32_t>();

    m_metadata.filename = m_reader->readFixedUtf16String<uint32_t>();

    // Read and discard the datetime field
    // The replay timestamp has what we want and this one doesn't make sense
    m_reader->readFixedUtf16String(DATETIME_STRING_LENGTH);

    // Read and discard the vermagic thing
    m_reader->readFixedCharString<uint32_t>();

    // discard a magic hash
    m_reader->readIntegral<uint32_t>();

    // read and validate zero
    m_reader->discardZero<uint8_t>();

    // Read final block - U2_SIZE is a count of uint32_t elements, not bytes
    m_reader->readBlock(U2_SIZE * sizeof(std::uint32_t));
}

void Parser::parseBody() {}

void Parser::parse(QIODevice& replayFile, ParserEventListener& eventListener) {
    try {
        Parser(replayFile, eventListener).parse();
    } catch (const ReplayParseException& exc) {
        // TODO: Consider removing me in favor of catches
        eventListener.onError(exc);
        throw;
    }
}

void ParserEventListener::onError(const ReplayParseException& exc) {}

void ParserEventListener::onHeaderParsed(const ReplayMetadata& exc) {}
}  // namespace LegionParser