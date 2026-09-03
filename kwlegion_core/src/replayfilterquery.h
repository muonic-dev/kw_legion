/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Muonic
 */

#pragma once

#include "filterquery.h"
#include "replaystoremodel.h"

class QObject;

namespace KWLegionCore {

// Case-insensitive substring match against a single plain-string
// ReplayStoreModel role (matchTitle, mapName, patch, ...).
class TextFieldReplayFilterQuery : public FilterQuery {
    Q_OBJECT
   public:
    TextFieldReplayFilterQuery(ReplayStoreModel::Roles role, QString needle,
                               QObject* parent = nullptr);

    [[nodiscard]] bool acceptRow(const QAbstractItemModel& source, int row,
                                 const QModelIndex& parent) const override;

    [[nodiscard]] QString repr() const override;

    static TextFieldReplayFilterQuery* matchTitle(QString needle,
                                                  QObject* parent = nullptr);
    static TextFieldReplayFilterQuery* mapName(QString needle,
                                               QObject* parent = nullptr);
    static TextFieldReplayFilterQuery* patch(QString needle,
                                             QObject* parent = nullptr);

   private:
    ReplayStoreModel::Roles m_role;
    QString m_needle;
};

class StringListContainsReplayFilterQuery : public FilterQuery {
    Q_OBJECT
   public:
    StringListContainsReplayFilterQuery(ReplayStoreModel::Roles role,
                                        QString needle,
                                        QObject* parent = nullptr);

    [[nodiscard]] bool acceptRow(const QAbstractItemModel& source, int row,
                                 const QModelIndex& parent) const override;

    [[nodiscard]] QString repr() const override;

    static StringListContainsReplayFilterQuery* player(
        QString needle, QObject* parent = nullptr);

   private:
    ReplayStoreModel::Roles m_role;
    QString m_needle;
};

class RelativeDateTimeQuery : public FilterQuery {
    Q_OBJECT
   public:
    enum class Comparison : std::uint8_t { BEFORE, AFTER };
    RelativeDateTimeQuery(ReplayStoreModel::Roles role, QDateTime compareTo,
                          Comparison comp, QObject* parent = nullptr);

    [[nodiscard]] bool acceptRow(const QAbstractItemModel& source, int row,
                                 const QModelIndex& parent) const override;

    [[nodiscard]] QString repr() const override;

   private:
    ReplayStoreModel::Roles m_role;
    QDateTime m_compareTo;
    Comparison m_comparison;
};

// Matches if any of matchTitle/mapName/patch/players contains the needle.
class AnyTextReplayFilterQuery : public DisjunctionFilterQuery {
    Q_OBJECT
   public:
    AnyTextReplayFilterQuery(QString needle, QObject* parent = nullptr);
};
}  // namespace KWLegionCore
