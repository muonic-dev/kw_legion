/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Muonic
 */

#pragma once

#include <QObject>

class QAbstractItemModel;
class QModelIndex;

namespace KWLegionCore {

/**
 * Abstract base class of filter queries
 */
class FilterQuery : public QObject {
    Q_OBJECT
   public:
    FilterQuery(QObject* parent = nullptr);

    /**
     * Invoked to determine whether to accept the a row for filtering or not
     *
     * The implementor is responsible for mapping from itself into the
     * appropriate roles to request
     */
    [[nodiscard]] virtual bool acceptRow(const QAbstractItemModel& source,
                                         int row,
                                         const QModelIndex& parent) const = 0;
};

/**
 * The default filter query which accepts all rows
 */
class TautologyFilterQuery : public FilterQuery {
    Q_OBJECT
   public:
    TautologyFilterQuery(QObject* parent = nullptr);

    [[nodiscard]] bool acceptRow(const QAbstractItemModel& source, int row,
                                 const QModelIndex& parent) const override;
};

class ContradictionFilterQuery : public FilterQuery {
    Q_OBJECT
   public:
    ContradictionFilterQuery(QObject* parent = nullptr);

    [[nodiscard]] bool acceptRow(const QAbstractItemModel& source, int row,
                                 const QModelIndex& parent) const override;
};

/**
 * A conjuction of multiple filter queries
 *
 * If there are no added queries then acceptRow treats this as
 * no falsifiable predicates and returns true
 */
class ConjunctionFilterQuery : public FilterQuery {
    Q_OBJECT
   public:
    ConjunctionFilterQuery(QObject* parent = nullptr);

    /**
     *  Add a predicate query. ConjuctionFilterQuery takes ownership via
     * parentage
     */
    void addQuery(FilterQuery* query);

    [[nodiscard]] bool acceptRow(const QAbstractItemModel& source, int row,
                                 const QModelIndex& parent) const override;

   private:
    QList<FilterQuery*> m_conjuctionOf;
};

/**
 * A disjunction of multiple filter queries
 *
 * If there are no added queries then acceptRow treats this as
 * no verifiable predicates and returns false
 */
class DisjunctionFilterQuery : public FilterQuery {
    Q_OBJECT

   public:
    DisjunctionFilterQuery(QObject* parent = nullptr);

    /**
     * Add a predicate query. DisjunctionFilterQuery takes ownership via
     * parentage
     */
    void addQuery(FilterQuery* query);

    [[nodiscard]] bool acceptRow(const QAbstractItemModel& source, int row,
                                 const QModelIndex& parent) const override;

   private:
    QList<FilterQuery*> m_disjunctionOf;
};

}  // namespace KWLegionCore