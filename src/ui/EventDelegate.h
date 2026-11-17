#pragma once

#include <QStyledItemDelegate>

namespace p2pquake::ui {

// 一覧の 1 行を「震度バッジ + タイトル + 補足」の 2 行レイアウトで描画する。
class EventDelegate : public QStyledItemDelegate
{
    Q_OBJECT

public:
    using QStyledItemDelegate::QStyledItemDelegate;

    void paint(QPainter *painter, const QStyleOptionViewItem &option,
               const QModelIndex &index) const override;
    QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const override;
};

} // namespace p2pquake::ui
