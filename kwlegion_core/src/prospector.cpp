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
    QObject::connect(&m_watcher, &QFileSystemWatcher::fileChanged, this,
                     &ReplayProspector::watchedFileChanged);
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

    // removePaths warns on an empty list, and both sets are empty before the
    // first sweep.
    if (!m_watcher.files().isEmpty()) {
        m_watcher.removePaths(m_watcher.files());
    }
    if (!m_watcher.directories().isEmpty()) {
        m_watcher.removePaths(m_watcher.directories());
    }
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
                qCWarning(logProspector) << "Failed to watch directory: "
                                         << nextInfo.canonicalFilePath();
            }
        } else if (nextInfo.isFile() && nextInfo.fileName().endsWith(
                                            ".KWReplay", Qt::CaseInsensitive)) {
            // This call to canonicalFilePath is load bearing to ensure we
            // detect deletes correctly below. It caches so we can see the file
            // again
            trackFile(nextInfo.canonicalFilePath(), nextInfo);
        }
    }

    emit initialSweepCompleted(m_knownFiles.keys());
}

void ReplayProspector::watchedDirectoryChanged(const QString& path) {
    // Only runs for namespace changes (create/delete/rename). An in-place
    // write to an existing file does generate kernel notifications, but Qt
    // stat-verifies the watched path before emitting, and a directory's own
    // mtime doesn't move when a child's contents change - so those are
    // discarded and the modify detection below is never reached for them.
    // Catching an in-place overwrite needs a per-file watch, whose verify is
    // against the file, whose size and mtime do move.
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
            // The file has never been seen before or the updated at time has
            // changed
            if (it == m_knownFiles.cend() ||
                it->lastModified() != nextInfo.lastModified()) {
                qCDebug(logProspector)
                    << "Replay file new/changed: " << canonicalFilePath;
                trackFile(canonicalFilePath, nextInfo);
                emit replayFileChanged(canonicalFilePath);
            }
        }
    }

    // Now we need to detect cases where a file may have vanished underneath us.
    // Collected first rather than erased in place because untrackFile also
    // touches the watcher, and emitting while iterating would let a handler
    // reach back into the map we are walking.
    QList<QString> vanished;
    for (auto it = m_knownFiles.cbegin(); it != m_knownFiles.cend(); ++it) {
        // If the file were actually removed this would return "", however, we
        // always force canonicalFilePath at insertion time so it should cache.
        const QString canonicalFilePath = it->canonicalFilePath();
        if (it->canonicalPath() == path &&
            !seenFilePaths.contains(canonicalFilePath)) {
            vanished.append(canonicalFilePath);
        }
    }

    for (const QString& canonicalFilePath : vanished) {
        qCDebug(logProspector)
            << "Replay file disappeared: " << canonicalFilePath;
        untrackFile(canonicalFilePath);
        emit replayFileRemoved(canonicalFilePath);
    }
}

void ReplayProspector::watchedFileChanged(const QString& path) {
    // Paths are registered canonically, and Qt hands back the string it was
    // given, so this matches m_knownFiles' keys directly.
    const auto it = m_knownFiles.find(path);
    if (it == m_knownFiles.end()) {
        // Either never ours, or the directory rescan already reconciled this
        // one. Whichever of the two notifications arrives second finds the
        // map already consistent and has nothing to report - which is what
        // keeps a delete seen down both routes from being emitted twice.
        return;
    }

    // Constructed fresh deliberately: the QFileInfo parked in m_knownFiles is
    // a cached snapshot of the state we last reported, so it can never
    // observe the change we are being notified about.
    const QFileInfo currentInfo(path);

    if (!currentInfo.exists()) {
        // Deleted or renamed away. Qt has already dropped its own watch by
        // this point; untrackFile keeps m_knownFiles in step with that.
        qCDebug(logProspector) << "Replay file disappeared: " << path;
        untrackFile(path);
        emit replayFileRemoved(path);
        return;
    }

    if (it->lastModified() == currentInfo.lastModified() &&
        it->size() == currentInfo.size()) {
        // A notification we have already accounted for - the directory
        // rescan and this watch both cover a newly created file, for one.
        return;
    }

    qCDebug(logProspector) << "Replay file new/changed: " << path;
    trackFile(path, currentInfo);
    emit replayFileChanged(path);
}

void ReplayProspector::trackFile(const QString& canonicalPath,
                                 const QFileInfo& info) {
    m_knownFiles.insert(canonicalPath, info);
    // Unconditional and idempotent. addPath reports false both for a real
    // failure and for a path already watched, so the result isn't worth
    // branching on - and re-adding is the point, since Qt drops a watch when
    // its file disappears and a recreated path would otherwise silently stop
    // being watched.
    m_watcher.addPath(canonicalPath);
}

void ReplayProspector::untrackFile(const QString& canonicalPath) {
    m_knownFiles.remove(canonicalPath);
    // Usually already gone - Qt drops the watch itself when the file does -
    // in which case this reports false and there is nothing to do about it.
    m_watcher.removePath(canonicalPath);
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
                qCWarning(logProspector) << "Failed to watch directory: "
                                         << nextInfo.canonicalFilePath();
            }
        } else if (nextInfo.isFile() && nextInfo.fileName().endsWith(
                                            ".KWReplay", Qt::CaseInsensitive)) {
            const QString canonicalFilePath = nextInfo.canonicalFilePath();
            qCDebug(logProspector)
                << "Replay file new/changed: " << canonicalFilePath;
            trackFile(canonicalFilePath, nextInfo);
            emit replayFileChanged(canonicalFilePath);
        }
    }
}

}  // namespace KWLegionCore