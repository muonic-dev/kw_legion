#pragma once

#include <QFileInfo>
#include <QFileSystemWatcher>
#include <QLoggingCategory>
#include <QMap>
#include <QObject>
#include <QString>

Q_DECLARE_LOGGING_CATEGORY(logProspector)

namespace KWLegion {

/* The replay prospector identifies and tracks replays that are in the Kane's
 * Wrath replays folder. This includes identifying replays that are added or
 * changed. Specifically, it is expected that replays such as Last
 * Replay.KWReplay will change over time KW writes the latest replays.*/
class ReplayProspector : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString replayDirectory READ replayDirectory WRITE
                   setReplayDirectory NOTIFY replayDirectoryChanged)

   public:
    explicit ReplayProspector(QObject* parent = nullptr);

    // Best-effort guess at this machine's Kane's Wrath replay folder, based
    // on the current Documents location. This is only a suggestion for
    // prefilling the UI - the prospector itself stays inert until
    // setReplayDirectory() is called explicitly.
    static QString defaultReplayDirectory();

    [[nodiscard]] QString replayDirectory() const { return m_replayDirectory; }

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
    void starting();
    void replayDirectoryChanged(const QString& path);
    void replayDiscovered(const QString& filePath);

   private:
    void processItem(const QFileInfo&);
    // If the item is new or updated handle the insert and emission
    void handleUpdatedItem(const QString&, const QFileInfo&);

    QFileSystemWatcher m_watcher;
    QString m_replayDirectory;

    // Store the known paths of
    QMap<QString, QFileInfo> m_knownFiles;
};

}  // namespace KWLegion
