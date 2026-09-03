/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Muonic
 */

#include "replayfilterquery.h"

#include <QAbstractItemModel>
#include <QMetaEnum>
#include <QObject>
#include <QVariant>
#include <Qt>
#include <utility>

#include "filterquery.h"
#include "replaystoremodel.h"

namespace KWLegionCore {
TextFieldReplayFilterQuery::TextFieldReplayFilterQuery(
    ReplayStoreModel::Roles role, QString needle, QObject* parent)
    : FilterQuery(parent), m_role(role), m_needle(std::move(needle)) {}

bool TextFieldReplayFilterQuery::acceptRow(const QAbstractItemModel& source,
                                           int row,
                                           const QModelIndex& parent) const {
    const QVariant value =
        source.data(source.index(row, 0, parent), static_cast<int>(m_role));
    if (value.typeId() != QMetaType::QString) {
        return false;
    }
    return value.toString().contains(m_needle, Qt::CaseInsensitive);
}

QString TextFieldReplayFilterQuery::repr() const {
    // Reuses the Roles enum's own key names (e.g. "MapNameRole") rather than
    // keeping a second field-name table in sync with the parser's dispatch
    // table.
    const QMetaEnum roleEnum = QMetaEnum::fromType<ReplayStoreModel::Roles>();
    return QStringLiteral("%1=%2").arg(
        QString::fromUtf8(roleEnum.valueToKey(static_cast<int>(m_role))),
        m_needle);
}

// Memory management is by qobject hierarchy
// NOLINTBEGIN(cppcoreguidelines-owning-memory)
TextFieldReplayFilterQuery* TextFieldReplayFilterQuery::matchTitle(
    QString needle, QObject* parent) {
    return new TextFieldReplayFilterQuery(
        ReplayStoreModel::Roles::MatchTitleRole, std::move(needle), parent);
}

TextFieldReplayFilterQuery* TextFieldReplayFilterQuery::mapName(
    QString needle, QObject* parent) {
    return new TextFieldReplayFilterQuery(ReplayStoreModel::Roles::MapNameRole,
                                          std::move(needle), parent);
}

TextFieldReplayFilterQuery* TextFieldReplayFilterQuery::patch(QString needle,
                                                              QObject* parent) {
    return new TextFieldReplayFilterQuery(ReplayStoreModel::Roles::PatchRole,
                                          std::move(needle), parent);
}
// NOLINTEND(cppcoreguidelines-owning-memory)

AnyTextReplayFilterQuery::AnyTextReplayFilterQuery(QString needle,
                                                   QObject* parent)
    : DisjunctionFilterQuery(parent) {
    addQuery(TextFieldReplayFilterQuery::matchTitle(needle));
    addQuery(TextFieldReplayFilterQuery::mapName(needle));
    addQuery(TextFieldReplayFilterQuery::patch(std::move(needle)));
}
}  // namespace KWLegionCore
