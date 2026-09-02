/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Muonic
 */

#pragma once

#include <QtQml/qqmlregistration.h>

#include <QObject>
#include <QString>

namespace KWLegionCore {

class FilterQuery;

class ReplayFilterQueryParser : public QObject {
    Q_OBJECT
    QML_ELEMENT

    Q_PROPERTY(QString queryText READ queryText WRITE setQueryText NOTIFY
                   queryTextChanged);
    Q_PROPERTY(QObject* query READ query NOTIFY queryChanged);

   public:
    ReplayFilterQueryParser(QObject* parent = nullptr);

    [[nodiscard]] QString queryText() const;
    void setQueryText(const QString& text);

    [[nodiscard]] QObject* query() const;

   signals:
    void queryTextChanged();
    void queryChanged();

   private:
    QString m_text;
    FilterQuery* m_current;
};

}  // namespace KWLegionCore
