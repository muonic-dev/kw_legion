/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Muonic
 */

#pragma once

#include <QDateTime>
#include <cstdint>

namespace KWLegionCore {
enum class ProblemType : uint8_t { TORN = 0, CORRUPT = 1 };

inline uint8_t problemToUInt8(ProblemType type) {
    return static_cast<uint8_t>(type);
}

inline ProblemType problemFromUInt8(uint8_t t) {
    if (t > static_cast<uint8_t>(ProblemType::CORRUPT)) {
        return ProblemType::CORRUPT;
    }
    return static_cast<ProblemType>(t);
}

/** Record of a replay problem type */
struct Problem {
    QString path;
    QDateTime noticedAt;
    ProblemType type;
};
}  // namespace KWLegionCore