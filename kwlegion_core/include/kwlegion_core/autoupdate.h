/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Muonic
 */
#pragma once

#include <qqmlintegration.h>

#include <QDateTime>
#include <QLoggingCategory>
#include <QObject>
#include <QString>
#include <QUrl>
#include <functional>
#include <optional>
#include <variant>

class QSettings;
class QTimer;
class QNetworkAccessManager;
class QQmlEngine;
class QJSEngine;

namespace KWLegionCore {
Q_DECLARE_LOGGING_CATEGORY(logAutoUpdater);

class Settings;

using Clock = std::function<QDateTime()>;

class Checker;

struct SuccessfulCheck {
    QString name;
    QString tagName;
    QUrl releaseUrl;
};

struct HttpFailedCheck {
    int statusCode;
    QString message;
};

struct GeneralFailedCheck {
    QString message;
};

using CheckResult =
    std::variant<SuccessfulCheck, HttpFailedCheck, GeneralFailedCheck>;

/**
 * The application facing component of auto-update
 */
class AutoUpdater : public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON
    Q_DISABLE_COPY_MOVE(AutoUpdater)
    Q_PROPERTY(bool updateAvailable READ updateAvailable NOTIFY
                   availableReleaseChanged)
    Q_PROPERTY(QString availableVersion READ availableVersion NOTIFY
                   availableReleaseChanged)
    Q_PROPERTY(QString availableReleaseName READ availableReleaseName NOTIFY
                   availableReleaseChanged)
    Q_PROPERTY(QUrl availableReleaseUrl READ availableReleaseUrl NOTIFY
                   availableReleaseChanged)
   public:
    explicit AutoUpdater(QObject* parent = nullptr);
    ~AutoUpdater() override = default;

    static AutoUpdater* create(QQmlEngine* qmlEngine, QJSEngine* jsEngine);

    static Clock systemClock() {
        return [] { return QDateTime::currentDateTimeUtc(); };
    }

    static QUrl githubApi() { return {"https://api.github.com"}; }

    void finishInit(Settings* settings);

    [[nodiscard]] bool updateAvailable() const;
    [[nodiscard]] QString availableVersion() const;
    [[nodiscard]] QString availableReleaseName() const;
    [[nodiscard]] QUrl availableReleaseUrl() const;

    Q_INVOKABLE void dismissAvailableRelease();

   signals:
    void availableReleaseChanged();

   private:
    void timerFired();
    void reapplyCheckPolicy();
    void checkComplete(CheckResult result);

    Clock m_clock = systemClock();
    QSettings* m_cache = nullptr;
    QTimer* m_timer;
    QNetworkAccessManager* m_nam;
    Checker* m_checker;
    std::optional<SuccessfulCheck> m_availableRelease;

    // Not initialized by the constructor, should be gated
    Settings* m_settings = nullptr;
};

}  // namespace KWLegionCore
