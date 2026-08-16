// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Muonic

#pragma once

#include <QFileInfo>
#include <QFileSystemWatcher>
#include <QLoggingCategory>
#include <QMap>
#include <QObject>
#include <QString>

Q_DECLARE_LOGGING_CATEGORY(logProspector)

namespace KWLegionCore {

/* The replay prospector identifies and tracks replays that are in the Kane's
 * Wrath replays folder. This includes identifying replays that are added or
 * changed. Specifically, it is expected that replays such as Last
 * Replay.KWReplay will change over time KW writes the latest replays.
 *
 * The expected sequence of events is as follows.
 * Something (such as QThread::started) should trigger initialSweep
 * The initialSweep will sweep the input directory for the full list of known
 * files and setup watches.
 *
 * It will then emit initialSweepComplete(QList<QString>)
 *
 * constaining all the paths that were discovered in this sweep.
 *
 * Subsequently, anytime something in the watch directory changes it will emit
 * either replayDiscovered or replayRemoved with the path that changed.
 *
 * It does this by maintaining a current model of the filesystem that is built
 * during the initial sweep
 */
class ReplayProspector : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString replayDirectory READ replayDirectory WRITE
                   setReplayDirectory NOTIFY replayDirectoryChanged)

   public:
    explicit ReplayProspector(QString replayDir = defaultReplayDirectory(),
                              QObject* parent = nullptr);

    // Best-effort guess at this machine's Kane's Wrath replay folder, based
    // on the current Documents location. This is only a suggestion for
    // prefilling the UI - the prospector itself stays inert until
    // setReplayDirectory() is called explicitly.
    static QString defaultReplayDirectory();

    [[nodiscard]] QString replayDirectory() const { return m_replayDirectory; }

    /**
     * Set the replay directory
     *
     * After setting this you should call initialSweep.
     */
    void setReplayDirectory(const QString& path);

    /**
     * Set the default replay directory
     */
    void setReplayDirectory();

    /**
     * Perform an initial sweep after setting the replay directory.
     *
     * This is only public so that it can be triggered as the thread starts up.
     */
    void initialSweep();

   signals:

    /**
     * Emitted when the replay directory is changed
     */
    void replayDirectoryChanged(const QString& path);

    /**
     * Emitted when the initial sweep completes.
     *
     * This will contain all subpaths under the replay directory that may be KW
     * Replays
     */
    void initialSweepCompleted(QList<QString> paths);

    /**
     * Emitted when a replay path is discovered under the replay directory
     */
    void replayFileChanged(const QString& filePath);

    /**
     * Emitted when a replay path that was known no longer exists
     */
    void replayFileRemoved(const QString& filePath);

   private:
    void processItem(const QFileInfo&);

    // If the item is new or updated handle the insert and emission
    void handleUpdatedItem(const QString&, const QFileInfo&);

    QFileSystemWatcher m_watcher;
    QString m_replayDirectory;

    // Store the known paths of
    QMap<QString, QFileInfo> m_knownFiles;
};

}  // namespace KWLegionCore
