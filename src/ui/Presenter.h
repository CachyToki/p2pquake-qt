// 受信した情報を、一覧・詳細・通知の表示内容に変換する。
#pragma once

#include "core/Types.h"

#include <QColor>
#include <QMetaType>

#include <optional>

namespace p2pquake::ui {

enum class Category { Quake, Tsunami, Eew, Userquake };

// 一覧 1 行分の表示内容
struct Summary {
    Category category = Category::Quake;
    QString badge; // "5弱" / "津波" / "EEW" / "感知"
    QColor badgeBackground;
    QColor badgeForeground;
    QString title;
    QString subtitle;
    QString timeText;
};

struct Notification {
    QString title;
    QString body;
    bool critical = false;
};

struct NotifySettings {
    // 通知する地震情報の最小震度。AllQuakes は震度不明 (震源に関する情報など) も含めて通知する。
    enum : int { AllQuakes = 0, Disabled = 1000 };
    int minScale = Scale::S3;
    bool userquake = false; // 地震感知情報も通知する

    bool enabled() const { return minScale != Disabled; }
};

QColor scaleBackground(int scale);
QColor scaleForeground(int scale);

// 一覧に表示する情報かどうか。ピア数や個々の地震感知情報、信頼度 0 の解析結果は表示しない。
bool isListed(const Event &event);
// 一覧での並び順に使う日時。
QDateTime sortTime(const Event &event);

Summary summarize(const Event &event);
QString detailHtml(const Event &event);
std::optional<Notification> notificationFor(const Event &event, const NotifySettings &settings);

} // namespace p2pquake::ui

Q_DECLARE_METATYPE(p2pquake::ui::Summary)
