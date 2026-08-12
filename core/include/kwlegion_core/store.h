#pragma once

#include <kwlegion_core/kwlegion_core_export.h>

#include <QDir>
#include <QLoggingCategory>
#include <QObject>
#include <QSqlDatabase>

Q_DECLARE_LOGGING_CATEGORY(logStore);

namespace KWLegionCore {
class KWLEGION_CORE_EXPORT ReplayStore : public QObject {
    Q_OBJECT

   public:
    explicit ReplayStore(QObject* parent = nullptr);

    void startup();
    void analyzeReplay(const QString& path);

   private:
    void ensureDb();
    void ensureDirectories();
    // Run all of the relevant ddl to bring the database up to current version
    void executeDdl();

    QSqlDatabase m_db;
    QString m_dbPath;
    QString m_replayDir;
    QString m_scratchDir;
};
}  // namespace KWLegionCore