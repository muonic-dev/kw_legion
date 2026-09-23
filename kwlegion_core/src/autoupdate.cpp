/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Muonic
 */

#include <kwlegion_core/appinfo.h>
#include <kwlegion_core/autoupdate.h>
#include <kwlegion_core/settings.h>

#include <QDebug>
#include <QDir>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QJsonValue>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QSettings>
#include <QStandardPaths>
#include <QTimer>
#include <QVersionNumber>
#include <chrono>
#include <memory>
#include <optional>
#include <utility>

#include "autoupdate_impl.h"
#include "version.h"

namespace {
template <typename... Ts>
struct Match : Ts... {
    using Ts::operator()...;
};
}  // namespace

namespace KWLegionCore {

Q_LOGGING_CATEGORY(logAutoUpdater, "kwlegion.autoupdater");

constexpr const char* const LAST_SUCCESSFUL_KEY = "lastSuccessfulCheck";
constexpr const char* const LAST_KEY = "lastCheck";
constexpr const char* const MOST_RECENTLY_DISMISSED_RELEASE_KEY =
    "mostRecentlyDismissedRelease";

// Autoupdater is a sub-composition root responsible for wiring together the
// relevant pieces for update
AutoUpdater::AutoUpdater(QObject* parent)
    : QObject(parent),
      m_timer(new QTimer(this)),
      m_nam(new QNetworkAccessManager(this)),
      m_checker(new Checker(githubApi(), m_nam, this)) {
    m_timer->setInterval(std::chrono::minutes{15});
    m_timer->callOnTimeout(this, &AutoUpdater::timerFired);
    connect(m_checker, &Checker::checkComplete, this,
            &AutoUpdater::checkComplete);

    const QString cacheLocation =
        QStandardPaths::writableLocation(QStandardPaths::CacheLocation);
    if (cacheLocation.isEmpty() || !QDir().mkpath(cacheLocation)) {
        qCWarning(logAutoUpdater).noquote()
            << "Automatic update checks disabled: unable to create cache "
               "directory:"
            << cacheLocation;
        return;
    }
    const QString path = cacheLocation + "/autoupdate.ini";
    m_cache = new QSettings(path, QSettings::Format::IniFormat, this);
}

AutoUpdater* AutoUpdater::create(QQmlEngine* /*qmlEngine*/,
                                 QJSEngine* /*jsEngine*/) {
    return new AutoUpdater;
}

void AutoUpdater::finishInit(Settings* settings) {
    Q_ASSERT(settings != nullptr);
    Q_ASSERT(m_settings == nullptr);
    m_settings = settings;

    connect(m_settings, &Settings::checkForUpdatesChanged, this,
            &AutoUpdater::reapplyCheckPolicy);
    connect(m_settings, &Settings::checkForUpdatesChanged, this,
            &AutoUpdater::timerFired);

    reapplyCheckPolicy();
    timerFired();
    m_timer->start();
}

bool AutoUpdater::updateAvailable() const {
    return m_availableRelease.has_value();
}

QString AutoUpdater::availableVersion() const {
    return m_availableRelease ? m_availableRelease->tagName : QString{};
}

QString AutoUpdater::availableReleaseName() const {
    return m_availableRelease ? m_availableRelease->name : QString{};
}

QUrl AutoUpdater::availableReleaseUrl() const {
    return m_availableRelease ? m_availableRelease->releaseUrl : QUrl{};
}

void AutoUpdater::dismissAvailableRelease() {
    if (!m_availableRelease) {
        return;
    }
    m_cache->setValue(MOST_RECENTLY_DISMISSED_RELEASE_KEY,
                      m_availableRelease->tagName);
    m_cache->sync();
    if (m_cache->status() != QSettings::NoError) {
        qCWarning(logAutoUpdater)
            << "Unable to persist the dismissed update release";
    }
    m_availableRelease.reset();
    emit availableReleaseChanged();
}

void AutoUpdater::timerFired() {
    // Lazy init or could fail to initialize so we we must guard here
    if (m_cache == nullptr || m_settings == nullptr) {
        return;
    }
    const QDateTime now{m_clock()};
    if (m_checker->isCheckDue(now)) {
        m_checker->startCheck();
        m_cache->setValue(LAST_KEY, now);
        reapplyCheckPolicy();
    }
}

void AutoUpdater::reapplyCheckPolicy() {
    // Lazy init or could fail to initialize so we we must guard here
    if (m_cache == nullptr || m_settings == nullptr) {
        return;
    }

    m_checker->setChecksEnabled(m_settings->checkForUpdates());
    m_checker->setLastSuccessfulUpdateCheck(
        m_cache->value(LAST_SUCCESSFUL_KEY).toDateTime());
    m_checker->setLastUpdateCheck(m_cache->value(LAST_KEY).toDateTime());
}

void AutoUpdater::checkComplete(CheckResult checkResult) {
    const QDateTime now{m_clock()};
    std::visit(Match{
                   [this, &now](SuccessfulCheck check) {
                       m_cache->setValue(LAST_SUCCESSFUL_KEY, now);
                       m_checker->setLastSuccessfulUpdateCheck(now);

                       const QString dismissedRelease =
                           m_cache->value(MOST_RECENTLY_DISMISSED_RELEASE_KEY)
                               .toString();
                       // Debugging aid: debug builds deliberately offer the
                       // latest published release even when it predates the
                       // running build, making the snackbar/browser flow easy
                       // to exercise against the repository's prior release.
                       const bool shouldOfferRelease =
                           DEBUG_BUILD ||
                           Checker::isReleaseNewer(
                               QString::fromUtf8(KW_LEGION_VERSION_SEMVER),
                               check.tagName);
                       if (shouldOfferRelease &&
                           dismissedRelease != check.tagName) {
                           m_availableRelease = std::move(check);
                           emit availableReleaseChanged();
                       }
                   },
                   [](const HttpFailedCheck& failure) {
                       qCWarning(logAutoUpdater).noquote()
                           << "Update check failed with HTTP status"
                           << failure.statusCode << ":" << failure.message;
                   },
                   [](const GeneralFailedCheck& failure) {
                       qCWarning(logAutoUpdater).noquote()
                           << "Update check failed:" << failure.message;
                   },
               },
               std::move(checkResult));
}

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

bool Checker::isReleaseNewer(const QString& currentVersion,
                             const QString& releaseTag) {
    const auto normalize = [](QString version) {
        if (version.startsWith('v', Qt::CaseInsensitive)) {
            version.remove(0, 1);
        }
        return version;
    };

    const QString current = normalize(currentVersion);
    const QString release = normalize(releaseTag);
    if (current == release) {
        return false;
    }

    qsizetype currentSuffix = 0;
    qsizetype releaseSuffix = 0;
    const QVersionNumber currentNumber =
        QVersionNumber::fromString(current, &currentSuffix);
    const QVersionNumber releaseNumber =
        QVersionNumber::fromString(release, &releaseSuffix);
    if (currentNumber.isNull() || releaseNumber.isNull()) {
        // GitHub has already identified this as the latest release. If its tag
        // is non-SemVer, only suppress an exact match.
        return true;
    }

    const int comparison =
        QVersionNumber::compare(releaseNumber, currentNumber);
    if (comparison != 0) {
        return comparison > 0;
    }

    // Equal numeric versions: a stable release supersedes a local prerelease.
    return releaseSuffix == release.size() && currentSuffix != current.size();
}

}  // namespace KWLegionCore
