// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Muonic

#include <QDateTime>
#include <QHostAddress>
#include <QNetworkAccessManager>
#include <QTcpServer>
#include <QTest>
#include <QUrl>
#include <catch2/catch_test_macros.hpp>
#include <chrono>
#include <optional>
#include <variant>

#include "autoupdate.h"
#include "autoupdate_impl.h"

using namespace KWLegionCore;

namespace {

const QDateTime CHECK_TIME =
    QDateTime::fromString("2026-09-21T12:00:00Z", Qt::ISODate);

Checker checker() { return Checker{QUrl{"http://127.0.0.1"}, nullptr}; }

}  // namespace

TEST_CASE("Update checks are not due while checks are disabled",
          "[autoupdate][policy]") {
    Checker subject = checker();

    CHECK_FALSE(subject.isCheckDue(CHECK_TIME));
}

TEST_CASE("The first enabled update check is due immediately",
          "[autoupdate][policy]") {
    Checker subject = checker();
    subject.setChecksEnabled(true);

    CHECK(subject.isCheckDue(CHECK_TIME));
}

TEST_CASE("A successful update check uses the slow interval",
          "[autoupdate][policy]") {
    using namespace std::chrono_literals;

    Checker subject = checker();
    subject.setChecksEnabled(true);
    subject.setLastUpdateCheck(CHECK_TIME);
    subject.setLastSuccessfulUpdateCheck(CHECK_TIME);

    CHECK_FALSE(subject.isCheckDue(CHECK_TIME + 72h - 1ms));
    CHECK(subject.isCheckDue(CHECK_TIME + 72h));
}

TEST_CASE("A failed update check uses the fast interval",
          "[autoupdate][policy]") {
    using namespace std::chrono_literals;

    Checker subject = checker();
    subject.setChecksEnabled(true);
    subject.setLastUpdateCheck(CHECK_TIME);

    CHECK_FALSE(subject.isCheckDue(CHECK_TIME + 2h - 1ms));
    CHECK(subject.isCheckDue(CHECK_TIME + 2h));
}

TEST_CASE("A failure after a success switches back to the fast interval",
          "[autoupdate][policy]") {
    using namespace std::chrono_literals;

    Checker subject = checker();
    subject.setChecksEnabled(true);
    subject.setLastSuccessfulUpdateCheck(CHECK_TIME - 1h);
    subject.setLastUpdateCheck(CHECK_TIME);

    CHECK_FALSE(subject.isCheckDue(CHECK_TIME + 2h - 1ms));
    CHECK(subject.isCheckDue(CHECK_TIME + 2h));
}

TEST_CASE("A GitHub release response is parsed into a successful check",
          "[autoupdate][response]") {
    const QByteArray response = R"json({
        "name": "Version 1.2.3",
        "tag_name": "v1.2.3",
        "html_url": "https://github.com/muonic-dev/kw_legion/releases/tag/v1.2.3"
    })json";

    const CheckResult result = Checker::parseReleaseResponse(response);
    REQUIRE(std::holds_alternative<SuccessfulCheck>(result));

    const auto& successful = std::get<SuccessfulCheck>(result);
    CHECK(successful.name == "Version 1.2.3");
    CHECK(successful.tagName == "v1.2.3");
    CHECK(successful.releaseUrl ==
          QUrl{"https://github.com/muonic-dev/kw_legion/releases/tag/v1.2.3"});
}

TEST_CASE("Malformed GitHub JSON is reported as a general failure",
          "[autoupdate][response]") {
    const CheckResult result = Checker::parseReleaseResponse("{not-json");

    REQUIRE(std::holds_alternative<GeneralFailedCheck>(result));
    CHECK_FALSE(std::get<GeneralFailedCheck>(result).message.isEmpty());
}

TEST_CASE("Only newer release versions are offered", "[autoupdate][version]") {
    CHECK(Checker::isReleaseNewer("0.4.0", "v0.5.0"));
    CHECK(Checker::isReleaseNewer("0.4.0-snapshot", "v0.4.0"));
    CHECK_FALSE(Checker::isReleaseNewer("0.4.0", "v0.4.0"));
    CHECK_FALSE(Checker::isReleaseNewer("0.5.0", "v0.4.0"));
}

TEST_CASE("A network error completes the check once and clears checking",
          "[autoupdate][network]") {
    // Reserve an ephemeral port and close it so the request deterministically
    // receives connection-refused rather than depending on a hard-coded port.
    QTcpServer portReservation;
    REQUIRE(portReservation.listen(QHostAddress::LocalHost, 0));
    const quint16 unusedPort = portReservation.serverPort();
    portReservation.close();

    QNetworkAccessManager nam;
    Checker subject{QUrl{QStringLiteral("http://127.0.0.1:%1").arg(unusedPort)},
                    &nam};
    std::optional<CheckResult> result;
    int completionCount = 0;
    QObject::connect(&subject, &Checker::checkComplete, &subject,
                     [&result, &completionCount](CheckResult completed) {
                         result = std::move(completed);
                         ++completionCount;
                     });

    subject.startCheck();
    CHECK(subject.isChecking());

    QTRY_VERIFY_WITH_TIMEOUT(result.has_value(), 2000);
    CHECK(std::holds_alternative<GeneralFailedCheck>(*result));
    CHECK_FALSE(std::get<GeneralFailedCheck>(*result).message.isEmpty());
    CHECK_FALSE(subject.isChecking());
    CHECK(completionCount == 1);
}
