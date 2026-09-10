/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Muonic
 */

#include <legionparser/chunkanalyzer.h>

#include <QByteArray>
#include <QtEndian>
#include <QtTypes>
#include <array>
#include <cstddef>
#include <optional>
#include <span>
#include <stdexcept>
#include <tuple>
#include <type_traits>
#include <variant>

namespace LegionParser {
void ChunkAnalyzer::begin() {}

void ChunkAnalyzer::finalize() {}

namespace {

/**
 * Parse a static fixed length command
 */
class FixedLengthCommandScanner {
   public:
    constexpr explicit FixedLengthCommandScanner(qsizetype len) : m_len(len) {}

    [[nodiscard]] std::optional<std::tuple<Command, qsizetype>> scan(
        std::span<const std::byte> buf) const {
        if (buf.size() < m_len) {
            return std::nullopt;
        }
        // m_len is the whole command's length including the 2-byte header,
        // so the trailing payload span is m_len - 2 bytes, not m_len.
        return std::make_tuple(
            Command{
                .commandId = std::to_integer<quint8>(buf[0]),
                .mangledPlayerIdx = std::to_integer<quint8>(buf[1]),
                .bytes = buf.subspan(2, m_len - 2),
            },
            m_len);
    }

   private:
    qsizetype m_len;
};

/**
 * Parse a standard-encoding variable length command
 */
class StandardLayoutCommandScanner {
   public:
    constexpr explicit StandardLayoutCommandScanner(qsizetype len)
        : m_len(len) {}

    [[nodiscard]] std::optional<std::tuple<Command, qsizetype>> scan(
        std::span<const std::byte> buf) const {
        if (buf.size() < 2) {
            return std::nullopt;
        }
        // We are now going to consume length nibbles until we get to the end of
        // the command
        qsizetype pos = m_len;
        // pos >= buf.size() signifies a parsing desync
        while (pos < buf.size() && std::to_integer<quint8>(buf[pos]) != 0xFF) {
            const qsizetype adv = (std::to_integer<quint8>(buf[pos]) >> 4) + 1;
            pos += (4 * adv) + 1;
        }
        // We desynced scanning for 0xFF
        if (pos >= buf.size()) {
            return std::nullopt;
        }
        // Else we hit the terminator so the real end is pos + 1
        // TODO: Placeholder for refactoring
        return std::make_tuple(
            Command{
                .commandId = std::to_integer<quint8>(buf[0]),
                .mangledPlayerIdx = std::to_integer<quint8>(buf[1]),
                .bytes = buf.subspan(
                    2, pos - 2 + 1),  // pos is on the final byte not after it
            },
            pos + 1);
    }

   private:
    qsizetype m_len;
};

/**
 * Parse a length-prefixed array of fixed-size records: a count byte at
 * countOffset (biased by countBias before use), followed by that many
 * recordSize-byte records, plus baseOverhead bytes of fixed header/footer.
 * Covers 0x28 and 0x31, which share this shape with different constants.
 */
class LengthPrefixedArrayCommandScanner {
   public:
    constexpr LengthPrefixedArrayCommandScanner(qsizetype countOffset,
                                                qsizetype recordSize,
                                                qsizetype additionalBytes,
                                                qsizetype additionalCount)
        : m_countOffset(countOffset),
          m_recordSize(recordSize),
          m_additionalBytes(additionalBytes),
          m_additionalCount(additionalCount) {}

    [[nodiscard]] std::optional<std::tuple<Command, qsizetype>> scan(
        std::span<const std::byte> buf) const {
        if (buf.size() < 2) {
            return std::nullopt;
        }

        // We can't ccess the count field
        if (buf.size() <= m_countOffset) {
            return std::nullopt;
        }

        const qsizetype endPosition =
            ((std::to_integer<qsizetype>(buf[m_countOffset]) +
              m_additionalCount) *
             m_recordSize) +
            m_additionalBytes;

        if (buf.size() < endPosition) {
            return std::nullopt;
        }

        return std::make_tuple(
            Command{
                .commandId = std::to_integer<quint8>(buf[0]),
                .mangledPlayerIdx = std::to_integer<quint8>(buf[1]),
                .bytes = buf.subspan(2, endPosition - 2),
            },
            endPosition);  // End position is off the end
    }

   private:
    qsizetype m_countOffset;
    qsizetype m_recordSize;
    qsizetype m_additionalBytes;
    qsizetype m_additionalCount;
};

/**
 * Parse the bespoke 0x2D command: it's fixed length, either 8 or 26 bytes,
 * and which one it is is signalled by whether byte 7 is already the
 * terminator.
 */
class Bespoke0x2DCommandScanner {
   public:
    [[nodiscard]] std::optional<std::tuple<Command, qsizetype>> scan(
        std::span<const std::byte> buf) const {
        if (buf.size() < 8) {
            return std::nullopt;
        }

        const qsizetype len = std::to_integer<quint8>(buf[7]) == 0xFF ? 8 : 26;

        if (buf.size() < len) {
            return std::nullopt;
        }

        return std::make_tuple(
            Command{
                .commandId = std::to_integer<quint8>(buf[0]),
                .mangledPlayerIdx = std::to_integer<quint8>(buf[1]),
                .bytes = buf.subspan(2, len - 2),
            },
            len);
    }
};

/**
 * Parse the bespoke 0x8B command (KW's use of the shared "uuid" shape):
 * a length-prefixed, one-byte-per-char string (length byte at offset 3,
 * data starting at offset 4), immediately followed by a second
 * length-prefixed string, this one two-byte-per-char (length byte first,
 * data right after), then a trailing 4-byte number and the terminator.
 */
class Bespoke0x8BCommandScanner {
   public:
    [[nodiscard]] std::optional<std::tuple<Command, qsizetype>> scan(
        std::span<const std::byte> buf) const {
        // Need to be able to read the first string's length byte at offset 3
        if (buf.size() < 4) {
            return std::nullopt;
        }

        const qsizetype firstStringLen = std::to_integer<qsizetype>(buf[3]);
        // Fixed 5-byte overhead (2-byte command header + 1 unknown byte +
        // 1 length byte + this offset itself) ahead of the second string
        const qsizetype afterFirstString = firstStringLen + 5;

        // Need to be able to read the second string's length byte
        if (buf.size() <= afterFirstString) {
            return std::nullopt;
        }

        const qsizetype secondStringLen =
            std::to_integer<qsizetype>(buf[afterFirstString]);
        // +2 (not +1) for the second string's own overhead, then +5 for the
        // trailing 4-byte number and the terminator - mirrors the reference
        // parser's arithmetic exactly, including its own unexplained extra
        // byte after the second string.
        const qsizetype len = afterFirstString + (2 * secondStringLen) + 2 + 5;

        if (buf.size() < len) {
            return std::nullopt;
        }

        return std::make_tuple(
            Command{
                .commandId = std::to_integer<quint8>(buf[0]),
                .mangledPlayerIdx = std::to_integer<quint8>(buf[1]),
                .bytes = buf.subspan(2, len - 2),
            },
            len);
    }
};

// A closed set of command shapes, dispatched via variant/visit instead of
// a virtual-dispatch interface
// std::monostate stands in for "no scanner registered for this command
// id"
using CommandScanner =
    std::variant<std::monostate, FixedLengthCommandScanner,
                 StandardLayoutCommandScanner,
                 LengthPrefixedArrayCommandScanner, Bespoke0x2DCommandScanner,
                 Bespoke0x8BCommandScanner>;

constexpr std::array<CommandScanner, 256> COMMAND_TABLE = [] {
    std::array<CommandScanner, 256> table{};
    auto add = [&table](quint8 id, CommandScanner scanner) {
        if (!std::holds_alternative<std::monostate>(table.at(id))) {
            throw std::logic_error("duplicate command id in kCommandTable");
        }
        table.at(id) = scanner;
    };

    // Standard-layout, offset 2 - by far the most common shape.
    for (const quint8 id :
         {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0B,
          0x0C, 0x0D, 0x0F, 0x10, 0x11, 0x12, 0x17, 0x72, 0x73, 0x4C,
          0x4D, 0x92, 0x93, 0xF9, 0xFA, 0xFB, 0xFC, 0xFD, 0xFE, 0xFF}) {
        add(id, StandardLayoutCommandScanner{2});
    }

    // Standard-layout, other offsets.
    add(0x27, StandardLayoutCommandScanner{34});
    add(0x2B, StandardLayoutCommandScanner{11});
    add(0x26, StandardLayoutCommandScanner{15});
    for (const quint8 id : {0xF5, 0xF6, 0xF8}) {
        add(id, StandardLayoutCommandScanner{4});
    }

    // Fixed-length.
    add(0x29, FixedLengthCommandScanner{28});
    for (const quint8 id : {0x2C, 0x2F, 0x30}) {
        add(id, FixedLengthCommandScanner{17});
    }
    add(0x2E, FixedLengthCommandScanner{22});
    for (const quint8 id : {0x34, 0x44, 0x87, 0x89}) {
        add(id, FixedLengthCommandScanner{8});
    }
    for (const quint8 id : {0x35, 0x43, 0x7E, 0x7F}) {
        add(id, FixedLengthCommandScanner{12});
    }
    add(0x36, FixedLengthCommandScanner{13});
    for (const quint8 id : {0x3C, 0x3D, 0x45}) {
        add(id, FixedLengthCommandScanner{21});
    }
    for (const quint8 id : {0x3E, 0x46, 0x47, 0x48, 0x5B, 0x8E, 0x90}) {
        add(id, FixedLengthCommandScanner{16});
    }
    add(0x61, FixedLengthCommandScanner{20});
    add(0x77, FixedLengthCommandScanner{3});
    add(0x7A, FixedLengthCommandScanner{29});
    add(0x8C, FixedLengthCommandScanner{45});
    add(0x8D, FixedLengthCommandScanner{1049});
    add(0x8F, FixedLengthCommandScanner{40});
    add(0x91, FixedLengthCommandScanner{10});

    // Special/bespoke.
    add(0x28, LengthPrefixedArrayCommandScanner{17, 4, 32, 1});
    add(0x31, LengthPrefixedArrayCommandScanner{12, 18, 17, 0});
    add(0x2D, Bespoke0x2DCommandScanner{});
    add(0x8B, Bespoke0x8BCommandScanner{});

    return table;
}();

template <typename T>
constexpr bool IS_BESPOKE_SCANNER =
    std::is_same_v<T, LengthPrefixedArrayCommandScanner> ||
    std::is_same_v<T, Bespoke0x2DCommandScanner> ||
    std::is_same_v<T, Bespoke0x8BCommandScanner>;

// If successful returns a command and how many bytes were consumed
[[nodiscard]] std::variant<std::tuple<Command, qsizetype>, DesyncTrigger>
scanCommand(const CommandScanner& scanner, std::span<const std::byte> buf) {
    return std::visit(
        [buf](const auto& s)
            -> std::variant<std::tuple<Command, qsizetype>, DesyncTrigger> {
            using T = std::decay_t<decltype(s)>;

            if constexpr (std::is_same_v<T, std::monostate>) {
                return DesyncTrigger::UnrecognizedCommand;
            } else {
                if (auto result = s.scan(buf)) {
                    return *result;
                }
                if constexpr (std::is_same_v<T, FixedLengthCommandScanner>) {
                    return DesyncTrigger::IncompleteFixedLengthCommand;
                } else if constexpr (std::is_same_v<
                                         T, StandardLayoutCommandScanner>) {
                    return DesyncTrigger::IncompleteStandardCommand;
                } else if constexpr (IS_BESPOKE_SCANNER<T>) {
                    return DesyncTrigger::IncompleteBespokeCommand;
                }
            }
            return DesyncTrigger::UnrecognizedCommand;
        },
        scanner);
}

template <typename... Ts>
struct Match : Ts... {
    using Ts::operator()...;
};

}  // namespace

bool CommandFrameAnalyzer::chunk(qsizetype chunkOffset, quint32 timecode,
                                 ChunkType type,
                                 std::span<const std::byte> buf) {
    if (type != ChunkType::Command) {
        return true;
    }

    const auto expected = std::to_integer<quint8>(buf[0]);
    Q_ASSERT(expected == 1);  // Always 1
    // We've desynced so stop
    if (expected != 1) {
        // TODO: Modify this to call the error handler
        return false;
    }

    const auto count = qFromLittleEndian<quint32>(buf.subspan(1, 4).data());
    if (count > 10000) {
        // TODO: Signal this to higher, we've exceeded an internal limit
        return false;
    }

    // TODO: empirically guess typical commands per chunk for reserve
    QList<Command> commands;
    commands.reserve(count);

    // We start at position 5 after the subheader
    qsizetype bufOffset = 5;
    while (bufOffset < buf.size()) {
        const quint8 cmdId = std::to_integer<quint8>(buf[bufOffset]);
        const auto& scanner = COMMAND_TABLE[cmdId];
        const bool didDesync = std::visit(
            Match{
                [this, &bufOffset, &buf, &commands, chunkOffset,
                 cmdId](const std::tuple<Command, qsizetype>& result) -> bool {
                    const auto [cmd, consumed] = result;
                    // the command is malformed because it consumed nothing
                    // (should always consume at least command and player),
                    // or it claims it consumed more than was available in the
                    // buffer crash in debug, decay to desync in release
                    Q_ASSERT(consumed > 0 &&
                             bufOffset + consumed <= buf.size());
                    if (consumed == 0 || buf.size() < bufOffset + consumed) {
                        m_error(DesyncDetails{
                            .cause = DesyncTrigger::InvalidScanResult,
                            .chunkOffset = chunkOffset,
                            .relativeCommandOffset = bufOffset,
                            .command = cmdId});
                        return true;
                    }
                    bufOffset += consumed;
                    commands.append(cmd);
                    return false;
                },
                // The command signalled a desync so forward to the error
                // handler and trigger a break
                [this, chunkOffset, bufOffset,
                 cmdId](DesyncTrigger trigger) -> bool {
                    m_error(DesyncDetails{.cause = trigger,
                                          .chunkOffset = chunkOffset,
                                          .relativeCommandOffset = bufOffset,
                                          .command = cmdId});
                    return true;
                },
            },
            scanCommand(scanner, buf.subspan(bufOffset)));
        if (didDesync) {
            break;
        }
    }

    return m_chunk(timecode, QSpan(commands));
}

}  // namespace LegionParser
