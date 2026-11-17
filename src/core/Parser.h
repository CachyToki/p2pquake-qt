#pragma once

#include "core/Types.h"

#include <QByteArray>
#include <QJsonObject>

#include <optional>

namespace p2pquake {

// API の日時文字列 ("2006/01/02 15:04:05.999"、小数部は省略可) を日本時間として解釈する。
// 解釈できない場合は invalid な QDateTime を返す。
QDateTime parseTime(const QString &text);

// JSON オブジェクト 1 件を解釈する。未知の情報コードや必須項目の欠落は nullopt。
std::optional<Event> parseEvent(const QJsonObject &object);

// WebSocket の 1 フレーム (JSON オブジェクト) を解釈する。
std::optional<Event> parseEvent(const QByteArray &json);

// /history のレスポンス (JSON 配列) を解釈する。解釈できない要素は読み飛ばす。
// JSON として不正な場合は error にメッセージを入れて空リストを返す。
QList<Event> parseHistory(const QByteArray &json, QString *error = nullptr);

} // namespace p2pquake
