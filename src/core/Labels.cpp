#include "core/Labels.h"

#include <QTimeZone>

#include <cmath>

using namespace Qt::StringLiterals;

namespace p2pquake {

namespace {

QDateTime toJst(const QDateTime &time)
{
    static const QTimeZone zone(9 * 3600);
    return time.toTimeZone(zone);
}

QString format(const QDateTime &time, const QString &pattern)
{
    return time.isValid() ? toJst(time).toString(pattern) : u"不明"_s;
}

} // namespace

QString scaleShortLabel(int scale)
{
    switch (scale) {
    case Scale::S0:            return u"0"_s;
    case Scale::S1:            return u"1"_s;
    case Scale::S2:            return u"2"_s;
    case Scale::S3:            return u"3"_s;
    case Scale::S4:            return u"4"_s;
    case Scale::S5Lower:       return u"5弱"_s;
    case Scale::S5LowerOrMore: return u"5弱以上"_s;
    case Scale::S5Upper:       return u"5強"_s;
    case Scale::S6Lower:       return u"6弱"_s;
    case Scale::S6Upper:       return u"6強"_s;
    case Scale::S7:            return u"7"_s;
    default:                   return u"-"_s;
    }
}

QString scaleLabel(int scale)
{
    if (scale == Scale::S5LowerOrMore)
        return u"震度5弱以上（推定）"_s;
    const QString label = scaleShortLabel(scale);
    return label == u"-"_s ? u"震度不明"_s : u"震度"_s + label;
}

QString scaleRangeLabel(int from, int to)
{
    if (to == Scale::OrMore)
        return scaleLabel(from) + u"程度以上"_s;
    if (from == to || from == Scale::Unknown)
        return scaleLabel(to);
    if (to == Scale::Unknown)
        return scaleLabel(from);
    return scaleLabel(from) + u"～"_s + scaleShortLabel(to);
}

QString quakeTypeLabel(const QString &issueType)
{
    if (issueType == "ScalePrompt"_L1)         return u"震度速報"_s;
    if (issueType == "Destination"_L1)         return u"震源に関する情報"_s;
    if (issueType == "ScaleAndDestination"_L1) return u"震度・震源に関する情報"_s;
    if (issueType == "DetailScale"_L1)         return u"各地の震度に関する情報"_s;
    if (issueType == "Foreign"_L1)             return u"遠地地震に関する情報"_s;
    return u"その他の地震情報"_s;
}

QString correctLabel(const QString &correct)
{
    if (correct == "None"_L1 || correct.isEmpty())  return {};
    if (correct == "ScaleOnly"_L1)                  return u"震度を訂正"_s;
    if (correct == "DestinationOnly"_L1)            return u"震源を訂正"_s;
    if (correct == "ScaleAndDestination"_L1)        return u"震度・震源を訂正"_s;
    return u"訂正あり（内容不明）"_s;
}

QString domesticTsunamiLabel(const QString &value)
{
    if (value == "None"_L1)         return u"津波の心配なし"_s;
    if (value == "Checking"_L1)     return u"津波の有無を調査中"_s;
    if (value == "NonEffective"_L1) return u"若干の海面変動が予想されるが、被害の心配なし"_s;
    if (value == "Watch"_L1)        return u"津波注意報を発表中"_s;
    if (value == "Warning"_L1)      return u"津波予報（種類不明）を発表中"_s;
    return u"不明"_s;
}

QString foreignTsunamiLabel(const QString &value)
{
    if (value == "None"_L1)               return u"津波の心配なし"_s;
    if (value == "Checking"_L1)           return u"津波の有無を調査中"_s;
    if (value == "NonEffectiveNearby"_L1) return u"震源の近傍で小さな津波の可能性があるが、被害の心配なし"_s;
    if (value == "WarningNearby"_L1)      return u"震源の近傍で津波の可能性がある"_s;
    if (value == "WarningPacific"_L1)     return u"太平洋で津波の可能性がある"_s;
    if (value == "WarningPacificWide"_L1) return u"太平洋の広域で津波の可能性がある"_s;
    if (value == "WarningIndian"_L1)      return u"インド洋で津波の可能性がある"_s;
    if (value == "WarningIndianWide"_L1)  return u"インド洋の広域で津波の可能性がある"_s;
    if (value == "Potential"_L1)          return u"一般にこの規模では津波の可能性がある"_s;
    return u"不明"_s;
}

QString tsunamiGradeLabel(const QString &grade)
{
    if (grade == "MajorWarning"_L1) return u"大津波警報"_s;
    if (grade == "Warning"_L1)      return u"津波警報"_s;
    if (grade == "Watch"_L1)        return u"津波注意報"_s;
    return u"津波予報（種類不明）"_s;
}

QString eewKindLabel(const QString &kindCode)
{
    if (kindCode == "10"_L1) return u"主要動は未到達と予測"_s;
    if (kindCode == "11"_L1) return u"主要動は既に到達と予測"_s;
    if (kindCode == "19"_L1) return u"到達予想なし（PLUM法）"_s;
    return {};
}

QString depthLabel(int depth)
{
    if (depth < 0)
        return u"不明"_s;
    if (depth == 0)
        return u"ごく浅い"_s;
    return u"約%1km"_s.arg(depth);
}

QString magnitudeLabel(double magnitude)
{
    if (magnitude < 0)
        return u"M不明"_s;
    return u"M%1"_s.arg(magnitude, 0, 'f', 1);
}

int userquakeLevel(double confidence)
{
    // 仕様書の定義: 0.97015=レベル1、0.96774=レベル2、0.97024=レベル3、0.98052=レベル4
    static constexpr struct { double value; int level; } table[] = {
        {0.97015, 1}, {0.96774, 2}, {0.97024, 3}, {0.98052, 4},
    };
    for (const auto &entry : table) {
        if (std::abs(confidence - entry.value) < 0.000005)
            return entry.level;
    }
    return 0;
}

QString formatDateTime(const QDateTime &time)
{
    return format(time, u"yyyy/MM/dd HH:mm"_s);
}

QString formatDateTimeSeconds(const QDateTime &time)
{
    return format(time, u"yyyy/MM/dd HH:mm:ss"_s);
}

QString formatShort(const QDateTime &time)
{
    return format(time, u"MM/dd HH:mm"_s);
}

QString formatTime(const QDateTime &time)
{
    return format(time, u"HH:mm:ss"_s);
}

} // namespace p2pquake
