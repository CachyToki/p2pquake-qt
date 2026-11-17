// API の値を日本語の表示文字列に変換する。
#pragma once

#include "core/Types.h"

namespace p2pquake {

// "5弱" のような短い表記。不明は "-"。
QString scaleShortLabel(int scale);
// "震度5弱" のような表記。不明は "震度不明"。
QString scaleLabel(int scale);
// EEW の予測震度の幅。"震度5弱"、"震度4～5弱"、"震度5強程度以上" など。
QString scaleRangeLabel(int from, int to);

QString quakeTypeLabel(const QString &issueType);
QString correctLabel(const QString &correct);
QString domesticTsunamiLabel(const QString &value);
QString foreignTsunamiLabel(const QString &value);
QString tsunamiGradeLabel(const QString &grade);
QString eewKindLabel(const QString &kindCode);

// "ごく浅い" / "約10km" / "不明"
QString depthLabel(int depth);
// "M5.5" / "M不明"
QString magnitudeLabel(double magnitude);

// 地震感知情報の信頼度から Beta3 の表示レベル (1～4) を返す。非表示は 0。
int userquakeLevel(double confidence);

// 日本時間での表示。
QString formatDateTime(const QDateTime &time); // "2026/11/16 13:28"
QString formatDateTimeSeconds(const QDateTime &time); // "2026/11/16 13:28:00"
QString formatShort(const QDateTime &time);    // "11/16 13:28"
QString formatTime(const QDateTime &time);     // "13:28:00"

} // namespace p2pquake
