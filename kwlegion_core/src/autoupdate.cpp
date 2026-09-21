/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Muonic
 */

#include <kwlegion_core/autoupdate.h>

#include <QDir>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QJsonValue>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QSettings>
#include <QSslError>
#include <QStandardPaths>
#include <QTimer>
#include <chrono>
#include <memory>
#include <optional>
#include <utility>

#include "autoupdate_impl.h"

namespace KWLegionCore {

AutoUpdater::AutoUpdater(QObject* parent) : QObject(parent) {}

constexpr const char* const LATEST_PATH =
    "/repos/muonic-dev/kw_legion/releases/latest";

Checker::Checker(const QUrl& githubApi, QNetworkAccessManager* nam,
                 QObject* parent)
    : QObject(parent), m_nam(nam) {
    m_latestRelease = githubApi.resolved(QUrl(LATEST_PATH));
}

constexpr std::chrono::milliseconds FAST_INTERVAL = std::chrono::hours{2};
constexpr std::chrono::milliseconds SLOW_INTERVAL = std::chrono::days{3};

bool Checker::isCheckDue(const QDateTime& now) const {
    if (!m_checksEnabled) {
        return false;
    }

    // No previous attempt
    if (!m_lastCheck.isValid()) {
        return true;
    }
    // General policy is as follows
    // Check slowly if the last check was successful
    // Check more often if the last check was a failure

    // The last check was successful, has it been >= 3 days
    if (m_lastSuccessfulCheck >= m_lastCheck) {
        return (now - m_lastCheck) >= SLOW_INTERVAL;
    }

    // The last check was unsuccessful, so lets try again
    return (now - m_lastCheck) >= FAST_INTERVAL;
}

void Checker::startCheck() {
    if (m_checking) {
        return;
    }

    m_checking = true;
    emit checkingChanged();

    QNetworkRequest request(m_latestRelease);
    QHttpHeaders headers;
    headers.append(QHttpHeaders::WellKnownHeader::UserAgent,
                   "muonic-dev/kw_legion");
    headers.append(QHttpHeaders::WellKnownHeader::Accept,
                   "application/vnd.github+json");
    headers.append("X-GitHub-Api-Version", "2026-03-10");
    request.setHeaders(headers);

    Q_ASSERT(m_nam != nullptr);
    QNetworkReply* reply = m_nam->get(request);

    // QNetworkReply reports HTTP failures through errorOccurred too. Record
    // the transport error here, but finish from finished() so every request
    // produces exactly one result and the HTTP status remains available for
    // classification.
    auto networkError = std::make_shared<std::optional<QString>>();
    QObject::connect(reply, &QNetworkReply::errorOccurred, this,
                     [reply, networkError](QNetworkReply::NetworkError error) {
                         *networkError = QStringLiteral("%1 (network error %2)")
                                             .arg(reply->errorString())
                                             .arg(static_cast<int>(error));
                     });

    QObject::connect(reply, &QNetworkReply::finished, reply,
                     &QObject::deleteLater);
    QObject::connect(
        reply, &QNetworkReply::finished, this, [this, reply, networkError]() {
            const QVariant statusAttribute =
                reply->attribute(QNetworkRequest::HttpStatusCodeAttribute);
            if (statusAttribute.isValid()) {
                const int statusCode = statusAttribute.toInt();
                if (statusCode != 200) {
                    finishCheck(HttpFailedCheck{
                        .statusCode = statusCode,
                        .message = reply->errorString(),
                    });
                    return;
                }
            }

            if (networkError->has_value()) {
                finishCheck(GeneralFailedCheck{
                    .message = networkError->value(),
                });
                return;
            }

            finishCheck(parseReleaseResponse(reply->readAll()));
        });
}

void Checker::finishCheck(CheckResult result) {
    m_checking = false;
    emit checkingChanged();
    emit checkComplete(std::move(result));
}

CheckResult Checker::parseReleaseResponse(const QByteArray& body) {
    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(body, &parseError);

    if (parseError.error != QJsonParseError::NoError) {
        return GeneralFailedCheck{
            .message = QStringLiteral("Malformed release JSON: %1 at offset %2")
                           .arg(parseError.errorString())
                           .arg(parseError.offset),
        };
    }

    if (!document.isObject()) {
        return GeneralFailedCheck{
            .message = QStringLiteral("Release response is not a JSON object"),
        };
    }

    const QJsonObject release = document.object();

    const QJsonValue name = release.value("name");
    const QJsonValue tagName = release.value("tag_name");
    const QJsonValue htmlUrl = release.value("html_url");

    if (!tagName.isString() || !htmlUrl.isString() || !name.isString()) {
        return GeneralFailedCheck{
            .message = QStringLiteral(
                "Release response is missing a string name, tag_name, or "
                "html_url field"),
        };
    }

    const QString releaseName = name.toString();
    const QString releaseVersion = tagName.toString();
    const QUrl releaseUrl(htmlUrl.toString());

    if (releaseName.isEmpty() || releaseVersion.isEmpty() ||
        releaseUrl.isEmpty() || !releaseUrl.isValid() ||
        releaseUrl.isRelative()) {
        return GeneralFailedCheck{
            .message = QStringLiteral(
                "Release response contains an empty name or tag, or an "
                "invalid URL"),
        };
    }

    return SuccessfulCheck{
        .name = releaseName,
        .tagName = releaseVersion,
        .releaseUrl = releaseUrl,
    };
}

AutoUpdater* AutoUpdater::create(QQmlEngine* /*qmlEngine*/,
                                 QJSEngine* /*jsEngine*/) {
    return new AutoUpdater;
}

}  // namespace KWLegionCore
