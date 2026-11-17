// フィクスチャは実際の API (2026/11/16 時点) とサンドボックスのレスポンスを保存したもの。
#include "core/AreaTable.h"
#include "core/Client.h"
#include "core/Labels.h"
#include "core/Parser.h"
#include "ui/EventListModel.h"
#include "ui/Presenter.h"

#include <QFile>
#include <QTest>
#include <QTimeZone>

using namespace Qt::StringLiterals;
using namespace p2pquake;
using namespace p2pquake::ui;

namespace {

QByteArray readFixture(const QString &name)
{
    QFile file(QStringLiteral(FIXTURE_DIR "/") + name);
    if (!file.open(QIODevice::ReadOnly))
        qFatal("フィクスチャを開けません: %s", qPrintable(file.fileName()));
    return file.readAll();
}

QList<Event> loadFixture(const QString &name)
{
    QString error;
    const QList<Event> events = parseHistory(readFixture(name), &error);
    if (!error.isEmpty())
        qFatal("フィクスチャを解析できません: %s", qPrintable(error));
    return events;
}

QDateTime jst(int y, int mo, int d, int h, int mi, int s, int ms = 0)
{
    return QDateTime(QDate(y, mo, d), QTime(h, mi, s, ms), QTimeZone(9 * 3600));
}

} // namespace

class TestP2PQuake : public QObject
{
    Q_OBJECT

private slots:
    // ---- 解析 ----
    void parseTime_data()
    {
        QTest::addColumn<QString>("text");
        QTest::addColumn<QDateTime>("expected");

        QTest::newRow("milliseconds") << u"2026/11/16 13:31:49.257"_s << jst(2026, 11, 16, 13, 31, 49, 257);
        QTest::newRow("no fraction") << u"2026/11/16 13:28:00"_s << jst(2026, 11, 16, 13, 28, 0);
        // サーバは末尾の 0 を省くことがある (".11" は 110 ミリ秒)
        QTest::newRow("two digits") << u"2026/11/16 19:16:19.11"_s << jst(2026, 11, 16, 19, 16, 19, 110);
        QTest::newRow("one digit") << u"2026/11/16 19:16:19.5"_s << jst(2026, 11, 16, 19, 16, 19, 500);
        QTest::newRow("empty") << QString() << QDateTime();
        QTest::newRow("garbage") << u"not a date"_s << QDateTime();
        QTest::newRow("bad fraction") << u"2026/11/16 19:16:19.xyz"_s << QDateTime();
    }

    void parseTime()
    {
        QFETCH(QString, text);
        QFETCH(QDateTime, expected);
        QCOMPARE(p2pquake::parseTime(text), expected);
    }

    void parseTimeIsJapanTime()
    {
        const QDateTime time = p2pquake::parseTime(u"2026/11/16 13:28:00"_s);
        QCOMPARE(time.toUTC(), QDateTime(QDate(2026, 11, 16), QTime(4, 28), QTimeZone::UTC));
        QCOMPARE(formatDateTime(time), u"2026/11/16 13:28"_s);
    }

    void parseQuake()
    {
        const QList<Event> events = loadFixture(u"quake.json"_s);
        QCOMPARE(events.size(), 3);

        const Event &event = events.at(0);
        QCOMPARE(event.code, int(Code::Quake));
        QCOMPARE(event.id, u"6afa873571973906912545f1"_s);
        QCOMPARE(event.time, jst(2026, 11, 16, 13, 31, 49, 257));

        const Quake *quake = event.as<Quake>();
        QVERIFY(quake);
        QCOMPARE(quake->issueSource, u"気象庁"_s);
        QCOMPARE(quake->issueType, u"DetailScale"_s);
        QCOMPARE(quake->correct, u"None"_s);
        QCOMPARE(quake->time, jst(2026, 11, 16, 13, 28, 0));
        QCOMPARE(quake->hypocenter.name, u"静岡県東部"_s);
        QCOMPARE(quake->hypocenter.latitude, 35.3);
        QCOMPARE(quake->hypocenter.longitude, 139.0);
        QCOMPARE(quake->hypocenter.depth, 0);
        QCOMPARE(quake->hypocenter.magnitude, 2.8);
        QCOMPARE(quake->maxScale, int(Scale::S1));
        QCOMPARE(quake->domesticTsunami, u"None"_s);
        QCOMPARE(quake->points.size(), 9);
        QCOMPARE(quake->points.at(0).pref, u"神奈川県"_s);
        QCOMPARE(quake->points.at(0).addr, u"小田原市久野"_s);
        QCOMPARE(quake->points.at(0).scale, int(Scale::S1));
        QVERIFY(!quake->points.at(0).isArea);

        QCOMPARE(events.at(1).as<Quake>()->hypocenter.depth, 280);
        QCOMPARE(events.at(1).as<Quake>()->points.size(), 32);
    }

    void parseTsunami()
    {
        const QList<Event> events = loadFixture(u"tsunami.json"_s);
        QCOMPARE(events.size(), 2);

        const Tsunami *cancelled = events.at(0).as<Tsunami>();
        QVERIFY(cancelled);
        QVERIFY(cancelled->cancelled);
        QVERIFY(cancelled->areas.isEmpty());
        QCOMPARE(cancelled->issueTime, jst(2023, 10, 9, 12, 0, 2));

        const Tsunami *tsunami = events.at(1).as<Tsunami>();
        QVERIFY(tsunami);
        QVERIFY(!tsunami->cancelled);
        QCOMPARE(tsunami->areas.size(), 9);
        const TsunamiArea &area = tsunami->areas.at(0);
        QCOMPARE(area.grade, u"Watch"_s);
        QCOMPARE(area.name, u"千葉県九十九里・外房"_s);
        QVERIFY(area.immediate);
        QCOMPARE(area.condition, u"津波到達中と推測"_s);
        QCOMPARE(area.maxHeightDescription, u"１ｍ"_s);
        QCOMPARE(area.maxHeightValue, 1.0);
        QVERIFY(!area.arrivalTime.isValid());
    }

    void parseEew()
    {
        const QList<Event> events = loadFixture(u"eew.json"_s);
        QCOMPARE(events.size(), 1);

        const Eew *eew = events.at(0).as<Eew>();
        QVERIFY(eew);
        QVERIFY(!eew->cancelled);
        QVERIFY(eew->hasEarthquake);
        QCOMPARE(eew->serial, u"1"_s);
        QCOMPARE(eew->eventId, u"20261108010441"_s);
        QCOMPARE(eew->hypocenter.name, u"愛知県東部"_s);
        QCOMPARE(eew->hypocenter.depth, 40);
        QCOMPARE(eew->hypocenter.magnitude, 5.5);
        QCOMPARE(eew->originTime, jst(2026, 11, 8, 1, 4, 35));
        QCOMPARE(eew->areas.size(), 5);
        QCOMPARE(eew->areas.at(0).name, u"愛知県西部"_s);
        QCOMPARE(eew->areas.at(0).scaleFrom, int(Scale::S5Lower));
        QCOMPARE(eew->areas.at(0).kindCode, u"19"_s);
        QCOMPARE(eew->areas.at(0).arrivalTime, jst(2026, 11, 8, 1, 4, 49));
        QVERIFY(!eew->areas.at(1).arrivalTime.isValid()); // JSON では null
        QCOMPARE(eew->areas.at(3).scaleFrom, int(Scale::S3));
        QCOMPARE(eew->areas.at(3).scaleTo, int(Scale::S4));
    }

    void parseCancelledEew()
    {
        const auto event = parseEvent(QByteArray(R"({"code":556,"id":"x","time":"2026/01/01 00:00:00",
            "cancelled":true,"issue":{"time":"2026/01/01 00:00:00","eventId":"e","serial":"2"}})"));
        QVERIFY(event);
        const Eew *eew = event->as<Eew>();
        QVERIFY(eew);
        QVERIFY(eew->cancelled);
        QVERIFY(!eew->hasEarthquake);
        QVERIFY(detailHtml(*event).contains(u"取り消されました"_s));
    }

    void parseOthers()
    {
        const QList<Event> detection = loadFixture(u"eew_detection.json"_s);
        QCOMPARE(detection.size(), 1);
        QCOMPARE(detection.at(0).as<EewDetection>()->type, u"Full"_s);

        const QList<Event> peers = loadFixture(u"areapeers.json"_s);
        QCOMPARE(peers.size(), 1);
        QCOMPARE(peers.at(0).as<Areapeers>()->areas.size(), 122);
        QCOMPARE(peers.at(0).as<Areapeers>()->total(), 2203);

        const QList<Event> userquake = loadFixture(u"userquake.json"_s);
        QCOMPARE(userquake.size(), 1);
        QCOMPARE(userquake.at(0).as<Userquake>()->area, 250);
    }

    void parseUserquakeEvaluation()
    {
        const QList<Event> events = loadFixture(u"userquake_evaluation.json"_s);
        QCOMPARE(events.size(), 3);

        const UserquakeEvaluation *latest = events.at(1).as<UserquakeEvaluation>();
        QVERIFY(latest);
        QCOMPARE(latest->count, 54);
        QCOMPARE(latest->confidence, 0.98052);
        QCOMPARE(userquakeLevel(latest->confidence), 4);
        QCOMPARE(latest->startedAt, jst(2026, 11, 16, 13, 25, 36, 68));
        QCOMPARE(latest->areaConfidences.size(), 24);
        // 信頼度の高い順に並べ替えられている
        QCOMPARE(latest->areaConfidences.at(0).area, 111);
        QCOMPARE(latest->areaConfidences.at(0).display, u"B"_s);
        QCOMPARE(latest->areaConfidences.at(1).area, 200);

        QCOMPARE(userquakeLevel(events.at(0).as<UserquakeEvaluation>()->confidence), 1);
        QCOMPARE(userquakeLevel(events.at(2).as<UserquakeEvaluation>()->confidence), 0);
    }

    void webSocketUsesUnderscoreId()
    {
        // WebSocket API は "id" ではなく "_id" で配信してくる。
        const auto event = parseEvent(QByteArray(
                R"({"_id":"6afb0256819a78200c1f6daf","code":554,"time":"2026/11/16 22:17:10.123","type":"Full"})"));
        QVERIFY(event);
        QCOMPARE(event->id, u"6afb0256819a78200c1f6daf"_s);
    }

    void rejectInvalid()
    {
        QVERIFY(!parseEvent(QByteArray("not json")));
        QVERIFY(!parseEvent(QByteArray("[]")));
        // 未知の情報コード
        QVERIFY(!parseEvent(QByteArray(R"({"code":999,"id":"x","time":"2026/01/01 00:00:00"})")));
        // id が無い
        QVERIFY(!parseEvent(QByteArray(R"({"code":554,"time":"2026/01/01 00:00:00","type":"Full"})")));
        // 必須の earthquake が無い
        QVERIFY(!parseEvent(QByteArray(R"({"code":551,"id":"x","time":"2026/01/01 00:00:00","issue":{}})")));

        QString error;
        QVERIFY(parseHistory("{", &error).isEmpty());
        QVERIFY(!error.isEmpty());
        error.clear();
        QVERIFY(parseHistory("[]", &error).isEmpty());
        QVERIFY(error.isEmpty());
        // 解釈できない要素だけを読み飛ばす
        QCOMPARE(parseHistory(R"([{"code":999}, {"code":554,"id":"x","time":"2026/01/01 00:00:00","type":"Full"}])").size(), 1);
    }

    // ---- 表示文字列 ----
    void labels()
    {
        QCOMPARE(scaleLabel(Scale::S5Lower), u"震度5弱"_s);
        QCOMPARE(scaleLabel(Scale::S7), u"震度7"_s);
        QCOMPARE(scaleLabel(Scale::Unknown), u"震度不明"_s);
        QCOMPARE(scaleShortLabel(Scale::S6Upper), u"6強"_s);
        QCOMPARE(scaleRangeLabel(Scale::S4, Scale::S4), u"震度4"_s);
        QCOMPARE(scaleRangeLabel(Scale::S3, Scale::S4), u"震度3～4"_s);
        QCOMPARE(scaleRangeLabel(Scale::S5Upper, Scale::OrMore), u"震度5強程度以上"_s);
        QCOMPARE(depthLabel(0), u"ごく浅い"_s);
        QCOMPARE(depthLabel(280), u"約280km"_s);
        QCOMPARE(depthLabel(-1), u"不明"_s);
        QCOMPARE(magnitudeLabel(5.0), u"M5.0"_s);
        QCOMPARE(magnitudeLabel(-1), u"M不明"_s);
        QCOMPARE(tsunamiGradeLabel(u"MajorWarning"_s), u"大津波警報"_s);
        QCOMPARE(areaName(10), u"北海道 石狩"_s);
        QCOMPARE(areaName(250), u"東京"_s);
        QCOMPARE(areaName(-5), u"地域コード -5"_s);
    }

    void summaries()
    {
        const Summary quake = summarize(loadFixture(u"quake.json"_s).at(1));
        QCOMPARE(quake.category, Category::Quake);
        QCOMPARE(quake.title, u"石川県能登地方"_s);
        QVERIFY(quake.subtitle.contains(u"M5.1"_s));
        QVERIFY(quake.subtitle.contains(u"深さ約280km"_s));
        QCOMPARE(quake.timeText, u"11/16 13:24"_s);

        const QList<Event> tsunami = loadFixture(u"tsunami.json"_s);
        QCOMPARE(summarize(tsunami.at(0)).title, u"津波予報 解除"_s);
        QCOMPARE(summarize(tsunami.at(1)).title, u"津波注意報"_s);

        const Summary eew = summarize(loadFixture(u"eew.json"_s).at(0));
        QCOMPARE(eew.category, Category::Eew);
        QVERIFY(eew.subtitle.contains(u"愛知県東部"_s));
        QVERIFY(eew.subtitle.contains(u"第1報"_s));

        const Summary userquake = summarize(loadFixture(u"userquake_evaluation.json"_s).at(1));
        QCOMPARE(userquake.title, u"地震感知情報 レベル4"_s);
        QVERIFY(userquake.subtitle.startsWith(u"54件"_s));
    }

    void detailHtmlContainsEverything()
    {
        const QList<Event> quakes = loadFixture(u"quake.json"_s);
        const QString html = detailHtml(quakes.at(0));
        QVERIFY(html.contains(u"各地の震度に関する情報"_s));
        QVERIFY(html.contains(u"最大震度1"_s));
        QVERIFY(html.contains(u"北緯35.3度 東経139.0度"_s));
        QVERIFY(html.contains(u"津波の心配なし"_s));
        for (const QuakePoint &point : quakes.at(0).as<Quake>()->points)
            QVERIFY2(html.contains(point.addr), qPrintable(point.addr));

        const QString tsunami = detailHtml(loadFixture(u"tsunami.json"_s).at(1));
        QVERIFY(tsunami.contains(u"千葉県九十九里・外房"_s));
        QVERIFY(tsunami.contains(u"予想高さ １ｍ"_s));

        const QString eew = detailHtml(loadFixture(u"eew.json"_s).at(0));
        QVERIFY(eew.contains(u"震度3～4"_s));
        QVERIFY(eew.contains(u"到達予測 01:04:49"_s));
    }

    void htmlIsEscaped()
    {
        const auto event = parseEvent(QByteArray(R"({"code":551,"id":"x","time":"2026/01/01 00:00:00",
            "issue":{"time":"2026/01/01 00:00:00","type":"Foreign"},
            "earthquake":{"time":"2026/01/01 00:00:00","hypocenter":{"name":"<b>A&B</b>"}},
            "comments":{"freeFormComment":"<script>x</script>"}})"));
        QVERIFY(event);
        const QString html = detailHtml(*event);
        QVERIFY(html.contains(u"&lt;b&gt;A&amp;B&lt;/b&gt;"_s));
        QVERIFY(!html.contains(u"<script>"_s));
    }

    void notifications()
    {
        const QList<Event> quakes = loadFixture(u"quake.json"_s); // いずれも最大震度1

        NotifySettings settings;
        settings.minScale = Scale::S3;
        QVERIFY(!notificationFor(quakes.at(0), settings));

        settings.minScale = Scale::S1;
        const auto notification = notificationFor(quakes.at(0), settings);
        QVERIFY(notification);
        QCOMPARE(notification->title, u"各地の震度に関する情報 最大震度1"_s);
        QVERIFY(notification->body.contains(u"静岡県東部"_s));
        QVERIFY(!notification->critical);

        // 津波・緊急地震速報は震度のしきい値に関係なく通知する
        settings.minScale = Scale::S5Lower;
        QVERIFY(notificationFor(loadFixture(u"tsunami.json"_s).at(1), settings));
        const auto eew = notificationFor(loadFixture(u"eew.json"_s).at(0), settings);
        QVERIFY(eew);
        QVERIFY(eew->critical);
        QVERIFY(eew->body.contains(u"愛知県西部"_s));

        // 地震感知情報は明示的に有効にしたときだけ
        const Event userquake = loadFixture(u"userquake_evaluation.json"_s).at(1);
        QVERIFY(!notificationFor(userquake, settings));
        settings.userquake = true;
        QVERIFY(notificationFor(userquake, settings));

        settings.minScale = NotifySettings::Disabled;
        QVERIFY(!notificationFor(loadFixture(u"eew.json"_s).at(0), settings));
        QVERIFY(!notificationFor(userquake, settings));
    }

    // ---- 一覧 ----
    void modelKeepsNewestFirstAndDropsDuplicates()
    {
        const QList<Event> quakes = loadFixture(u"quake.json"_s); // 新しい順
        EventListModel model;

        QCOMPARE(model.addEvent(quakes.at(1)), EventListModel::AddResult::Inserted);
        QCOMPARE(model.addEvent(quakes.at(2)), EventListModel::AddResult::Inserted);
        QCOMPARE(model.addEvent(quakes.at(0)), EventListModel::AddResult::Inserted);
        QCOMPARE(model.addEvent(quakes.at(1)), EventListModel::AddResult::Ignored);
        QCOMPARE(model.rowCount(), 3);
        for (int i = 0; i < 3; ++i)
            QCOMPARE(model.eventAt(i)->id, quakes.at(i).id);

        // 一覧に出さない情報
        QCOMPARE(model.addEvent(loadFixture(u"areapeers.json"_s).at(0)), EventListModel::AddResult::Ignored);
        QCOMPARE(model.addEvent(loadFixture(u"userquake.json"_s).at(0)), EventListModel::AddResult::Ignored);
        QCOMPARE(model.rowCount(), 3);

        model.clear();
        QCOMPARE(model.rowCount(), 0);
        QCOMPARE(model.addEvent(quakes.at(0)), EventListModel::AddResult::Inserted);
    }

    void modelMergesUserquakeEvaluations()
    {
        // [0] 同じ地震の最初の解析結果、[1] 同じ地震の最新の解析結果、[2] 信頼度 0 の別の地震
        const QList<Event> evaluations = loadFixture(u"userquake_evaluation.json"_s);
        EventListModel model;

        QCOMPARE(model.addEvent(evaluations.at(2)), EventListModel::AddResult::Ignored);
        QCOMPARE(model.addEvent(evaluations.at(0)), EventListModel::AddResult::Inserted);
        QCOMPARE(model.addEvent(evaluations.at(1)), EventListModel::AddResult::Updated);
        QCOMPARE(model.rowCount(), 1);
        QCOMPARE(model.eventAt(0)->as<UserquakeEvaluation>()->count, 54);

        // 古い解析結果が後から届いても巻き戻らない (履歴は新しい順に届く)
        QCOMPARE(model.addEvent(evaluations.at(0)), EventListModel::AddResult::Ignored);
        QCOMPARE(model.eventAt(0)->as<UserquakeEvaluation>()->count, 54);
    }

    void modelIsCapped()
    {
        EventListModel model;
        const QDateTime base = jst(2026, 1, 1, 0, 0, 0);
        for (int i = 0; i < EventListModel::kMaxItems + 20; ++i) {
            Event event;
            event.id = QString::number(i);
            event.code = Code::EewDetection;
            event.time = base.addSecs(i);
            event.body = EewDetection{u"Full"_s};
            model.addEvent(event);
        }
        QCOMPARE(model.rowCount(), int(EventListModel::kMaxItems));
        QCOMPARE(model.eventAt(0)->id, QString::number(EventListModel::kMaxItems + 19));
        // 押し出された古い情報は、もう一度届いても末尾より古いので入らない
        Event old;
        old.id = u"0"_s;
        old.code = Code::EewDetection;
        old.time = base;
        old.body = EewDetection{u"Full"_s};
        QCOMPARE(model.addEvent(old), EventListModel::AddResult::Ignored);
    }

    void categoryFilter()
    {
        EventListModel model;
        CategoryFilterModel filter;
        filter.setSourceModel(&model);
        for (const Event &event : loadFixture(u"quake.json"_s))
            model.addEvent(event);
        model.addEvent(loadFixture(u"eew.json"_s).at(0));
        QCOMPARE(filter.rowCount(), 4);

        filter.setCategoryVisible(Category::Quake, false);
        QCOMPARE(filter.rowCount(), 1);
        filter.setCategoryVisible(Category::Eew, false);
        QCOMPARE(filter.rowCount(), 0);
        filter.setCategoryVisible(Category::Quake, true);
        QCOMPARE(filter.rowCount(), 3);
    }

    // ---- 通信 ----
    void urls()
    {
        Client client;
        QCOMPARE(client.historyUrl({551, 552}, 50).toString(),
                 u"https://api.p2pquake.net/v2/history?codes=551&codes=552&limit=50"_s);
        QCOMPARE(client.historyUrl({555}, 1000).toString(),
                 u"https://api.p2pquake.net/v2/history?codes=555&limit=100"_s);
        QCOMPARE(client.webSocketUrl().toString(), u"wss://api.p2pquake.net/v2/ws"_s);

        client.setSandbox(true);
        QCOMPARE(client.historyUrl({551}, 10).host(), u"api-v2-sandbox.p2pquake.net"_s);
        QCOMPARE(client.webSocketUrl().toString(), u"wss://api-realtime-sandbox.p2pquake.net/v2/ws"_s);
    }
};

QTEST_MAIN(TestP2PQuake)
#include "tst_p2pquake.moc"
