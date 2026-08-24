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

namespace {

// Resolves path to its canonical form so that it always matches the
// canonical paths QFileInfo hands back for files discovered underneath it.
// If path doesn't (yet) exist as a directory it's returned unchanged, since
// the prospector is allowed to be configured with a directory ahead of it
// being created.
QString canonicalizeDirectoryPath(const QString& path) {
    const QFileInfo info(path);
    return info.isDir() ? info.canonicalFilePath() : path;
}

}  // namespace

ReplayProspector::ReplayProspector(const QString& replayDir, QObject* parent)
    : QObject(parent),
      m_watcher(this),
      m_replayDirectory(canonicalizeDirectoryPath(replayDir)) {
    QObject::connect(&m_watcher, &QFileSystemWatcher::directoryChanged, this,
                     &ReplayProspector::watchedDirectoryChanged);
}

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
    m_replayDirectory = canonicalizeDirectoryPath(path);
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

    // The directory may not have existed yet when it was set, in which case
    // m_replayDirectory is still whatever raw path was given. Now that we
    // know it exists, canonicalize it so it matches the canonical paths
    // QFileInfo hands back for everything discovered underneath it.
    m_replayDirectory = replayDirInfo.canonicalFilePath();

    QDirIterator dirIter(m_replayDirectory,
                         QDir::AllEntries | QDir::NoDotAndDotDot,
                         QDirIterator::Subdirectories);

    if (!m_watcher.addPath(m_replayDirectory)) {
        qCWarning(logProspector)
            << "Failed to watch replay directory: " << m_replayDirectory;
    }

    while (dirIter.hasNext()) {
        const QFileInfo nextInfo = dirIter.nextFileInfo();
        // For the initial sweep we specifically need to continue recursing
        // if its a directory
        if (nextInfo.isDir()) {
            if (!m_watcher.addPath(nextInfo.canonicalFilePath())) {
                qCWarning(logProspector)
                    << "Failed to watch directory: "
                    << nextInfo.canonicalFilePath();
            }
        } else if (nextInfo.isFile() && nextInfo.fileName().endsWith(
                                            ".KWReplay", Qt::CaseInsensitive)) {
            // This call to canonicalFilePath is load bearing to ensure we
            // detect deletes correctly below. It caches so we can see the file
            // again
            m_knownFiles.insert(nextInfo.canonicalFilePath(), nextInfo);
        }
    }

    emit initialSweepCompleted(m_knownFiles.keys());
}

void ReplayProspector::watchedDirectoryChanged(const QString& path) {
    // TODO: temporary diagnostic logging, remove once the modify-detection
    // issue on some machines is root-caused.
    qCDebug(logProspector) << "watchedDirectoryChanged fired for: " << path;

    QDirIterator dirIter(path, QDir::AllEntries | QDir::NoDotAndDotDot);

    // We want to track the paths we have seen so that we can identify files
    // that have disappeared
    QSet<QString> seenFilePaths;

    // First, test all files currently in the directory against their historical
    // timestamps
    while (dirIter.hasNext()) {
        const QFileInfo nextInfo = dirIter.nextFileInfo();

        if (nextInfo.isDir()) {
            const QString canonicalDirPath = nextInfo.canonicalFilePath();
            // A directory that showed up here wasn't under watch yet, so
            // nothing inside it (or anything nested further below it) would
            // otherwise be noticed until something outside prompted another
            // look at this level.
            if (!m_watcher.directories().contains(canonicalDirPath)) {
                qCDebug(logProspector)
                    << "Watching newly discovered directory: "
                    << canonicalDirPath;
                watchDirectoryTree(canonicalDirPath);
            }
            continue;
        }

        const QString canonicalFilePath = nextInfo.canonicalFilePath();
        if (nextInfo.isFile() &&
            nextInfo.fileName().endsWith(".KWReplay", Qt::CaseInsensitive)) {
            seenFilePaths.insert(canonicalFilePath);

            const auto it = m_knownFiles.find(canonicalFilePath);
            // TODO: temporary diagnostic logging, remove once the
            // modify-detection issue on some machines is root-caused.
            qCDebug(logProspector)
                << "Checked" << canonicalFilePath << "knownModified="
                << (it != m_knownFiles.cend() ? it->lastModified()
                                              : QDateTime())
                << "diskModified=" << nextInfo.lastModified();
            // The file has never been seen before or the updated at time has
            // changed
            if (it == m_knownFiles.cend() ||
                it->lastModified() != nextInfo.lastModified()) {
                qCDebug(logProspector)
                    << "Replay file new/changed: " << canonicalFilePath;
                emit replayFileChanged(canonicalFilePath);
                m_knownFiles.insert(canonicalFilePath, nextInfo);
            }
        }
    }

    // Now we need to detect cases where a file may have vanished underneath us
    for (auto it = m_knownFiles.cbegin(); it != m_knownFiles.cend();) {
        // If the file were actually removed this would return "", however, we
        // always force canonicalFilePath at insertion time so it should cache.
        const QString canonicalFilePath = it->canonicalFilePath();
        if (it->canonicalPath() == path &&
            !seenFilePaths.contains(canonicalFilePath)) {
            qCDebug(logProspector)
                << "Replay file disappeared: " << canonicalFilePath;
            emit replayFileRemoved(canonicalFilePath);
            it = m_knownFiles.erase(it);

        } else {
            it++;
        }
    }
}

void ReplayProspector::watchDirectoryTree(const QString& path) {
    if (!m_watcher.addPath(path)) {
        qCWarning(logProspector) << "Failed to watch directory: " << path;
    }

    QDirIterator dirIter(path, QDir::AllEntries | QDir::NoDotAndDotDot,
                         QDirIterator::Subdirectories);
    while (dirIter.hasNext()) {
        const QFileInfo nextInfo = dirIter.nextFileInfo();
        if (nextInfo.isDir()) {
            if (!m_watcher.addPath(nextInfo.canonicalFilePath())) {
                qCWarning(logProspector)
                    << "Failed to watch directory: "
                    << nextInfo.canonicalFilePath();
            }
        } else if (nextInfo.isFile() && nextInfo.fileName().endsWith(
                                            ".KWReplay", Qt::CaseInsensitive)) {
            const QString canonicalFilePath = nextInfo.canonicalFilePath();
            qCDebug(logProspector)
                << "Replay file new/changed: " << canonicalFilePath;
            emit replayFileChanged(canonicalFilePath);
            m_knownFiles.insert(canonicalFilePath, nextInfo);
        }
    }
}

}  // namespace KWLegionCore