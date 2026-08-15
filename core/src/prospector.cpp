// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Muonic

#include <kwlegion_core/prospector.h>

#include <QDebug>
#include <QDir>
#include <QDirIterator>
#include <QStack>
#include <QStandardPaths>
#include <QThread>

Q_LOGGING_CATEGORY(logProspector, "kwlegion.prospector")

namespace KWLegionCore {

// TODO: Some way to signal that an I/O error happened

ReplayProspector::ReplayProspector(QObject* parent)
    : QObject(parent),
      m_watcher(this),
      m_replayDirectory(defaultReplayDirectory()) {}

QString ReplayProspector::defaultReplayDirectory() {
    const QString documents =
        QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
    return documents +
           QStringLiteral("/Command & Conquer 3 Kane's Wrath/Replays");
}

void ReplayProspector::setReplayDirectory(const QString& path) {
    qCInfo(logProspector) << "Setting replay directory to: " << path;

    // clear everything

    m_watcher.removePaths(m_watcher.directories());
    m_knownFiles.clear();
    m_replayDirectory = path;
}

void ReplayProspector::setReplayDirectory() {
    setReplayDirectory(defaultReplayDirectory());
}

void ReplayProspector::initialSweep() {
    qCDebug(logProspector) << "initial sweep";
    // Let downstream things trigger to do their own initialization

    if (m_replayDirectory.isNull() || m_replayDirectory.isEmpty()) {
        qCDebug(logProspector) << "No valid replay path, skipping sweep";
        return;
    }

    const QFileInfo replayDirInfo(m_replayDirectory);
    if (!replayDirInfo.isDir()) {
        qCWarning(logProspector) << "Replay path is not a directory";
        return;
    }

    QDirIterator dirIter(m_replayDirectory,
                         QDir::AllEntries | QDir::NoDotAndDotDot,
                         QDirIterator::Subdirectories);

    while (dirIter.hasNext()) {
        const QFileInfo nextInfo = dirIter.nextFileInfo();
        // For the initial sweep we specifically need to continue recursing
        // if its a directory
        processItem(nextInfo);
    }
}

void ReplayProspector::processItem(const QFileInfo& info) {
    const QString canonicalPath = info.canonicalFilePath();
    // If this is a directory we should add it to the watcher if we haven't
    // already done so
    if (info.isDir()) {
        qCDebug(logProspector)
            << "Adding directory for watch: " << canonicalPath;
        m_watcher.addPath(canonicalPath);
    } else if (info.isFile() &&
               info.fileName().endsWith(".KWReplay", Qt::CaseInsensitive)) {
        const auto it = m_knownFiles.find(canonicalPath);
        if (it == m_knownFiles.cend()) {
            handleUpdatedItem(canonicalPath, info);
        } else {
            // The file has been seen before so we need to see if it has
            // changed since we last tracked it
            // This is important for things like Last Replay.KWReplay
            // which will change periodically
            if (it->lastModified() != info.lastModified()) {
                qCDebug(logProspector)
                    << "Found modified file: " << canonicalPath;
                handleUpdatedItem(canonicalPath, info);
            } else {
                // The file hasn't changed since last time so do nothing
                qCDebug(logProspector)
                    << "Found existing file: " << canonicalPath;
            }
        }
    } else {
        qCDebug(logProspector) << "Discarding file: " << canonicalPath;
    }
}

void ReplayProspector::handleUpdatedItem(const QString& canonicalPath,
                                         const QFileInfo& info) {
    // The file has never been seen before so track and emit
    qCDebug(logProspector) << "Replay file new/changed: " << canonicalPath;
    m_knownFiles.insert(canonicalPath, info);
    emit replayFileChanged(canonicalPath);
}

}  // namespace KWLegionCore