#pragma once

#include "core/Types.h"
#include "ui/Presenter.h"

#include <QAbstractListModel>
#include <QSet>
#include <QSortFilterProxyModel>

namespace p2pquake::ui {

// 受信した情報の一覧。新しい順に並ぶ。
class EventListModel : public QAbstractListModel
{
    Q_OBJECT

public:
    enum Role { SummaryRole = Qt::UserRole + 1, CategoryRole };
    enum class AddResult { Ignored, Inserted, Updated };

    static constexpr int kMaxItems = 500;

    using QAbstractListModel::QAbstractListModel;

    int rowCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role) const override;

    // 情報を追加する。同じ id は無視し、地震感知情報の解析結果は同じ地震の行を上書きする。
    AddResult addEvent(const Event &event);
    void clear();

    const Event *eventAt(int row) const;

private:
    struct Item {
        Event event;
        Summary summary;
        QDateTime sortTime;
    };

    int findEvaluation(const QDateTime &startedAt) const;

    QList<Item> m_items;
    QSet<QString> m_ids;
};

// 種類ごとの表示・非表示を切り替えるフィルタ。
class CategoryFilterModel : public QSortFilterProxyModel
{
    Q_OBJECT

public:
    using QSortFilterProxyModel::QSortFilterProxyModel;

    void setCategoryVisible(Category category, bool visible);
    bool isCategoryVisible(Category category) const { return !m_hidden.contains(category); }

protected:
    bool filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent) const override;

private:
    QSet<Category> m_hidden;
};

} // namespace p2pquake::ui
