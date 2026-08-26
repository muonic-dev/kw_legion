// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Muonic

#pragma once

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

constexpr std::uint8_t toUInt8(GameType type) {
    return static_cast<std::uint8_t>(type);
}

constexpr GameType gameTypeFromUInt8(std::uint8_t raw) {
    switch (raw) {
        case toUInt8(GameType::Skirmish):
            return GameType::Skirmish;
        case toUInt8(GameType::Multiplayer):
            return GameType::Multiplayer;
        default:
            return GameType::Unknown;
    }
}

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
    Unknown  // also random
};

constexpr std::uint8_t toUInt8(Faction faction) {
    return static_cast<std::uint8_t>(faction);
}

constexpr Faction factionFromUInt8(std::uint8_t raw) {
    if (raw < toUInt8(Faction::GDI) || toUInt8(Faction::Traveler) < raw) {
        return Faction::Unknown;
    }
    return static_cast<Faction>(raw);
}

struct Player {
    // In a parsed online replay the player id seems to be consistent
    // likely a unique id assigned by the server for login.
    // In skirmish, the player id appears to always be zero.
    // The behavior in network is always zero
    std::uint32_t id = std::numeric_limits<std::uint32_t>::max();
    QString name = QLatin1String("");
    // Sourced entirely from the S= slot text (see
    // Parser::parsePlayerSlots), for both skirmish and multiplayer
    // replays, using 1-based numbering matching the lobby UI. Multiplayer
    // replays also carry a binary team_number byte, but real network
    // matches confirmed so far are all 1v1, where that byte's behavior
    // can't be distinguished from the slot text's; the slot text is used
    // uniformly rather than trusting the byte for the untested case of a
    // real online team match. A player with no team explicitly assigned
    // gets a unique placeholder value instead of a shared one, so it never
    // compares equal to another unallied player's teamNumber. The default
    // (also larger than any real or placeholder value) means parsing
    // hasn't reached this player yet.
    std::uint32_t teamNumber = std::numeric_limits<std::uint32_t>::max();
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