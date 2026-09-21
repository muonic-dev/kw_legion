/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Muonic
 */

#include <QByteArray>
#include <QDateTime>
#include <QObject>
#include <QString>
#include <QUrl>
#include <variant>

class QNetworkAccessManager;

namespace KWLegionCore {

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
 * The policy implementation of checks
 */
class Checker : public QObject {
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(Checker)

   public:
    explicit Checker(const QUrl& githubApi, QNetworkAccessManager* nam,
                     QObject* parent = nullptr);
    ~Checker() override = default;

    [[nodiscard]] bool isChecking() const { return m_checking; }

    [[nodiscard]] bool isCheckDue(const QDateTime& now) const;

    void setLastUpdateCheck(QDateTime timeOf) {
        m_lastCheck = std::move(timeOf);
    }
    void setLastSuccessfulUpdateCheck(QDateTime timeOf) {
        m_lastSuccessfulCheck = std::move(timeOf);
    }
    void setChecksEnabled(bool enabled) { m_checksEnabled = enabled; }

    void startCheck();

    static CheckResult parseReleaseResponse(const QByteArray& body);

   signals:
    void checkComplete(CheckResult result);
    void checkingChanged();

   private:
    void finishCheck(CheckResult result);

    bool m_checking = false;

    QUrl m_latestRelease;
    QNetworkAccessManager* m_nam;

    // The state
    QDateTime m_lastCheck;
    QDateTime m_lastSuccessfulCheck;
    bool m_checksEnabled = false;
};
}  // namespace KWLegionCore
