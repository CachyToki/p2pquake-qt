#include "ui/Presenter.h"

#include "core/AreaTable.h"
#include "core/Labels.h"

#include <QMap>
#include <QStringList>

#include <algorithm>
#include <cmath>

using namespace Qt::StringLiterals;

namespace p2pquake::ui {

namespace {

const QColor kNeutralBackground(0x8a, 0x8f, 0x98);
const QColor kEewBackground(0xd3, 0x2f, 0x2f);
const QColor kUserquakeBackground(0x2e, 0x7d, 0x32);
const QString kSeparator = u" ・ "_s;

// ---- 共通 -----------------------------------------------------------------

int tsunamiGradeRank(const QString &grade)
{
    if (grade == "MajorWarning"_L1) return 3;
    if (grade == "Warning"_L1)      return 2;
    if (grade == "Watch"_L1)        return 1;
    return 0;
}

QColor tsunamiGradeBackground(const QString &grade)
{
    switch (tsunamiGradeRank(grade)) {
    case 3:  return QColor(0xc8, 0x00, 0xff);
    case 2:  return QColor(0xff, 0x28, 0x00);
    case 1:  return QColor(0xfa, 0xf5, 0x00);
    default: return kNeutralBackground;
    }
}

QColor tsunamiGradeForeground(const QString &grade)
{
    return tsunamiGradeRank(grade) == 1 ? QColor(Qt::black) : QColor(Qt::white);
}

// 発表中で最も重い津波予報の種類。areas が空なら空文字列。
QString highestGrade(const Tsunami &tsunami)
{
    QString highest;
    for (const TsunamiArea &area : tsunami.areas) {
        if (highest.isEmpty() || tsunamiGradeRank(area.grade) > tsunamiGradeRank(highest))
            highest = area.grade;
    }
    return highest;
}

QString userquakeLevelLabel(double confidence)
{
    const int level = userquakeLevel(confidence);
    if (level > 0)
        return u"レベル%1"_s.arg(level);
    return u"信頼度 %1%"_s.arg(confidence * 100.0, 0, 'f', 1);
}

QString positionLabel(const Hypocenter &h)
{
    if (!h.hasPosition())
        return {};
    return u"%1%2度 %3%4度"_s
            .arg(h.latitude >= 0 ? u"北緯"_s : u"南緯"_s)
            .arg(std::abs(h.latitude), 0, 'f', 1)
            .arg(h.longitude >= 0 ? u"東経"_s : u"西経"_s)
            .arg(std::abs(h.longitude), 0, 'f', 1);
}

// "A、B ほか3" のように先頭 limit 件だけ並べる。
QString joinLimited(const QStringList &names, int limit)
{
    if (names.size() <= limit)
        return names.join(u"、"_s);
    return names.mid(0, limit).join(u"、"_s) + u" ほか%1"_s.arg(names.size() - limit);
}

// ---- HTML -----------------------------------------------------------------

QString esc(const QString &text)
{
    return text.toHtmlEscaped();
}

QString badgeHtml(const QString &text, const QColor &background, const QColor &foreground,
                  bool large = false)
{
    return u"<span style=\"background-color:%1; color:%2; font-weight:bold;%3\">&nbsp;%4&nbsp;</span>"_s
            .arg(background.name(), foreground.name(),
                 large ? u" font-size:x-large;"_s : QString(), esc(text));
}

QString scaleBadgeHtml(int scale, bool large = false)
{
    return badgeHtml(scaleLabel(scale), scaleBackground(scale), scaleForeground(scale), large);
}

class Table
{
public:
    void row(const QString &key, const QString &valueHtml)
    {
        if (valueHtml.isEmpty())
            return;
        m_rows += u"<tr><td style=\"color:#808890; padding-right:16px;\">%1</td><td>%2</td></tr>"_s
                .arg(esc(key), valueHtml);
    }

    QString html() const
    {
        return u"<table cellspacing=\"0\" cellpadding=\"3\">%1</table>"_s.arg(m_rows);
    }

private:
    QString m_rows;
};

QString heading(const QString &text)
{
    return u"<h2>%1</h2>"_s.arg(esc(text));
}

QString subheading(const QString &text)
{
    return u"<h3>%1</h3>"_s.arg(esc(text));
}

QString note(const QString &text)
{
    return u"<p style=\"color:#808890;\">%1</p>"_s.arg(esc(text));
}

QString hypocenterHtml(const Hypocenter &h)
{
    if (h.name.isEmpty())
        return {};
    const QString position = positionLabel(h);
    return position.isEmpty() ? esc(h.name) : u"%1（%2）"_s.arg(esc(h.name), position);
}

// ---- 地震情報 (551) -------------------------------------------------------

bool hasHypocenter(const Quake &q)
{
    return !q.hypocenter.name.isEmpty();
}

Summary summarizeQuake(const Quake &q)
{
    Summary s;
    s.category = Category::Quake;
    s.badge = scaleShortLabel(q.maxScale);
    s.badgeBackground = scaleBackground(q.maxScale);
    s.badgeForeground = scaleForeground(q.maxScale);
    s.title = hasHypocenter(q) ? q.hypocenter.name : quakeTypeLabel(q.issueType);
    s.timeText = formatShort(q.time.isValid() ? q.time : q.issueTime);

    QStringList parts;
    if (hasHypocenter(q)) {
        parts << quakeTypeLabel(q.issueType);
        if (q.hypocenter.magnitude >= 0)
            parts << magnitudeLabel(q.hypocenter.magnitude);
        if (q.hypocenter.depth >= 0)
            parts << u"深さ"_s + depthLabel(q.hypocenter.depth);
    } else if (!q.points.isEmpty()) {
        // 震度速報: 震源が未発表なので、最も揺れた地域を示す。
        QStringList areas;
        for (const QuakePoint &p : q.points) {
            if (p.scale == q.maxScale && !areas.contains(p.addr))
                areas << p.addr;
        }
        parts << joinLimited(areas, 2);
    }
    s.subtitle = parts.join(kSeparator);
    return s;
}

QString pointsHtml(const Quake &q)
{
    if (q.points.isEmpty())
        return {};

    // 震度の大きい順 → 都道府県 (発表順) → 観測点
    QMap<int, QList<QPair<QString, QStringList>>> byScale;
    for (const QuakePoint &p : q.points) {
        auto &prefs = byScale[p.scale];
        auto it = std::ranges::find_if(prefs, [&](const auto &entry) { return entry.first == p.pref; });
        if (it == prefs.end()) {
            prefs.append({p.pref, {}});
            it = prefs.end() - 1;
        }
        it->second << p.addr;
    }

    QString rows;
    for (auto it = byScale.constEnd(); it != byScale.constBegin();) {
        --it;
        QStringList lines;
        for (const auto &[pref, addrs] : it.value())
            lines << u"<b>%1</b>&nbsp; %2"_s.arg(esc(pref), esc(addrs.join(u"　"_s)));
        rows += u"<tr><td style=\"padding-right:12px;\">%1</td><td>%2</td></tr>"_s
                .arg(scaleBadgeHtml(it.key()), lines.join(u"<br>"_s));
    }

    const bool areas = std::ranges::all_of(q.points, &QuakePoint::isArea);
    return subheading(areas ? u"地域ごとの震度"_s : u"各地の震度"_s)
            + u"<table cellspacing=\"0\" cellpadding=\"5\">%1</table>"_s.arg(rows);
}

QString quakeHtml(const Quake &q)
{
    QString html = heading(quakeTypeLabel(q.issueType));
    if (q.maxScale != Scale::Unknown)
        html += u"<p>%1</p>"_s.arg(badgeHtml(u"最大"_s + scaleLabel(q.maxScale),
                                               scaleBackground(q.maxScale),
                                               scaleForeground(q.maxScale), true));

    Table table;
    if (q.time.isValid())
        table.row(u"発生日時"_s, esc(formatDateTime(q.time)) + u" ごろ"_s);
    table.row(u"震源"_s, hypocenterHtml(q.hypocenter));
    if (hasHypocenter(q)) {
        table.row(u"深さ"_s, esc(depthLabel(q.hypocenter.depth)));
        table.row(u"規模"_s, esc(magnitudeLabel(q.hypocenter.magnitude)));
    }
    // 震度速報の時点では津波の有無は発表されない (Unknown) ため、行ごと省く。
    if (q.domesticTsunami != "Unknown"_L1 && !q.domesticTsunami.isEmpty())
        table.row(u"津波（国内）"_s, esc(domesticTsunamiLabel(q.domesticTsunami)));
    if (q.foreignTsunami != "Unknown"_L1 && !q.foreignTsunami.isEmpty())
        table.row(u"津波（海外）"_s, esc(foreignTsunamiLabel(q.foreignTsunami)));
    table.row(u"発表"_s, esc(u"%1 %2"_s.arg(q.issueSource, formatDateTime(q.issueTime)).trimmed()));
    table.row(u"訂正"_s, esc(correctLabel(q.correct)));
    html += table.html();

    if (!q.freeFormComment.isEmpty())
        html += u"<p>%1</p>"_s.arg(esc(q.freeFormComment).replace(u'\n', u"<br>"_s));

    html += pointsHtml(q);
    return html;
}

// ---- 津波予報 (552) -------------------------------------------------------

Summary summarizeTsunami(const Tsunami &t)
{
    const QString grade = highestGrade(t);

    Summary s;
    s.category = Category::Tsunami;
    s.badge = u"津波"_s;
    s.badgeBackground = t.cancelled ? kNeutralBackground : tsunamiGradeBackground(grade);
    s.badgeForeground = t.cancelled ? QColor(Qt::white) : tsunamiGradeForeground(grade);
    s.timeText = formatShort(t.issueTime);

    if (t.cancelled || t.areas.isEmpty()) {
        s.title = u"津波予報 解除"_s;
        s.subtitle = u"津波予報はすべて解除されました"_s;
        return s;
    }

    s.title = tsunamiGradeLabel(grade);
    QStringList names;
    for (const TsunamiArea &area : t.areas) {
        if (area.grade == grade)
            names << area.name;
    }
    s.subtitle = joinLimited(names, 2);
    return s;
}

QString tsunamiAreaDetail(const TsunamiArea &area)
{
    QStringList parts;
    if (!area.maxHeightDescription.isEmpty())
        parts << u"予想高さ %1"_s.arg(area.maxHeightDescription);
    if (area.arrivalTime.isValid())
        parts << u"第1波 %1"_s.arg(formatShort(area.arrivalTime));
    if (!area.condition.isEmpty())
        parts << area.condition;
    else if (area.immediate)
        parts << u"直ちに来襲と予想"_s;
    return parts.join(kSeparator);
}

QString tsunamiHtml(const Tsunami &t)
{
    QString html = heading(u"津波予報"_s);

    Table table;
    table.row(u"発表"_s, esc(u"%1 %2"_s.arg(t.issueSource, formatDateTime(t.issueTime)).trimmed()));
    html += table.html();

    if (t.cancelled || t.areas.isEmpty()) {
        html += u"<p><b>津波予報はすべて解除されました。</b></p>"_s;
        return html;
    }

    QList<TsunamiArea> areas = t.areas;
    std::ranges::stable_sort(areas, [](const TsunamiArea &a, const TsunamiArea &b) {
        return tsunamiGradeRank(a.grade) > tsunamiGradeRank(b.grade);
    });

    QString rows;
    for (const TsunamiArea &area : areas) {
        rows += u"<tr><td>%1</td><td style=\"padding-left:8px;\"><b>%2</b></td>"
                "<td style=\"padding-left:12px;\">%3</td></tr>"_s
                .arg(badgeHtml(tsunamiGradeLabel(area.grade), tsunamiGradeBackground(area.grade),
                               tsunamiGradeForeground(area.grade)),
                     esc(area.name), esc(tsunamiAreaDetail(area)));
    }
    html += subheading(u"津波予報区"_s)
            + u"<table cellspacing=\"0\" cellpadding=\"5\">%1</table>"_s.arg(rows);
    return html;
}

// ---- 緊急地震速報 (554 / 556) ---------------------------------------------

Summary summarizeEew(const Eew &e)
{
    Summary s;
    s.category = Category::Eew;
    s.badge = u"EEW"_s;
    s.badgeBackground = e.cancelled ? kNeutralBackground : kEewBackground;
    s.badgeForeground = Qt::white;
    s.title = e.cancelled ? u"緊急地震速報（警報）取消"_s : u"緊急地震速報（警報）"_s;
    if (e.test)
        s.title += u"【テスト】"_s;
    s.timeText = formatShort(e.issueTime);

    QStringList parts;
    if (e.hasEarthquake && !e.hypocenter.name.isEmpty())
        parts << e.hypocenter.name;
    if (e.hasEarthquake && e.hypocenter.magnitude >= 0)
        parts << magnitudeLabel(e.hypocenter.magnitude);
    if (!e.serial.isEmpty())
        parts << u"第%1報"_s.arg(e.serial);
    s.subtitle = parts.join(kSeparator);
    return s;
}

QString eewHtml(const Eew &e)
{
    QString html = heading(e.cancelled ? u"緊急地震速報（警報）取消"_s : u"緊急地震速報（警報）"_s);
    if (e.test)
        html += u"<p><b>これはテスト配信です。</b></p>"_s;
    if (e.cancelled)
        html += u"<p><b>先ほどの緊急地震速報は取り消されました。</b></p>"_s;

    Table table;
    table.row(u"発表"_s, esc(formatDateTimeSeconds(e.issueTime))
                                 + (e.serial.isEmpty() ? QString() : u"（第%1報）"_s.arg(esc(e.serial))));
    if (e.hasEarthquake) {
        table.row(u"発生時刻"_s, esc(formatDateTimeSeconds(e.originTime)));
        table.row(u"震央"_s, hypocenterHtml(e.hypocenter));
        table.row(u"深さ"_s, esc(depthLabel(e.hypocenter.depth)));
        table.row(u"規模"_s, esc(magnitudeLabel(e.hypocenter.magnitude)));
        table.row(u"備考"_s, esc(e.condition));
    }
    html += table.html();

    if (!e.areas.isEmpty()) {
        // 予測震度の大きい地域から並べる。scaleTo の 99 は「～程度以上」なので下限で比べる。
        const auto strongest = [](const EewArea &a) {
            return a.scaleTo == Scale::OrMore ? a.scaleFrom : std::max(a.scaleFrom, a.scaleTo);
        };
        QList<EewArea> areas = e.areas;
        std::ranges::stable_sort(areas, [&](const EewArea &a, const EewArea &b) {
            return strongest(a) > strongest(b);
        });

        QString rows;
        for (const EewArea &area : areas) {
            const int colorScale = area.scaleFrom != Scale::Unknown ? area.scaleFrom : area.scaleTo;
            QStringList detail;
            if (const QString kind = eewKindLabel(area.kindCode); !kind.isEmpty())
                detail << kind;
            if (area.arrivalTime.isValid())
                detail << u"到達予測 %1"_s.arg(formatTime(area.arrivalTime));
            rows += u"<tr><td>%1</td><td style=\"padding-left:8px;\"><b>%2</b></td>"
                    "<td style=\"padding-left:12px;\">%3</td></tr>"_s
                    .arg(badgeHtml(scaleRangeLabel(area.scaleFrom, area.scaleTo),
                                   scaleBackground(colorScale), scaleForeground(colorScale)),
                         esc(area.name), esc(detail.join(kSeparator)));
        }
        html += subheading(u"強い揺れが予想される地域"_s)
                + u"<table cellspacing=\"0\" cellpadding=\"5\">%1</table>"_s.arg(rows);
    }

    html += note(u"P2P地震情報 API が配信する緊急地震速報（警報）は内容・配信品質とも無保証です。"
                 "身を守るための情報源としては使用しないでください。"_s);
    return html;
}

Summary summarizeEewDetection(const Event &event)
{
    Summary s;
    s.category = Category::Eew;
    s.badge = u"EEW"_s;
    s.badgeBackground = QColor(0xef, 0x6c, 0x00);
    s.badgeForeground = Qt::white;
    s.title = u"緊急地震速報 発表検出"_s;
    s.subtitle = u"緊急地震速報の発表を検出しました"_s;
    s.timeText = formatShort(event.time);
    return s;
}

QString eewDetectionHtml(const Event &event, const EewDetection &d)
{
    Table table;
    table.row(u"検出日時"_s, esc(formatDateTimeSeconds(event.time)));
    table.row(u"検出種類"_s, d.type == "Full"_L1 ? u"チャイム＋音声"_s : esc(d.type));
    return heading(u"緊急地震速報 発表検出"_s) + table.html()
            + note(u"放送などから緊急地震速報の発表を検出した際に配信される情報です。"
                   "震源や予測震度は含まれません。"_s);
}

// ---- 地震感知情報 解析結果 (9611) ------------------------------------------

QStringList confidentAreaNames(const UserquakeEvaluation &ev)
{
    // 信頼度順に並んでいる。表示が E・F (信頼度が低い) の地域は除く。
    QStringList names;
    for (const AreaConfidence &area : ev.areaConfidences) {
        if (area.confidence >= 0.2)
            names << areaName(area.area);
    }
    return names;
}

Summary summarizeUserquake(const UserquakeEvaluation &ev)
{
    Summary s;
    s.category = Category::Userquake;
    s.badge = u"感知"_s;
    s.badgeBackground = kUserquakeBackground;
    s.badgeForeground = Qt::white;
    s.title = u"地震感知情報 "_s + userquakeLevelLabel(ev.confidence);
    s.timeText = formatShort(ev.startedAt);

    QStringList parts{u"%1件"_s.arg(ev.count)};
    if (const QStringList names = confidentAreaNames(ev); !names.isEmpty())
        parts << joinLimited(names, 2);
    s.subtitle = parts.join(kSeparator);
    return s;
}

QString userquakeHtml(const UserquakeEvaluation &ev)
{
    QString html = heading(u"地震感知情報"_s);
    html += u"<p>%1</p>"_s.arg(badgeHtml(userquakeLevelLabel(ev.confidence), kUserquakeBackground,
                                           Qt::white, true));

    Table table;
    table.row(u"開始"_s, esc(formatDateTimeSeconds(ev.startedAt)));
    table.row(u"最終更新"_s, esc(formatDateTimeSeconds(ev.updatedAt)));
    table.row(u"感知件数"_s, u"%1件"_s.arg(ev.count));
    html += table.html();

    if (!ev.areaConfidences.isEmpty()) {
        QString rows;
        for (const AreaConfidence &area : ev.areaConfidences) {
            rows += u"<tr><td><b>%1</b></td><td style=\"padding-left:8px;\">%2</td>"
                    "<td align=\"right\" style=\"padding-left:16px;\">%3件</td></tr>"_s
                    .arg(esc(area.display.isEmpty() ? u"-"_s : area.display),
                         esc(areaName(area.area)))
                    .arg(area.count);
        }
        html += subheading(u"地域ごとの信頼度"_s)
                + u"<table cellspacing=\"0\" cellpadding=\"4\">%1</table>"_s.arg(rows);
    }

    html += note(u"P2P地震情報のユーザーが「揺れた」と報告した情報の集計です。"
                 "気象庁の発表ではなく、誤報を含むことがあります。信頼度は A が最も高く F が最も低い値です。"_s);
    return html;
}

} // namespace

// ---- 公開関数 -------------------------------------------------------------

QColor scaleBackground(int scale)
{
    switch (scale) {
    case Scale::S0:            return QColor(0xd8, 0xdc, 0xe0);
    case Scale::S1:            return QColor(0xf2, 0xf2, 0xff);
    case Scale::S2:            return QColor(0x00, 0xaa, 0xff);
    case Scale::S3:            return QColor(0x00, 0x41, 0xff);
    case Scale::S4:            return QColor(0xfa, 0xe6, 0x96);
    case Scale::S5Lower:
    case Scale::S5LowerOrMore: return QColor(0xff, 0xe6, 0x00);
    case Scale::S5Upper:       return QColor(0xff, 0x99, 0x00);
    case Scale::S6Lower:       return QColor(0xff, 0x28, 0x00);
    case Scale::S6Upper:       return QColor(0xa5, 0x00, 0x21);
    case Scale::S7:            return QColor(0xb4, 0x00, 0x68);
    default:                   return kNeutralBackground;
    }
}

QColor scaleForeground(int scale)
{
    switch (scale) {
    case Scale::S0:
    case Scale::S1:
    case Scale::S2:
    case Scale::S4:
    case Scale::S5Lower:
    case Scale::S5LowerOrMore:
    case Scale::S5Upper:
        return Qt::black;
    default:
        return Qt::white;
    }
}

bool isListed(const Event &event)
{
    switch (event.code) {
    case Code::Quake:
    case Code::Tsunami:
    case Code::EewDetection:
    case Code::Eew:
        return true;
    case Code::UserquakeEvaluation: {
        const auto *ev = event.as<UserquakeEvaluation>();
        return ev && ev->confidence > 0 && ev->startedAt.isValid();
    }
    default:
        return false;
    }
}

QDateTime sortTime(const Event &event)
{
    // 解析結果は更新のたびに受信日時が変わるため、イベントの開始日時で並べる。
    if (const auto *ev = event.as<UserquakeEvaluation>(); ev && ev->startedAt.isValid())
        return ev->startedAt;
    return event.time;
}

Summary summarize(const Event &event)
{
    if (const auto *q = event.as<Quake>())
        return summarizeQuake(*q);
    if (const auto *t = event.as<Tsunami>())
        return summarizeTsunami(*t);
    if (const auto *e = event.as<Eew>())
        return summarizeEew(*e);
    if (event.as<EewDetection>())
        return summarizeEewDetection(event);
    if (const auto *ev = event.as<UserquakeEvaluation>())
        return summarizeUserquake(*ev);
    return {};
}

QString detailHtml(const Event &event)
{
    if (const auto *q = event.as<Quake>())
        return quakeHtml(*q);
    if (const auto *t = event.as<Tsunami>())
        return tsunamiHtml(*t);
    if (const auto *e = event.as<Eew>())
        return eewHtml(*e);
    if (const auto *d = event.as<EewDetection>())
        return eewDetectionHtml(event, *d);
    if (const auto *ev = event.as<UserquakeEvaluation>())
        return userquakeHtml(*ev);
    return {};
}

std::optional<Notification> notificationFor(const Event &event, const NotifySettings &settings)
{
    if (!settings.enabled())
        return std::nullopt;

    if (const auto *q = event.as<Quake>()) {
        if (q->maxScale == Scale::Unknown) {
            if (settings.minScale != NotifySettings::AllQuakes)
                return std::nullopt;
        } else if (q->maxScale < settings.minScale) {
            return std::nullopt;
        }

        Notification n;
        n.critical = q->maxScale >= Scale::S5Lower;
        QStringList title{quakeTypeLabel(q->issueType)};
        if (q->maxScale != Scale::Unknown)
            title << u"最大"_s + scaleLabel(q->maxScale);
        n.title = title.join(u" "_s);

        QStringList lines;
        QStringList first;
        if (q->time.isValid())
            first << formatShort(q->time) + u"ごろ"_s;
        if (hasHypocenter(*q)) {
            first << q->hypocenter.name;
            if (q->hypocenter.magnitude >= 0)
                first << magnitudeLabel(q->hypocenter.magnitude);
            if (q->hypocenter.depth >= 0)
                first << u"深さ"_s + depthLabel(q->hypocenter.depth);
        }
        if (!first.isEmpty())
            lines << first.join(u" "_s);
        if (q->domesticTsunami != "Unknown"_L1 && !q->domesticTsunami.isEmpty())
            lines << domesticTsunamiLabel(q->domesticTsunami);
        n.body = lines.join(u'\n');
        return n;
    }

    if (const auto *t = event.as<Tsunami>()) {
        const Summary s = summarizeTsunami(*t);
        return Notification{s.title, s.subtitle, !t->cancelled};
    }

    if (const auto *e = event.as<Eew>()) {
        const Summary s = summarizeEew(*e);
        QStringList names;
        for (const EewArea &area : e->areas)
            names << area.name;
        QStringList lines;
        if (!s.subtitle.isEmpty())
            lines << s.subtitle;
        if (!names.isEmpty())
            lines << u"強い揺れに警戒: "_s + joinLimited(names, 4);
        return Notification{s.title, lines.join(u'\n'), !e->cancelled};
    }

    if (event.as<EewDetection>()) {
        const Summary s = summarizeEewDetection(event);
        return Notification{s.title, s.subtitle, true};
    }

    if (const auto *ev = event.as<UserquakeEvaluation>()) {
        if (!settings.userquake || !isListed(event))
            return std::nullopt;
        const Summary s = summarizeUserquake(*ev);
        return Notification{s.title, s.subtitle, false};
    }

    return std::nullopt;
}

} // namespace p2pquake::ui
