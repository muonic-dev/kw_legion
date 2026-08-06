#pragma once

#include <legionparser/legionparser_export.h>

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

    QDateTime replayTimestamp;

    /**
     * The filename embedded within the replay. This will be unreliable and
     * doesn't seem to include the extension
     */
    QString filename;
};
}  // namespace LegionParser