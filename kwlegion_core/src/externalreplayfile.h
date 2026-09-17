/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Muonic
 */

#pragma once

#include <QByteArray>
#include <QDateTime>
#include <QString>
#include <QtTypes>


namespace KWLegionCore {
/**
 * A replay file that exists externally (including its checksum)
 */
struct ExternalReplayFile {
    QByteArray replayChecksum;
    QString externalPath;
    QDateTime mtime;
    qsizetype fileSize;
};
}  // namespace KWLegionCore