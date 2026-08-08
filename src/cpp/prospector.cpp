#include "prospector.h"

#include <QDir>
#include <QStandardPaths>

namespace KWLegion {

ReplayProspector::ReplayProspector(QObject* parent) : QObject(parent), m_watcher(this) {
    connect(&m_watcher, &QFileSystemWatcher::directoryChanged, this,
            &ReplayProspector::rescan);
}

QString ReplayProspector::defaultReplayDirectory() {
    const QString documents =
        QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
    return documents + QStringLiteral("/Command & Conquer 3 Kane's Wrath/Replays");
}

void ReplayProspector::setReplayDirectory(const QString& path) {
    if (path == m_replayDirectory) {
        return;
    }

    if (!m_replayDirectory.isEmpty()) {
        m_watcher.removePath(m_replayDirectory);
    }

    m_knownFiles.clear();
    m_replayDirectory = path;
    emit replayDirectoryChanged(m_replayDirectory);

    if (m_replayDirectory.isEmpty()) {
        return;
    }

    m_watcher.addPath(m_replayDirectory);
    rescan();
}

void ReplayProspector::rescan() {
    const QDir dir(m_replayDirectory);
    if (!dir.exists()) {
        return;
    }

    const QStringList files = dir.entryList({QStringLiteral("*.KWReplay")}, QDir::Files);

    QSet<QString> currentFiles(files.begin(), files.end());

    for (const QString& file : currentFiles) {
        if (!m_knownFiles.contains(file)) {
            emit replayDiscovered(dir.filePath(file));
        }
    }
    for (const QString& file : m_knownFiles) {
        if (!currentFiles.contains(file)) {
            emit replayRemoved(dir.filePath(file));
        }
    }

    m_knownFiles = std::move(currentFiles);
}

}  // namespace KWLegion
