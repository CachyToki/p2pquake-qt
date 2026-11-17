#pragma once

#include <QString>

namespace p2pquake {

// EPSP 地域コード (epsp-area.csv) から地域名を引く。未知のコードは "地域コード NNN"。
// 例: 10 -> "北海道 石狩"、250 -> "東京"
QString areaName(int code);

} // namespace p2pquake
