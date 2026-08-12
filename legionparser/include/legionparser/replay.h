#pragma once

#include <legionparser/legionparser_export.h>

#include <QByteArray>
#include <QDateTime>
#include <QList>
#include <QString>
#include <cstdint>

namespace LegionParser {

enum class GameType : std::uint8_t {
    Unknown = 0x03,
    Skirmish = 0x04,
    Multiplayer = 0x05,
};

// Values derived empirically from the ";S=" player-slot list in the header
// string, cross-referenced against skirmish replays with a known faction per
// side (see test/replays/skirmish_h<faction>_v_c<faction>.KWReplay).
enum class Faction : std::uint8_t {
    GDI = 6,
    ST,
    ZOCOM,
    Nod,
    BH,
    MoK,
    Scrin,
    Reaper,
    Traveler,
    Unknown
};

struct Player {
    std::uint32_t playerId = std::numeric_limits<std::uint32_t>::max();
    QString playerName = "";
    // Only relevant for multiplayer
    // The default will also be larger than the input
    std::int32_t teamNumber = std::numeric_limits<std::int32_t>::max();
    Faction faction = Faction::Unknown;
    bool isComputer = false;
    bool isReplaySaver = false;
};

struct ReplayMetadata {
    std::uint32_t versionMajor;
    std::uint32_t versionMinor;
    std::uint32_t buildMajor;
    std::uint32_t buildMinor;

    GameType gameType = GameType::Multiplayer;  // Make init happy
    bool hasCommentary;

    QString matchTitle;
    QString matchDescription;
    QString mapName;
    QString mapId;

    QList<Player> players;

    QDateTime timestamp;

    /**
     * The filename embedded within the replay. This will be unreliable and
     * doesn't seem to include the extension
     */
    QString filename;
    /**
     * A normalized (lowercased, forward-slash-separated) virtual path into
     * whichever .big archive contains the map, e.g.
     * "data/maps/official/abandoned subway 1.02+__25beta". Confirmed against
     * the game's own archive listings: matches directly for maps shipped in
     * the base game, and for custom/community maps it points into whatever
     * separately-installed .big file provides them.
     *
     * This may be useful to do things like determine whether or not the file
     * is from the current community patch version since there are files that
     * control which .big files are loaded and those contain this literal
     * string.
     */
    QString mapReference;

    /**
     * A SHA-256 checksum (raw 32 bytes; use toHex() for display) of the
     * replay's raw payload body (everything after the header - the
     * game-action chunks, end-of-chunks terminator, and footer). The body
     * itself is not parsed; this exists as a cheap way to
     * fingerprint/compare replay content.
     */
    QByteArray checksum;
};
}  // namespace LegionParser