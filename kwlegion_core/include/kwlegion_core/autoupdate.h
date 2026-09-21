/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Muonic
 */
#pragma once

#include <qqmlintegration.h>

#include <QDateTime>
#include <QObject>
#include <QUrl>

class QSettings;
class QTimer;
class QNetworkAccessManager;
class QQmlEngine;
class QJSEngine;

namespace KWLegionCore {
class Settings;

using Clock = std::function<QDateTime()>;

class Checker;

/**
 * The application facing component of auto-update
 */
class AutoUpdater : public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON
    Q_DISABLE_COPY_MOVE(AutoUpdater)
   public:
    explicit AutoUpdater(QObject* parent = nullptr);
    ~AutoUpdater() override = default;

    static AutoUpdater* create(QQmlEngine* qmlEngine, QJSEngine* jsEngine);

    static Clock systemClock() {
        return [] { return QDateTime::currentDateTimeUtc(); };
    }

    static QUrl githubApi() { return {"https://api.github.com"}; }

    void finishInit(Settings* settings);

   private:
    QSettings* m_cache;
    QTimer* m_timer;
    Checker* m_checker;
    Clock m_clock;
};

}  // namespace KWLegionCore