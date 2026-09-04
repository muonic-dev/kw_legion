// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Muonic

#include <legionparser/exception.h>
#include <legionparser/parser.h>

#include <QByteArrayView>
#include <QCryptographicHash>
#include <QDataStream>
#include <QList>
#include <QString>
#include <QStringList>
#include <QTimeZone>
#include <Qt>
#include <QtEndian>
#include <QtMinMax>
#include <QtTypes>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <utility>

#include "legionparser/replay.h"
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

// Team membership is sourced entirely from the S= slot text - see
// parseOnePlayer for why the binary team_number field (present only in
// multiplayer replays) is read past but not used. The team value lives at
// a different position depending on slot type field 8 for a human slot,
// field 5 for a computer slot and is encoded as (UI team number - 1), or
// -1 if no team was ever assigned. Skirmish replays with a known,
// deliberately configured team split (including explicit numeric team
// assignment via the lobby UI, and an all-FFA replay to check the -1 case)
// were diffed against replays with no allies to isolate these fields.
constexpr qsizetype HUMAN_TEAM_SLOT_FIELD = 7;
constexpr qsizetype COMP_TEAM_SLOT_FIELD = 4;

// -1 means "no team assigned" (FFA), not "team 0" - and every FFA slot
// shares this same raw -1, even though those slots are not allied with one
// another. ReplayModel groups players into teams by exact teamNumber
// equality, so mapping every FFA slot to one shared value would wrongly
// merge unrelated FFA players into a single team. Each FFA slot instead
// gets its own placeholder, offset comfortably above any real team number
// (MAX_PLAYERS caps how large a real one can be).
constexpr std::uint32_t UNALLIED_TEAM_BASE = 100;

// The largest valid replay is a few MB at most; this bounds how much of a
// corrupt/malicious file we'll read while fingerprinting the payload,
// rather than trusting the device to eventually hit a real EOF.
constexpr qint64 BODY_READ_CHUNK_SIZE = static_cast<qint64>(16 * 1024);
constexpr size_t MAX_BODY_SIZE = static_cast<size_t>(32 * 1024 * 1024);

// Footer format, per
// https://github.com/louisdx/cnc-replayreaders/blob/master/eareplay.html :
//   char footer_str[18];    // "C&C3 REPLAY FOOTER"
//   uint32_t final_time_code;
//   byte data[...];         // {0x02}, or {0x01, 0x02, uint32_t n, byte[n]}
//   uint32_t footer_length; // total byte length of this whole structure
constexpr const char* const FOOTER_MAGIC = "C&C3 REPLAY FOOTER";
constexpr qsizetype FOOTER_MAGIC_SIZE = 18;
constexpr qsizetype FOOTER_LENGTH_FIELD_SIZE =
    static_cast<qsizetype>(sizeof(uint32_t));
// Shortest possible footer: magic + final_time_code + the shortest data
// variant (a single 0x02 byte) + footer_length. data itself is variable
// length, so this is only a lower bound, not the footer's actual size.
constexpr qsizetype FOOTER_MIN_SIZE =
    FOOTER_MAGIC_SIZE + FOOTER_LENGTH_FIELD_SIZE + 1 + FOOTER_LENGTH_FIELD_SIZE;
// How much of the file's tail is enough to find the footer in. This is the
// largest footer either path will accept, so it has to be the same number in
// both: readRemainingChunked hands verifyFooter the last two chunks, and
// looksComplete seeks back by this much. Were looksComplete to read less, it
// would reject files that a full parse accepts - and since it short-circuits
// before the parser ever runs, its answer would win.
constexpr qint64 FOOTER_TAIL_WINDOW = 2 * BODY_READ_CHUNK_SIZE;

namespace {
quint32 readLE32(QByteArrayView data) {
    return qFromLittleEndian<quint32>(data.data());
}

// Locates the footer structure at the end of tail, which MUST be a view of
// the file's final bytes - every offset here is measured backwards from the
// end, so a view that stops short of EOF silently reads the wrong fields.
//
// This is only the cheap, structural half of footer validation: enough to
// tell "the file doesn't end in a footer yet" (the writer is still going)
// from "it does". Returns the footer's bytes when one is present, so callers
// that want to validate further don't have to recompute the bounds.
std::optional<QByteArrayView> footerFromTail(QByteArrayView tail) {
    if (std::cmp_less(tail.size(), FOOTER_LENGTH_FIELD_SIZE)) {
        return std::nullopt;
    }

    // footer_length is self-describing and lives in the final 4 bytes of
    // the file, so we can slice out exactly the footer's bytes directly
    // from the end rather than scanning for the magic string.
    const quint32 footerLength =
        readLE32(tail.sliced(tail.size() - FOOTER_LENGTH_FIELD_SIZE));
    if (std::cmp_less(footerLength, FOOTER_MIN_SIZE) ||
        std::cmp_greater(footerLength, tail.size())) {
        return std::nullopt;
    }

    const QByteArrayView footer =
        tail.last(static_cast<qsizetype>(footerLength));

    const auto magicView = QByteArrayView(FOOTER_MAGIC, FOOTER_MAGIC_SIZE);
    if (footer.first(magicView.size()) != magicView) {
        return std::nullopt;
    }

    return footer;
}
}  // namespace

SynopsisParser::SynopsisParser(QIODevice& replayFile)
    : m_reader(std::make_unique<Reader>(replayFile)),
      m_synopsis{},
      m_offset{0} {}

SynopsisParser::~SynopsisParser() = default;

void SynopsisParser::parse() {
    checkMagic();
    parseHeader();
    parseBody();
}

void SynopsisParser::checkMagic() {
    const QByteArray magic = m_reader->readBlock(MAGIC_SIZE);
    if (magic != QByteArray(CNC_MAGIC, MAGIC_SIZE)) {
        throw CorruptDataException(QLatin1String("replay magic"),
                                   m_reader->offset() - MAGIC_SIZE);
    }
}

void SynopsisParser::parseHeader() {
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
    // The current reader position is now the location that the body begins
    // Record is for use elsewhere
    m_synopsis.bodyOffset = m_reader->offset();
}

void SynopsisParser::parseGameType() {
    // Immediately following the magic is a header that appears to signify
    // skirmish (0x04) or multiplayer (0x05)
    m_synopsis.gameType = gameTypeFromUInt8(m_reader->readByte<std::uint8_t>());
}

void SynopsisParser::parseVersions() {
    // Following the match type there are a series of build sequences
    // These are all basically the same always, but worth tracking
    m_reader->withDataStream([this](QDataStream& stream) {
        m_synopsis.versionMajor = m_reader->readIntegral<std::uint32_t>(stream);
        m_synopsis.versionMinor = m_reader->readIntegral<std::uint32_t>(stream);
        m_synopsis.buildMajor = m_reader->readIntegral<std::uint32_t>(stream);
        m_synopsis.buildMinor = m_reader->readIntegral<std::uint32_t>(stream);
    });
}

void SynopsisParser::parseCommentaryFlag() {
    // Following the build numbers is a byte indicating whether the replay
    // includes commentary (0x06 no commentary, 0x1E with commentary),
    // followed by a reserved byte that is always 0x00
    const auto hnum2 = m_reader->readByte<uint8_t>();
    switch (hnum2) {
        case 0x06:
            m_synopsis.hasCommentary = false;
            break;
        case 0x1E:
            m_synopsis.hasCommentary = true;
            break;
        default:
            // TODO: signal this as a warning once ParserEventListener grows
            // a warning-level callback; an unrecognized value here isn't
            // fatal enough to treat as an error via onError.
            m_synopsis.hasCommentary = false;
    }

    const auto zero1 = m_reader->readByte<uint8_t>();
    if (zero1 != 0x0) {
        throw CorruptDataException(QLatin1String("reserved byte"),
                                   m_reader->lastOffset());
    }
}

void SynopsisParser::parseMatchStrings() {
    m_synopsis.matchTitle = m_reader->readUtf16String();
    m_synopsis.matchDescription = m_reader->readUtf16String();
    m_synopsis.mapName = m_reader->readUtf16String();
    m_synopsis.mapId = m_reader->readUtf16String();
}

void SynopsisParser::parsePlayers() {
    const auto playerCount = m_reader->readByte<uint8_t>();
    if (playerCount == 0) {
        throw CorruptDataException(QLatin1String("Player count is 0"),
                                   m_reader->lastOffset());
    }
    if (playerCount > MAX_PLAYERS) {
        throw CorruptDataException(
            QLatin1String("Player count is improbably large"),
            m_reader->lastOffset());
    }
    for (uint8_t i = 0; i < playerCount; i++) {
        m_synopsis.players.append(parseOnePlayer());
    }
    // Parse the final commentator player that always appears to exist
    m_synopsis.players.append(parseOnePlayer());
}

Player SynopsisParser::parseOnePlayer() {
    const auto id = m_reader->readIntegral<std::uint32_t>();
    const QString name = m_reader->readUtf16String();
    if (m_synopsis.gameType == GameType::Multiplayer) {
        // This byte exists here in multiplayer replays, but team is sourced
        // entirely from the S= slot text instead (see parsePlayerSlots): the
        // only replays that let us confirm real team semantics are 1v1
        // skirmishes, where slot text is unambiguous, while every real
        // multiplayer replay seen so far is also 1v1 and always carries -1
        // (not applicable) in the analogous slot field, leaving this byte's
        // behavior for an actual online team match unconfirmed. Still needs
        // to be consumed so the header's byte offset accounting stays
        // correct.
        m_reader->readByte<std::uint8_t>();
    }
    return Player{.id = id, .name = name};
}

void SynopsisParser::parseMapReference(const QStringView header) {
    // The header's GameInfo text begins with "M=" followed by a numeric
    // (short-range, hex-encoded per the format doc) unknown value of
    // variable width - it can contain hex letters (e.g. a trailing 'b'),
    // so its exact width/character class isn't reliable to scan for.
    // Empirically (107 real replays), the map reference that follows always
    // begins with "data/", which is a far more reliable anchor.
    constexpr QLatin1String mapMarker("M=");
    constexpr QLatin1String pathAnchor("data/");

    if (!header.startsWith(mapMarker)) {
        throw CorruptDataException(
            QLatin1String("header missing map reference"),
            m_reader->lastOffset());
    }

    const qsizetype mapStart =
        header.indexOf(pathAnchor, mapMarker.size(), Qt::CaseInsensitive);
    if (mapStart < 0) {
        throw CorruptDataException(
            QLatin1String("header map reference missing data/ prefix"),
            m_reader->lastOffset());
    }

    const qsizetype mapEnd = header.indexOf(QLatin1Char(';'), mapStart);
    if (mapEnd < 0) {
        throw CorruptDataException(
            QLatin1String("header map reference missing terminator"),
            m_reader->lastOffset());
    }

    m_synopsis.mapReference =
        header.sliced(mapStart, mapEnd - mapStart).toString();
}

void SynopsisParser::parsePlayerSlots(const QStringView header) {
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
        throw CorruptDataException(
            QLatin1String("header missing player slot list"),
            m_reader->lastOffset());
    }
    // Move past the starting token
    slotStart += slotMarker.size();

    for (auto player = m_synopsis.players.begin();
         // TODO: breaking due to slotStart running off the end before we've
         // marked all the players should be tracked/warned
         slotStart < header.size() && player < m_synopsis.players.end();
         player++) {
        const qsizetype playerIdx{player - m_synopsis.players.begin()};

        // find the end of the current slot. This is either the following ':',
        // the next ';', or the end of the header.
        qsizetype slotEnd = header.indexOf(QLatin1String(":"), slotStart);
        if (slotEnd == -1) {
            slotEnd = header.indexOf(QLatin1String(";"), slotStart);
        }
        if (slotEnd == -1) {
            slotEnd = header.size();
        }
        const QStringView slotView =
            QStringView(header).sliced(slotStart, slotEnd - slotStart);

        // We only ever get 8 players, the synthetic +1 player
        // just doesn't get parsed in this case
        if (slotView.isEmpty()) {
            if (playerIdx == m_synopsis.players.size() - 1) {
                // The synthetic commentator has no slot entry when the lobby is
                // full
                break;
            }
            throw CorruptDataException(
                QString("player %1 slot missing").arg(playerIdx),
                m_reader->lastOffset());
        }
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
        player->faction =
            (std::cmp_less(factionOrdinal, 0) ||
                     std::cmp_greater_equal(
                         factionOrdinal,
                         static_cast<std::uint8_t>(Faction::Unknown))
                 ? Faction::Unknown
                 : factionFromUInt8(static_cast<std::uint8_t>(factionOrdinal)));

        const qsizetype teamField = slotType == QLatin1Char('H')
                                        ? HUMAN_TEAM_SLOT_FIELD
                                        : COMP_TEAM_SLOT_FIELD;
        if (std::cmp_less_equal(fields.size(), teamField)) {
            throw CorruptDataException(
                QString("player %1 slot missing expected fields")
                    .arg(playerIdx),
                m_reader->lastOffset());
        }
        bool teamOk = false;
        const int rawTeam = fields.at(teamField).toInt(&teamOk);
        if (!teamOk) {
            throw CorruptDataException(
                QString("player %1 team value non-numeric").arg(playerIdx),
                m_reader->lastOffset());
        }
        player->teamNumber =
            rawTeam >= 0
                ? static_cast<std::uint32_t>(rawTeam) + 1
                : UNALLIED_TEAM_BASE + static_cast<std::uint32_t>(playerIdx);

        // At the end, prepare for the next path
        slotStart = slotEnd + 1;
    }
}

void SynopsisParser::parseOffsetAndMagic() {
    m_offset = m_reader->readIntegral<uint32_t>();
    // Read this manually since we want to mark mid-read so we track it
    const auto strReplLength = m_reader->readIntegral<uint32_t>();
    if (strReplLength != REPL_MAGIC_SIZE) {
        throw CorruptDataException(
            QLatin1String("CNC3RPL magic incorrect length"),
            m_reader->lastOffset());
    }

    // We mark now because the offset is from the start of the magic string to
    // the first data block
    m_reader->setMark();

    const QByteArray strReplMagic = m_reader->readBlock(REPL_MAGIC_SIZE);

    if (strReplMagic != QByteArray(REPL_MAGIC, REPL_MAGIC_SIZE)) {
        throw CorruptDataException(
            QLatin1String("CNC3RPL magic incorrect value"),
            m_reader->lastOffset());
    }
}

void SynopsisParser::parseHeaderTail() {
    // no mod_info in kw
    const auto ts = m_reader->readIntegral<uint32_t>();
    m_synopsis.timestamp = QDateTime::fromSecsSinceEpoch(
        static_cast<qint64>(ts), QTimeZone(QTimeZone::UTC));

    // read and discard the unknown1 block
    m_reader->readBlock(U1_SIZE);

    // read the header
    const QString header = m_reader->readFixedCharString<uint32_t>();
    parseMapReference(header);
    parsePlayerSlots(header);

    const auto replaySaver = m_reader->readByte<uint8_t>();
    if (replaySaver < m_synopsis.players.size()) {
        (m_synopsis.players.begin() + replaySaver)->isReplaySaver = true;
    }
    // TODO: else warn here, not worth throwing on corruption

    // read 2 zeroes
    m_reader->discardZero<uint32_t>();
    m_reader->discardZero<uint32_t>();

    m_synopsis.filename = m_reader->readFixedUtf16String<uint32_t>();

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

void SynopsisParser::parseBody() {
    // The payload isn't parsed at this time; just fingerprint it so callers
    // can cheaply compare/identify replay content. Read in bounded chunks
    // rather than the whole remaining file at once, so a corrupt or
    // maliciously oversized file can't force unbounded memory use.
    QCryptographicHash hash(QCryptographicHash::Sha256);
    size_t totalSize = 0;
    // The footer lives in the last handful of bytes of the file, but isn't
    // guaranteed to fall entirely within the very last chunk read - e.g. if
    // the file size puts the footer's start right at a chunk boundary.
    // readRemainingChunked hands back the last two chunks concatenated so
    // the footer's bytes are guaranteed complete somewhere within it.
    const QByteArray tail = m_reader->readRemainingChunked(
        [&](QByteArrayView chunk) {
            totalSize += static_cast<size_t>(chunk.size());
            if (totalSize > MAX_BODY_SIZE) {
                throw LimitExceededException(QLatin1String("replay payload"),
                                             m_reader->offset(), MAX_BODY_SIZE,
                                             totalSize);
            }
            hash.addData(chunk);
        },
        BODY_READ_CHUNK_SIZE);

    verifyFooter(tail);

    m_synopsis.checksum = hash.result();
}

void SynopsisParser::verifyFooter(QByteArrayView lastChunk) const {
    const std::optional<QByteArrayView> maybeFooter = footerFromTail(lastChunk);
    if (!maybeFooter) {
        throw TornDataException(m_reader->offset());
    }
    const QByteArrayView footer = *maybeFooter;

    // Once we have read the FOOTER_MAGIC the likelihood that further validation
    // errors are the result of a torn read are vanishingly unlikely so we
    // switch back to throwing CorruptDataException from here on

    // data sits between final_time_code and footer_length (the trailing 4
    // bytes, whose value is footer.size() itself); it's either {0x02}, or
    // {0x01, 0x02, uint32_t n, byte[n]}.
    const qsizetype dataStart = FOOTER_MAGIC_SIZE + FOOTER_LENGTH_FIELD_SIZE;
    const qsizetype dataSize =
        footer.size() - dataStart - FOOTER_LENGTH_FIELD_SIZE;
    const auto dataTag = static_cast<uchar>(footer.at(dataStart));

    if (dataTag == 0x02) {
        if (dataSize != 1) {
            throw CorruptDataException(
                QLatin1String(
                    "replay footer data has an unrecognized structure"),
                m_reader->offset());
        }
    } else if (dataTag == 0x01) {
        constexpr qsizetype extendedDataFixedSize =
            2 + FOOTER_LENGTH_FIELD_SIZE;
        // Bounds-check before reading the payload length field below with
        // the minimum-size data (dataSize == 1) that field would fall past
        // the end of footer.
        if (std::cmp_less(dataSize, extendedDataFixedSize) ||
            static_cast<uchar>(footer.at(dataStart + 1)) != 0x02) {
            throw CorruptDataException(
                QLatin1String(
                    "replay footer data has an unrecognized structure"),
                m_reader->offset());
        }
        const quint32 payloadLength = readLE32(footer.sliced(dataStart + 2));
        if (dataSize !=
            extendedDataFixedSize + static_cast<qsizetype>(payloadLength)) {
            throw CorruptDataException(
                QLatin1String(
                    "replay footer data has an unrecognized structure"),
                m_reader->offset());
        }
    } else {
        throw CorruptDataException(
            QLatin1String("replay footer data has an unrecognized structure"),
            m_reader->offset());
    }
}

bool SynopsisParser::looksComplete(QIODevice& replayFile) {
    // Every "can't tell" path here answers true. This check exists only to
    // rule a file out cheaply - anything it can't inspect falls through to a
    // full parse, which is the authority on the result either way.
    if (replayFile.isSequential()) {
        return true;
    }

    const qint64 size = replayFile.size();
    if (std::cmp_less(size, FOOTER_MIN_SIZE)) {
        // Too small to hold a footer at all - no point spending the seeks.
        return false;
    }

    const qint64 window = qMin(size, FOOTER_TAIL_WINDOW);
    const qint64 resume = replayFile.pos();
    if (!replayFile.seek(size - window)) {
        return true;
    }

    const QByteArray tail = replayFile.read(window);
    replayFile.seek(resume);

    // A short read means the view doesn't actually reach EOF - the file was
    // truncated out from under us, most likely - and footerFromTail's
    // offsets are all measured from the end, so the answer would be
    // meaningless. Defer to the full parse instead.
    if (tail.size() != window) {
        return true;
    }

    return footerFromTail(tail).has_value();
}

ReplaySynopsis SynopsisParser::parse(QIODevice& replayFile) {
    SynopsisParser parser{replayFile};
    parser.parse();
    return parser.metadata();
}
}  // namespace LegionParser