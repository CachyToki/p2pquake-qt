#include "ui/EventDelegate.h"

#include "ui/EventListModel.h"
#include "ui/Presenter.h"

#include <QApplication>
#include <QPainter>

#include <algorithm>

namespace p2pquake::ui {

namespace {

constexpr int kMargin = 8;
constexpr int kSpacing = 10;
constexpr int kLineSpacing = 3;
constexpr int kBadgeRadius = 6;

int badgeSize(const QFontMetrics &metrics)
{
    return std::max(40, metrics.height() * 2 + kLineSpacing);
}

// バッジの文字数に応じて、枠に収まる大きさの太字フォントを選ぶ。
QFont badgeFont(const QFont &base, const QString &text, int size)
{
    QFont font = base;
    font.setBold(true);
    qreal pointSize = base.pointSizeF() * 1.7;
    for (; pointSize > 6.0; pointSize -= 0.5) {
        font.setPointSizeF(pointSize);
        if (QFontMetrics(font).horizontalAdvance(text) <= size - 6)
            break;
    }
    return font;
}

} // namespace

void EventDelegate::paint(QPainter *painter, const QStyleOptionViewItem &option,
                          const QModelIndex &index) const
{
    QStyleOptionViewItem opt = option;
    initStyleOption(&opt, index);
    opt.text.clear(); // 文字は自前で描く

    const QWidget *widget = opt.widget;
    QStyle *style = widget ? widget->style() : QApplication::style();
    style->drawPrimitive(QStyle::PE_PanelItemViewItem, &opt, painter, widget);

    const auto summary = index.data(EventListModel::SummaryRole).value<Summary>();
    const bool selected = opt.state & QStyle::State_Selected;
    const QPalette::ColorGroup group = !(opt.state & QStyle::State_Enabled) ? QPalette::Disabled
            : (opt.state & QStyle::State_Active) ? QPalette::Active : QPalette::Inactive;
    const QColor textColor = opt.palette.color(group, selected ? QPalette::HighlightedText
                                                                : QPalette::Text);
    QColor mutedColor = textColor;
    mutedColor.setAlphaF(0.65f);

    painter->save();
    painter->setRenderHint(QPainter::Antialiasing);

    const QRect content = opt.rect.adjusted(kMargin, kMargin, -kMargin, -kMargin);
    const int size = badgeSize(opt.fontMetrics);

    // バッジ
    const QRect badge(content.left(), content.top() + (content.height() - size) / 2, size, size);
    // 震度1 のような淡い色でも背景に埋もれないよう、少し濃い色で縁取る。
    painter->setPen(QPen(summary.badgeBackground.darker(125), 1));
    painter->setBrush(summary.badgeBackground);
    painter->drawRoundedRect(QRectF(badge).adjusted(0.5, 0.5, -0.5, -0.5), kBadgeRadius, kBadgeRadius);
    painter->setPen(summary.badgeForeground);
    painter->setFont(badgeFont(opt.font, summary.badge, size));
    painter->drawText(badge, Qt::AlignCenter, summary.badge);

    // 文字 2 行
    const int textLeft = badge.right() + 1 + kSpacing;
    const int lineHeight = opt.fontMetrics.height();
    const int textTop = content.top() + (content.height() - (lineHeight * 2 + kLineSpacing)) / 2;
    const QRect line1(textLeft, textTop, content.right() - textLeft + 1, lineHeight);
    const QRect line2 = line1.translated(0, lineHeight + kLineSpacing);

    // 1 行目: 右端に時刻、残りにタイトル
    painter->setFont(opt.font);
    painter->setPen(mutedColor);
    const int timeWidth = opt.fontMetrics.horizontalAdvance(summary.timeText);
    painter->drawText(line1, Qt::AlignRight | Qt::AlignVCenter, summary.timeText);

    QFont titleFont = opt.font;
    titleFont.setBold(true);
    const QFontMetrics titleMetrics(titleFont);
    const int titleWidth = line1.width() - timeWidth - kSpacing;
    painter->setFont(titleFont);
    painter->setPen(textColor);
    painter->drawText(line1.adjusted(0, 0, -(timeWidth + kSpacing), 0),
                      Qt::AlignLeft | Qt::AlignVCenter,
                      titleMetrics.elidedText(summary.title, Qt::ElideRight, titleWidth));

    // 2 行目: 補足
    painter->setFont(opt.font);
    painter->setPen(mutedColor);
    painter->drawText(line2, Qt::AlignLeft | Qt::AlignVCenter,
                      opt.fontMetrics.elidedText(summary.subtitle, Qt::ElideRight, line2.width()));

    painter->restore();
}

QSize EventDelegate::sizeHint(const QStyleOptionViewItem &option, const QModelIndex &) const
{
    return {240, badgeSize(option.fontMetrics) + kMargin * 2};
}

} // namespace p2pquake::ui
