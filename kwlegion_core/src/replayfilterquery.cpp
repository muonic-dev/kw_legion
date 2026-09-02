/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Muonic
 */

#include "replayfilterquery.h"

#include <QAbstractItemModel>

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

AnyTextReplayFilterQuery::AnyTextReplayFilterQuery(QString needle,
                                                   QObject* parent)
    : DisjunctionFilterQuery(parent) {
    addQuery(TextFieldReplayFilterQuery::matchTitle(needle));
    addQuery(TextFieldReplayFilterQuery::mapName(needle));
    addQuery(TextFieldReplayFilterQuery::patch(std::move(needle)));
}
}  // namespace KWLegionCore
