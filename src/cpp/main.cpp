#include <QDebug>
#include <QDir>
#include <QFile>
#include <QGuiApplication>
#include <QMutex>
#include <QQmlApplicationEngine>
#include <QStandardPaths>
#include <QTextStream>

namespace {

void fileMessageHandler(QtMsgType type, const QMessageLogContext& context,
                        const QString& msg) {
    static QMutex mutex;
    static QFile file;

    const QMutexLocker locker(&mutex);

    if (!file.isOpen()) {
        const QString dir =
            QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
        QDir().mkpath(dir);
        file.setFileName(QDir(dir).filePath("kw_legion.log"));
        if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate |
                       QIODevice::Text)) {
            return;
        }
    }

    const char* level = "Debug";
    switch (type) {
        case QtDebugMsg:
            level = "Debug";
            break;
        case QtInfoMsg:
            level = "Info";
            break;
        case QtWarningMsg:
            level = "Warning";
            break;
        case QtCriticalMsg:
            level = "Critical";
            break;
        case QtFatalMsg:
            level = "Fatal";
            break;
    }

    QTextStream stream(&file);
    stream << '[' << level << "] " << msg << " (" << context.file << ':'
           << context.line << ")\n";
    stream.flush();
    file.flush();
}

}  // namespace

// NOLINTNEXTLINE(modernize-avoid-c-arrays, cppcoreguidelines-avoid-c-arrays)
int main(int argc, char* argv[]) {
    QCoreApplication::setApplicationName("kw_legion");

    qInstallMessageHandler(fileMessageHandler);

    QGuiApplication app(argc, argv);

    QQmlApplicationEngine engine;
    QObject::connect(
        &engine, &QQmlApplicationEngine::objectCreationFailed, &app,
        [](const QUrl& url) {
            qCritical() << "Failed to create QML object from" << url;
            QCoreApplication::exit(-1);
        },
        Qt::QueuedConnection);
    QObject::connect(&engine, &QQmlEngine::warnings, &app,
                     [](const QList<QQmlError>& errors) {
                         for (const auto& error : errors) {
                             qWarning() << error.toString();
                         }
                     });
    engine.loadFromModule("kw_legion", "Main");

    return QGuiApplication::exec();
}
