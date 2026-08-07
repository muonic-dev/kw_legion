#include <legionparser/exception.h>
#include <legionparser/parser.h>

#include <QByteArrayView>
#include <QCryptographicHash>
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

//  The S= string has the faction field at 6 and the computer has it at 3
constexpr qsizetype HUMAN_FACTION_SLOT_FIELD = 5;
constexpr qsizetype COMP_FACTION_SLOT_FIELD = 2;

// The largest valid replay is a few MB at most; this bounds how much of a
// corrupt/malicious file we'll read while fingerprinting the payload,
// rather than trusting the device to eventually hit a real EOF.
constexpr qint64 BODY_READ_CHUNK_SIZE = static_cast<qint64>(16 * 1024);
constexpr size_t MAX_BODY_SIZE = static_cast<size_t>(32 * 1024 * 1024);

Parser::Parser(QIODevice& replayFile)
    : m_reader(std::make_unique<Reader>(replayFile)),
      m_metadata{},
      m_offset{0} {}

Parser::~Parser() = default;

void Parser::parse() {
    checkMagic();
    parseHeader();
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
    constexpr auto factionMin = static_cast<int>(Faction::GDI);
    constexpr auto factionMax = static_cast<int>(Faction::Traveler);
    if (raw < factionMin || factionMax < raw) {
        return Faction::Unknown;
    }
    return static_cast<Faction>(raw);
}

void Parser::parseMapReference(const QStringView header) {
    // The header's GameInfo text begins with "M=" followed by a numeric
    // (short-range, hex-encoded per the format doc) unknown value of
    // variable width - it can contain hex letters (e.g. a trailing 'b'),
    // so its exact width/character class isn't reliable to scan for.
    // Empirically (107 real replays), the map reference that follows always
    // begins with "data/", which is a far more reliable anchor.
    constexpr QLatin1String mapMarker("M=");
    constexpr QLatin1String pathAnchor("data/");

    if (!header.startsWith(mapMarker)) {
        throw CorruptDataException(QString("header missing map reference"),
                                   m_reader->lastOffset());
    }

    const qsizetype mapStart =
        header.indexOf(pathAnchor, mapMarker.size(), Qt::CaseInsensitive);
    if (mapStart < 0) {
        throw CorruptDataException(
            QString("header map reference missing data/ prefix"),
            m_reader->lastOffset());
    }

    const qsizetype mapEnd = header.indexOf(QLatin1Char(';'), mapStart);
    if (mapEnd < 0) {
        throw CorruptDataException(
            QString("header map reference missing terminator"),
            m_reader->lastOffset());
    }

    m_metadata.mapReference =
        header.sliced(mapStart, mapEnd - mapStart).toString();
}

void Parser::parsePlayerSlots(const QStringView header) {
    // The header's ";S=" key holds a colon-separated list of player slots,
    // e.g. "S=HMuonic,0,0,TT,4,10,-1,-1,0,1,-1,:CB,-1,11,-1,-1,0,4:X:X:...;"
    // Each slot starts with a type letter: H (human), C (computer), or X
    // (empty/unused, skipped). Search for ";S=" rather than "S=" alone,
    // since other keys like "MS=0" contain "S=" as a substring.
    // The human player has their faction marker in the 6th field and the
    // computer player has its faction marker in the 3rd field
    const QLatin1String slotMarker(";S=");
    qsizetype slotStart = header.indexOf(slotMarker);
    if (slotStart < 0) {
        throw CorruptDataException(QString("header missing player slot list"),
                                   m_reader->lastOffset());
    }
    // Move past the starting token
    slotStart += slotMarker.size();

    for (auto player = m_metadata.players.begin();
         // TODO: breaking due to slotStart running off the end before we've
         // marked all the players should be tracked/warned
         slotStart < header.size() && player < m_metadata.players.end();
         player++) {
        const qsizetype playerIdx{player - m_metadata.players.begin()};

        // find the end of the current slot. This is either the following ':',
        // the next ';', or the end of the header.
        qsizetype slotEnd = header.indexOf(QString(":"), slotStart);
        if (slotEnd == -1) {
            slotEnd = header.indexOf(QString(";"), slotStart);
        }
        if (slotEnd == -1) {
            slotEnd = header.size();
        }
        const QStringView slotView =
            QStringView(header).sliced(slotStart, slotEnd - slotStart);

        // Decode the player data
        const QChar slotType = slotView.at(0);
        qsizetype factionField{};
        if (slotType == QLatin1Char('H')) {
            factionField = HUMAN_FACTION_SLOT_FIELD;
            player->isComputer = false;
        } else if (slotType == QLatin1Char('C')) {
            factionField = COMP_FACTION_SLOT_FIELD;
            player->isComputer = true;
        } else {
            // TODO: see about continuing with the partial data here
            throw CorruptDataException(
                QString("player %1 type is not recognized").arg(playerIdx),
                m_reader->lastOffset());
        }

        const QList<QStringView> fields = slotView.split(QChar(','));

        if (std::cmp_less_equal(fields.size(), factionField)) {
            throw CorruptDataException(
                QString("player %1 slot missing expected fields")
                    .arg(playerIdx),
                m_reader->lastOffset());
        }

        bool ok = false;
        const int factionOrdinal = fields.at(factionField).toInt(&ok);
        if (!ok) {
            throw CorruptDataException(
                QString("player type %s non-numeric").arg(playerIdx),
                m_reader->lastOffset());
        }
        player->faction = factionFromRaw(factionOrdinal);

        // At the end, prepare for the next path
        slotStart = slotEnd + 1;
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
    parseMapReference(header);
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

void Parser::parseBody() {
    // The payload isn't parsed at this time; just fingerprint it so callers
    // can cheaply compare/identify replay content. Read in bounded chunks
    // rather than the whole remaining file at once, so a corrupt or
    // maliciously oversized file can't force unbounded memory use.
    QCryptographicHash hash(QCryptographicHash::Sha256);
    size_t totalSize = 0;
    m_reader->readRemainingChunked(
        [&](QByteArrayView chunk) {
            totalSize += static_cast<size_t>(chunk.size());
            if (totalSize > MAX_BODY_SIZE) {
                throw LimitExceededException(QString("replay payload"),
                                             m_reader->offset(), MAX_BODY_SIZE,
                                             totalSize);
            }
            hash.addData(chunk);
        },
        BODY_READ_CHUNK_SIZE);

    m_metadata.payloadChecksum = hash.result();
}

ReplayMetadata Parser::parse(QIODevice& replayFile) {
    Parser parser{replayFile};
    parser.parse();
    return parser.metadata();
}
}  // namespace LegionParser