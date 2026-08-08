#pragma once

#include <QFileSystemWatcher>
#include <QObject>
#include <QSet>
#include <QString>

namespace KWLegion {

class ReplayProspector : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString replayDirectory READ replayDirectory WRITE setReplayDirectory NOTIFY
                   replayDirectoryChanged)

   public:
    explicit ReplayProspector(QObject* parent = nullptr);

    // Best-effort guess at this machine's Kane's Wrath replay folder, based
    // on the current Documents location. This is only a suggestion for
    // prefilling the UI - the prospector itself stays inert until
    // setReplayDirectory() is called explicitly.
    static QString defaultReplayDirectory();

    [[nodiscard]] QString replayDirectory() const { return m_replayDirectory; }
    void setReplayDirectory(const QString& path);

   signals:
    void replayDirectoryChanged(const QString& path);
    void replayDiscovered(const QString& filePath);
    void replayRemoved(const QString& filePath);

   private slots:
    void rescan();

   private:
    QFileSystemWatcher m_watcher;
    QString m_replayDirectory;
    QSet<QString> m_knownFiles;
};

}  // namespace KWLegion
