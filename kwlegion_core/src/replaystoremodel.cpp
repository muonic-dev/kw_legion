/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Muonic
 */

#include <kwlegion_core/replayanalyzer.h>
#include <kwlegion_core/replaystore.h>
#include <kwlegion_core/replaystoremodel.h>

#include <QAbstractItemModel>
#include <QDebug>
#include <QHash>
#include <QHashFunctions>
#include <QJSEngine>
#include <QList>
#include <QObject>
#include <QQmlEngine>
#include <QSet>
#include <QUrl>
#include <QVariant>
#include <QtAlgorithms>
#include <QtTypes>
#include <algorithm>
#include <ranges>
#include <utility>

#include "replayanalyzerbridge.h"
#include "replaymodel.h"
#include "teammodel.h"

namespace KWLegionCore {

namespace {

template <typename... Ts>
struct Match : Ts... {
    using Ts::operator()...;
};

QTime formatDuration(quint32 engineTicks) {
    // I think the engine tick rate of 30 was incorrectly and that is UI draw.
    // I believe its actually 15hz empirically
    return QTime(0, 0).addSecs(static_cast<qint32>(engineTicks) / 15);
}

}  // namespace

const QList SELECTED_ROLE{
    static_cast<int>(ReplayStoreModel::Roles::SelectedRole)};

ReplayStoreModel::ReplayStoreModel(QObject* parent)
    : QAbstractListModel(parent),
      m_roleNames{
          {static_cast<int>(Roles::ChecksumRole),
           QByteArrayLiteral("checksum")},
          {static_cast<int>(Roles::TimestampRole),
           QByteArrayLiteral("timestamp")},
          {static_cast<int>(Roles::MatchTitleRole),
           QByteArrayLiteral("matchTitle")},
          {static_cast<int>(Roles::MatchDescriptionRole),
           QByteArrayLiteral("matchDescription")},
          {static_cast<int>(Roles::MapNameRole), QByteArrayLiteral("mapName")},
          {static_cast<int>(Roles::MapReferenceRole),
           QByteArrayLiteral("mapReference")},
          {static_cast<int>(Roles::HasExternalPathRole),
           QByteArrayLiteral("hasExternalPath")},
          {static_cast<int>(Roles::TeamsRole), QByteArrayLiteral("teams")},
          {static_cast<int>(Roles::PlayersRole), QByteArrayLiteral("players")},
          {static_cast<int>(Roles::PatchRole), QByteArrayLiteral("patch")},
          {static_cast<int>(Roles::DurationRole),
           QByteArrayLiteral("duration")},
          {static_cast<int>(Roles::SelectedRole),
           QByteArrayLiteral("selected")},
          {static_cast<int>(Roles::ExpandedRole),
           QByteArrayLiteral("expanded")},
          {static_cast<int>(Roles::AnalysisStateRole),
           QByteArrayLiteral("analysisState")},
          {static_cast<int>(Roles::AnalysisAPMRole),
           QByteArrayLiteral("analysisApm")},
      } {}

ReplayStoreModel::~ReplayStoreModel() = default;

ReplayStoreModel* ReplayStoreModel::create(QQmlEngine* /*qmlEngine*/,
                                           QJSEngine* /*jsEngine*/) {
    // Signature is Qt's QML_SINGLETON factory contract - must return T*, not
    // gsl::owner<T*>. Ownership transfers to the QML engine at the call site.
    return new ReplayStoreModel();
}

void ReplayStoreModel::finishInit(ReplayStore* store,
                                  ReplayAnalyzer* analyzer) {
    connect(store, &ReplayStore::replaysLoaded, this,
            &ReplayStoreModel::replaysLoaded);
    connect(store, &ReplayStore::replaysChanged, this,
            &ReplayStoreModel::replaysChanged);

    connect(this, &ReplayStoreModel::shouldToggleReplayExposed, store,
            &ReplayStore::toggleReplayExposed);
    connect(this, &ReplayStoreModel::shouldExposeReplay, store,
            &ReplayStore::ensureReplayExposed);
    connect(this, &ReplayStoreModel::shouldHideReplay, store,
            &ReplayStore::ensureReplayHidden);
    connect(this, &ReplayStoreModel::shouldSaveReplay, store,
            &ReplayStore::saveReplayAs);
    connect(this, &ReplayStoreModel::shouldExportReplays, store,
            &ReplayStore::exportReplaysAs);

    connect(this, &ReplayStoreModel::shouldClearOverrideTitle, store,
            &ReplayStore::clearOverrideTitle);
    connect(this, &ReplayStoreModel::shouldSetOverrideTitle, store,
            &ReplayStore::setOverrideTitle);
    // Tie ourselves to the replay analyzer
    m_replayAnalyzerBridge = new ReplayAnalyzerBridge(analyzer, this);
}

void ReplayStoreModel::replaysLoaded(const QList<Replay>& replays) {
    beginResetModel();
    qDeleteAll(m_replays);
    m_replays.clear();
    for (const auto& replay : replays) {
        m_replays.append(new ReplayModel(replay, this));
    }
    m_selections.clear();
    m_analysisEntries.clear();
    endResetModel();
}

void ReplayStoreModel::replaysChanged(const QList<Replay>& replays) {
    for (const auto& replay : replays) {
        auto it =
            std::ranges::find_if(m_replays, [&replay](const ReplayModel* r) {
                return r->checksum() == replay.checksum;
            });

        if (it != m_replays.end()) {
            // Update the existing replay proxy with the new data
            auto roles = (*it)->updateFromReplay(replay);
            dataChangedByIter(it, roles);
        } else {
            const int row = static_cast<int>(m_replays.size());
            beginInsertRows(QModelIndex(), row, row);
            m_replays.append(new ReplayModel(replay, this));
            endInsertRows();
        }
    }
}

QString ReplayStoreModel::friendlySaveName(const QByteArray& checksum) {
    auto it = std::ranges::find_if(m_replays, [&checksum](const auto& replay) {
        return replay->checksum() == checksum;
    });
    if (it == std::ranges::end(m_replays)) {
        return {};
    }
    const QString displayTitle = (*it)->displayMatchTitle();
    return QString("%1 - %2").arg(displayTitle,
                                  QString(checksum.toHex()).slice(0, 8));
}

void ReplayStoreModel::setOverrideTitle(const QByteArray& checksum,
                                        const QString& title) {
    // TODO: Extract this to a helper since it occurs so often
    auto it = std::ranges::find_if(m_replays, [&checksum](const auto& replay) {
        return replay->checksum() == checksum;
    });
    if (it == std::ranges::end(m_replays)) {
        return;
    }
    emit shouldSetOverrideTitle(checksum, title);
}

void ReplayStoreModel::clearOverrideTitle(const QByteArray& checksum) {
    // TODO: Extract this to a helper since it occurs so often
    auto it = std::ranges::find_if(m_replays, [&checksum](const auto& replay) {
        return replay->checksum() == checksum;
    });
    if (it == std::ranges::end(m_replays)) {
        return;
    }
    emit shouldClearOverrideTitle(checksum);
}

void ReplayStoreModel::toggleReplayExposed(const QByteArray& checksum) {
    emit shouldToggleReplayExposed(checksum);
}

void ReplayStoreModel::showSelectedReplays() {
    for (const auto& checksum : m_selections) {
        emit shouldExposeReplay(checksum);
    }
}

void ReplayStoreModel::hideSelectedReplays() {
    for (const auto& checksum : m_selections) {
        emit shouldHideReplay(checksum);
    }
}

void ReplayStoreModel::setReplaySelected(const QByteArray& checksum) {
    auto it = std::ranges::find_if(m_replays, [&checksum](ReplayModel* replay) {
        return replay->checksum() == checksum;
    });
    if (it != m_replays.end()) {
        const qsizetype before = m_selections.size();
        m_selections.insert(checksum);
        dataChangedByIter(it, SELECTED_ROLE);
        if (m_selections.size() != before) {
            emit selectionCountChanged();
        }
    }
}

void ReplayStoreModel::extendReplaySelection(
    const QList<QByteArray>& checksums) {
    const qsizetype before = m_selections.size();
    for (const auto& checksum : checksums) {
        auto it =
            std::ranges::find_if(m_replays, [&checksum](ReplayModel* replay) {
                return replay->checksum() == checksum;
            });
        if (it != m_replays.end()) {
            m_selections.insert(checksum);
            dataChangedByIter(it, SELECTED_ROLE);
        }
    }
    if (m_selections.size() != before) {
        emit selectionCountChanged();
    }
}

bool ReplayStoreModel::toggleReplaySelected(const QByteArray& checksum) {
    auto it = std::ranges::find_if(m_replays, [&checksum](ReplayModel* replay) {
        return replay->checksum() == checksum;
    });
    bool active{false};
    if (it != m_replays.end()) {
        if (m_selections.contains(checksum)) {
            m_selections.remove(checksum);
        } else {
            m_selections.insert(checksum);
            active = true;
        }
        dataChangedByIter(it, SELECTED_ROLE);
        emit selectionCountChanged();
    }
    return active;
}

void ReplayStoreModel::clearSelected() {
    const QSet<QByteArray> checksums = std::move(m_selections);
    for (auto it = m_replays.cbegin(); it < m_replays.cend(); ++it) {
        if (checksums.contains((*it)->checksum())) {
            dataChangedByIter(it, SELECTED_ROLE);
        }
    }
    if (!checksums.isEmpty()) {
        emit selectionCountChanged();
    }
}

void ReplayStoreModel::restrictSelectionTo(const QList<QByteArray>& checksums) {
    const QSet<QByteArray> keep{checksums.begin(), checksums.end()};
    const qsizetype before = m_selections.size();
    for (auto it = m_replays.cbegin(); it != m_replays.cend(); ++it) {
        const QByteArray& checksum = (*it)->checksum();
        if (m_selections.contains(checksum) && !keep.contains(checksum)) {
            m_selections.remove(checksum);
            dataChangedByIter(it, SELECTED_ROLE);
        }
    }
    if (m_selections.size() != before) {
        emit selectionCountChanged();
    }
}

void ReplayStoreModel::invertSelection(const QList<QByteArray>& checksums) {
    const QSet<QByteArray> scope{checksums.begin(), checksums.end()};
    const qsizetype before = m_selections.size();
    for (auto it = m_replays.cbegin(); it != m_replays.cend(); ++it) {
        const QByteArray& checksum = (*it)->checksum();
        if (!scope.contains(checksum)) {
            // Outside the scope this invert applies to - drop any stale
            // selection rather than leave it lingering untouched.
            if (m_selections.remove(checksum)) {
                dataChangedByIter(it, SELECTED_ROLE);
            }
            continue;
        }
        if (m_selections.contains(checksum)) {
            m_selections.remove(checksum);
        } else {
            m_selections.insert(checksum);
        }
        dataChangedByIter(it, SELECTED_ROLE);
    }
    if (m_selections.size() != before) {
        emit selectionCountChanged();
    }
}

void ReplayStoreModel::requestAnalysis(const QByteArray& checksum) {
    if (m_analysisEntries.contains(checksum)) {
        return;
    }
    const auto it =
        std::ranges::find_if(m_replays, [&checksum](const ReplayModel* replay) {
            return replay->checksum() == checksum;
        });

    if (it == std::end(m_replays)) {
        return;
    }
    m_analysisEntries.insert(checksum, AsyncValue<ReplayAnalysis>{});

    dataChangedByIter(it, QList{static_cast<int>(Roles::ExpandedRole),
                                static_cast<int>(Roles::AnalysisStateRole)});

    QFuture<AnalysisResult> future = m_replayAnalyzerBridge->analyze(checksum);
    future.then(this, [this, checksum](AnalysisResult result) {
        auto entryIt = m_analysisEntries.find(checksum);
        if (entryIt == m_analysisEntries.end()) {
            // Dismissed before the analysis finished - nothing left to
            // update.
            return;
        }
        std::visit(Match{[&entryIt](AnalysisFailure) { entryIt->error(); },
                         [&entryIt](ReplayAnalysis& analysis) {
                             entryIt->finish(std::move(analysis));
                         }},
                   result);

        const auto rowIt = std::ranges::find_if(
            m_replays, [&checksum](const ReplayModel* replay) {
                return replay->checksum() == checksum;
            });
        dataChangedByIter(rowIt,
                          QList{static_cast<int>(Roles::AnalysisStateRole),
                                static_cast<int>(Roles::AnalysisAPMRole)});
    });
}

void ReplayStoreModel::dismissAnalysis(const QByteArray& checksum) {
    const auto it =
        std::ranges::find_if(m_replays, [&checksum](const ReplayModel* replay) {
            return replay->checksum() == checksum;
        });
    if (it == std::ranges::end(m_replays)) {
        return;
    }
    m_analysisEntries.remove(checksum);
    dataChangedByIter(it, QList{static_cast<int>(Roles::ExpandedRole)});
}

void ReplayStoreModel::saveReplayAs(const QByteArray& checksum,
                                    const QUrl& path) {
    emit shouldSaveReplay(checksum, path);
}

void ReplayStoreModel::exportSelectedReplaysTo(const QUrl& path) {
    const QList<QByteArray> checksums{m_selections.begin(), m_selections.end()};
    emit shouldExportReplays(checksums, path);
}

QHash<int, QByteArray> ReplayStoreModel::roleNames() const {
    return m_roleNames;
}

int ReplayStoreModel::rowCount(const QModelIndex& parent) const {
    if (parent.isValid()) {
        return 0;
    }
    return static_cast<int>(m_replays.size());
}

QVariant ReplayStoreModel::data(const QModelIndex& index, int role) const {
    if (!index.isValid() ||
        std::cmp_greater_equal(index.row(), m_replays.size())) {
        return {};
    }
    const ReplayModel* replay = m_replays.at(index.row());
    switch (static_cast<Roles>(role)) {
        case Roles::ChecksumRole:
            return replay->checksum();
        case Roles::TimestampRole:
            return replay->timestamp();
        case Roles::MatchTitleRole:
            return replay->displayMatchTitle();
        case Roles::MatchDescriptionRole:
            return replay->matchDescription();
        case Roles::MapNameRole:
            return replay->mapName();
        case Roles::HasExternalPathRole:
            return replay->hasExternalPath();
        case Roles::TeamsRole:
            return QVariant::fromValue(replay->teams());
        case Roles::PlayersRole: {
            QStringList players;
            for (const auto* const obj : replay->teams()) {
                const auto* const team = qobject_cast<const TeamModel*>(obj);
                if (team == nullptr) {  // should be impossible
                    continue;
                }
                players.append(team->playerNames());
            }
            return players;
        }
        case Roles::SelectedRole:
            return m_selections.contains(replay->checksum());
        case Roles::ExpandedRole:
            return m_analysisEntries.contains(replay->checksum());
        case Roles::AnalysisStateRole: {
            const auto it = m_analysisEntries.constFind(replay->checksum());
            if (it == m_analysisEntries.cend()) {
                return {};
            }
            return QVariant::fromValue(it->state());
        }
        case Roles::AnalysisAPMRole: {
            const auto it = m_analysisEntries.constFind(replay->checksum());
            if (it == m_analysisEntries.cend() ||
                it->state() != AsyncState::Complete) {
                return {};
            }
            return QVariant::fromValue(it->value().apmPlot);
        }
        case Roles::PatchRole:
            return replay->inferPatch();
        case Roles::DurationRole:
            return formatDuration(replay->engineTicks());
        default:
            return {};
    }
}

}  // namespace KWLegionCore