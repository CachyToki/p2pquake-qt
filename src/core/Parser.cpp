#include "core/Parser.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QTimeZone>

#include <algorithm>

using namespace Qt::StringLiterals;

namespace p2pquake {

namespace {

const QTimeZone &jst()
{
    // 気象庁・P2P地震情報の日時はすべて日本時間。tzdata に依存しないよう固定オフセットを使う。
    static const QTimeZone zone(9 * 3600);
    return zone;
}

QDateTime timeOf(const QJsonValue &value)
{
    return value.isString() ? parseTime(value.toString()) : QDateTime();
}

// 「システムの都合で小数点が付く」整数値を含め、数値を int として読む。
int intOf(const QJsonValue &value, int fallback)
{
    return value.isDouble() ? static_cast<int>(value.toDouble()) : fallback;
}

Hypocenter hypocenterOf(const QJsonObject &o)
{
    Hypocenter h;
    h.name = o["name"_L1].toString();
    h.reduceName = o["reduceName"_L1].toString();
    h.latitude = o["latitude"_L1].toDouble(-200.0);
    h.longitude = o["longitude"_L1].toDouble(-200.0);
    h.depth = intOf(o["depth"_L1], -1);
    h.magnitude = o["magnitude"_L1].toDouble(-1.0);
    return h;
}

std::optional<EventBody> quakeOf(const QJsonObject &o)
{
    if (!o["issue"_L1].isObject() || !o["earthquake"_L1].isObject())
        return std::nullopt;

    const QJsonObject issue = o["issue"_L1].toObject();
    const QJsonObject eq = o["earthquake"_L1].toObject();

    Quake q;
    q.issueSource = issue["source"_L1].toString();
    q.issueTime = timeOf(issue["time"_L1]);
    q.issueType = issue["type"_L1].toString();
    q.correct = issue["correct"_L1].toString();
    q.time = timeOf(eq["time"_L1]);
    q.hypocenter = hypocenterOf(eq["hypocenter"_L1].toObject());
    q.maxScale = intOf(eq["maxScale"_L1], Scale::Unknown);
    q.domesticTsunami = eq["domesticTsunami"_L1].toString();
    q.foreignTsunami = eq["foreignTsunami"_L1].toString();
    q.freeFormComment = o["comments"_L1].toObject()["freeFormComment"_L1].toString();

    const QJsonArray points = o["points"_L1].toArray();
    q.points.reserve(points.size());
    for (const QJsonValue &v : points) {
        const QJsonObject p = v.toObject();
        q.points.append({p["pref"_L1].toString(), p["addr"_L1].toString(),
                         p["isArea"_L1].toBool(), intOf(p["scale"_L1], Scale::Unknown)});
    }
    return q;
}

std::optional<EventBody> tsunamiOf(const QJsonObject &o)
{
    if (!o["issue"_L1].isObject())
        return std::nullopt;

    const QJsonObject issue = o["issue"_L1].toObject();

    Tsunami t;
    t.cancelled = o["cancelled"_L1].toBool();
    t.issueSource = issue["source"_L1].toString();
    t.issueTime = timeOf(issue["time"_L1]);
    t.issueType = issue["type"_L1].toString();

    const QJsonArray areas = o["areas"_L1].toArray();
    t.areas.reserve(areas.size());
    for (const QJsonValue &v : areas) {
        const QJsonObject a = v.toObject();
        const QJsonObject first = a["firstHeight"_L1].toObject();
        const QJsonObject max = a["maxHeight"_L1].toObject();

        TsunamiArea area;
        area.grade = a["grade"_L1].toString();
        area.immediate = a["immediate"_L1].toBool();
        area.name = a["name"_L1].toString();
        area.arrivalTime = timeOf(first["arrivalTime"_L1]);
        area.condition = first["condition"_L1].toString();
        area.maxHeightDescription = max["description"_L1].toString();
        area.maxHeightValue = max["value"_L1].toDouble(-1.0);
        t.areas.append(area);
    }
    return t;
}

std::optional<EventBody> eewOf(const QJsonObject &o)
{
    if (!o["issue"_L1].isObject())
        return std::nullopt;

    const QJsonObject issue = o["issue"_L1].toObject();

    Eew e;
    e.test = o["test"_L1].toBool();
    e.cancelled = o["cancelled"_L1].toBool();
    e.issueTime = timeOf(issue["time"_L1]);
    e.eventId = issue["eventId"_L1].toString();
    e.serial = issue["serial"_L1].toString();

    if (o["earthquake"_L1].isObject()) {
        const QJsonObject eq = o["earthquake"_L1].toObject();
        e.hasEarthquake = true;
        e.originTime = timeOf(eq["originTime"_L1]);
        e.arrivalTime = timeOf(eq["arrivalTime"_L1]);
        e.condition = eq["condition"_L1].toString();
        e.hypocenter = hypocenterOf(eq["hypocenter"_L1].toObject());
    }

    const QJsonArray areas = o["areas"_L1].toArray();
    e.areas.reserve(areas.size());
    for (const QJsonValue &v : areas) {
        const QJsonObject a = v.toObject();
        e.areas.append({a["pref"_L1].toString(), a["name"_L1].toString(),
                        intOf(a["scaleFrom"_L1], Scale::Unknown),
                        intOf(a["scaleTo"_L1], Scale::Unknown), a["kindCode"_L1].toString(),
                        timeOf(a["arrivalTime"_L1])});
    }
    return e;
}

std::optional<EventBody> areapeersOf(const QJsonObject &o)
{
    if (!o["areas"_L1].isArray())
        return std::nullopt;

    Areapeers peers;
    const QJsonArray areas = o["areas"_L1].toArray();
    peers.areas.reserve(areas.size());
    for (const QJsonValue &v : areas) {
        const QJsonObject a = v.toObject();
        peers.areas.append({intOf(a["id"_L1], 0), intOf(a["peer"_L1], 0)});
    }
    return peers;
}

std::optional<EventBody> userquakeEvaluationOf(const QJsonObject &o)
{
    if (!o["count"_L1].isDouble() || !o["confidence"_L1].isDouble())
        return std::nullopt;

    UserquakeEvaluation ev;
    ev.count = intOf(o["count"_L1], 0);
    ev.confidence = o["confidence"_L1].toDouble();
    ev.startedAt = timeOf(o["started_at"_L1]);
    ev.updatedAt = timeOf(o["updated_at"_L1]);

    const QJsonObject confidences = o["area_confidences"_L1].toObject();
    ev.areaConfidences.reserve(confidences.size());
    for (auto it = confidences.begin(); it != confidences.end(); ++it) {
        bool ok = false;
        const int area = it.key().toInt(&ok);
        if (!ok)
            continue;
        const QJsonObject c = it.value().toObject();
        ev.areaConfidences.append({area, c["confidence"_L1].toDouble(), intOf(c["count"_L1], 0),
                                   c["display"_L1].toString()});
    }
    // 信頼度の高い地域から順に並べる。
    std::ranges::stable_sort(ev.areaConfidences, [](const AreaConfidence &a, const AreaConfidence &b) {
        return a.confidence > b.confidence;
    });
    return ev;
}

std::optional<EventBody> bodyOf(int code, const QJsonObject &o)
{
    switch (code) {
    case Code::Quake:
        return quakeOf(o);
    case Code::Tsunami:
        return tsunamiOf(o);
    case Code::EewDetection:
        return EewDetection{o["type"_L1].toString()};
    case Code::Areapeers:
        return areapeersOf(o);
    case Code::Eew:
        return eewOf(o);
    case Code::Userquake:
        if (!o["area"_L1].isDouble())
            return std::nullopt;
        return Userquake{intOf(o["area"_L1], 0)};
    case Code::UserquakeEvaluation:
        return userquakeEvaluationOf(o);
    default:
        return std::nullopt;
    }
}

} // namespace

QDateTime parseTime(const QString &text)
{
    // "yyyy/MM/dd HH:mm:ss" + 省略可能な ".f" ～ ".fff…"
    const qsizetype dot = text.indexOf(u'.');
    const QString base = dot < 0 ? text : text.left(dot);

    // ローカルタイムゾーンを経由すると夏時間の切り替え時刻で狂うため、日付と時刻を別々に読む。
    if (base.size() != 19)
        return {};
    const QDate date = QDate::fromString(base.left(10), u"yyyy/MM/dd"_s);
    const QTime time = QTime::fromString(base.mid(11), u"HH:mm:ss"_s);
    if (!date.isValid() || !time.isValid())
        return {};
    QDateTime dt(date, time, jst());

    if (dot >= 0) {
        const QString fraction = text.mid(dot + 1, 3).leftJustified(3, u'0');
        bool ok = false;
        const int msec = fraction.toInt(&ok);
        if (!ok || msec < 0)
            return {};
        dt = dt.addMSecs(msec);
    }
    return dt;
}

std::optional<Event> parseEvent(const QJsonObject &object)
{
    if (!object["code"_L1].isDouble())
        return std::nullopt;

    Event event;
    event.code = object["code"_L1].toInt();
    // 仕様書では "id" だが、WebSocket API は "_id" というキーで配信してくる (2026/11 時点)。
    event.id = object["id"_L1].toString();
    if (event.id.isEmpty())
        event.id = object["_id"_L1].toString();
    event.time = timeOf(object["time"_L1]);
    if (event.id.isEmpty() || !event.time.isValid())
        return std::nullopt;

    std::optional<EventBody> body = bodyOf(event.code, object);
    if (!body)
        return std::nullopt;
    event.body = std::move(*body);
    return event;
}

std::optional<Event> parseEvent(const QByteArray &json)
{
    const QJsonDocument doc = QJsonDocument::fromJson(json);
    if (!doc.isObject())
        return std::nullopt;
    return parseEvent(doc.object());
}

QList<Event> parseHistory(const QByteArray &json, QString *error)
{
    QJsonParseError parseError;
    const QJsonDocument doc = QJsonDocument::fromJson(json, &parseError);
    if (!doc.isArray()) {
        if (error) {
            *error = parseError.error != QJsonParseError::NoError
                    ? parseError.errorString()
                    : u"JSON 配列ではありません"_s;
        }
        return {};
    }

    QList<Event> events;
    const QJsonArray array = doc.array();
    events.reserve(array.size());
    for (const QJsonValue &v : array) {
        if (std::optional<Event> event = parseEvent(v.toObject()))
            events.append(std::move(*event));
    }
    return events;
}

} // namespace p2pquake
