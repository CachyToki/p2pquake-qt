#include "ui/EventListModel.h"

#include <algorithm>

namespace p2pquake::ui {

int EventListModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : static_cast<int>(m_items.size());
}

QVariant EventListModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_items.size())
        return {};

    const Item &item = m_items.at(index.row());
    switch (role) {
    case Qt::DisplayRole:
        return item.summary.title;
    case Qt::ToolTipRole:
        return item.summary.subtitle;
    case SummaryRole:
        return QVariant::fromValue(item.summary);
    case CategoryRole:
        return static_cast<int>(item.summary.category);
    default:
        return {};
    }
}

EventListModel::AddResult EventListModel::addEvent(const Event &event)
{
    if (!isListed(event) || m_ids.contains(event.id))
        return AddResult::Ignored;

    // 地震感知情報の解析結果は、同じ地震 (started_at が同じ) について何度も更新が届く。
    // 行を増やさず、最新の内容で置き換える。
    if (const auto *evaluation = event.as<UserquakeEvaluation>()) {
        const int row = findEvaluation(evaluation->startedAt);
        if (row >= 0) {
            Item &item = m_items[row];
            if (event.time < item.event.time)
                return AddResult::Ignored;
            m_ids.remove(item.event.id);
            m_ids.insert(event.id);
            item.event = event;
            item.summary = summarize(event);
            emit dataChanged(index(row), index(row));
            return AddResult::Updated;
        }
    }

    Item item{event, summarize(event), sortTime(event)};

    // 新しい順を保つ位置に挿入する。同時刻なら後から届いたものを上にする。
    const auto position = std::ranges::partition_point(
            m_items, [&](const Item &other) { return other.sortTime > item.sortTime; });
    const int row = static_cast<int>(position - m_items.begin());
    if (row >= kMaxItems)
        return AddResult::Ignored;

    beginInsertRows({}, row, row);
    m_items.insert(row, std::move(item));
    m_ids.insert(event.id);
    endInsertRows();

    if (m_items.size() > kMaxItems) {
        const int last = static_cast<int>(m_items.size()) - 1;
        beginRemoveRows({}, kMaxItems, last);
        for (int i = kMaxItems; i <= last; ++i)
            m_ids.remove(m_items.at(i).event.id);
        m_items.remove(kMaxItems, last - kMaxItems + 1);
        endRemoveRows();
    }
    return AddResult::Inserted;
}

void EventListModel::clear()
{
    beginResetModel();
    m_items.clear();
    m_ids.clear();
    endResetModel();
}

const Event *EventListModel::eventAt(int row) const
{
    return row >= 0 && row < m_items.size() ? &m_items.at(row).event : nullptr;
}

int EventListModel::findEvaluation(const QDateTime &startedAt) const
{
    for (int i = 0; i < m_items.size(); ++i) {
        const auto *evaluation = m_items.at(i).event.as<UserquakeEvaluation>();
        if (evaluation && evaluation->startedAt == startedAt)
            return i;
    }
    return -1;
}

void CategoryFilterModel::setCategoryVisible(Category category, bool visible)
{
    if (visible == isCategoryVisible(category))
        return;
#if QT_VERSION >= QT_VERSION_CHECK(6, 10, 0)
    beginFilterChange();
#endif
    if (visible)
        m_hidden.remove(category);
    else
        m_hidden.insert(category);
#if QT_VERSION >= QT_VERSION_CHECK(6, 10, 0)
    endFilterChange(QSortFilterProxyModel::Direction::Rows);
#else
    invalidateRowsFilter();
#endif
}

bool CategoryFilterModel::filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent) const
{
    const QModelIndex index = sourceModel()->index(sourceRow, 0, sourceParent);
    const auto category = static_cast<Category>(index.data(EventListModel::CategoryRole).toInt());
    return !m_hidden.contains(category);
}

} // namespace p2pquake::ui
