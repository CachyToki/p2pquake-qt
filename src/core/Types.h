// P2P地震情報 JSON API v2 のデータ型。
// 仕様: https://www.p2pquake.net/develop/json_api_v2/
#pragma once

#include <QDateTime>
#include <QList>
#include <QString>

#include <variant>

namespace p2pquake {

// 情報コード
namespace Code {
enum : int {
    Quake = 551,                // 地震情報
    Tsunami = 552,              // 津波予報
    EewDetection = 554,         // 緊急地震速報 発表検出
    Areapeers = 555,            // 各地域ピア数
    Eew = 556,                  // 緊急地震速報（警報）
    Userquake = 561,            // 地震感知情報
    UserquakeEvaluation = 9611, // 地震感知情報 解析結果
};
}

// 震度。API の値をそのまま使う (10=震度1 ... 45=5弱 ... 70=震度7)。
namespace Scale {
enum : int {
    Unknown = -1,
    S0 = 0,
    S1 = 10,
    S2 = 20,
    S3 = 30,
    S4 = 40,
    S5Lower = 45,
    S5LowerOrMore = 46, // 5弱以上と推定されるが震度情報を入手していない
    S5Upper = 50,
    S6Lower = 55,
    S6Upper = 60,
    S7 = 70,
    OrMore = 99,        // EEW の scaleTo: 「～程度以上」
};
}

struct Hypocenter {
    QString name;
    QString reduceName;        // EEW のみ
    double latitude = -200.0;  // 情報なしは -200
    double longitude = -200.0; // 情報なしは -200
    int depth = -1;            // km。ごく浅い=0、情報なし=-1
    double magnitude = -1.0;   // 情報なしは -1

    bool hasPosition() const { return latitude > -200.0 && longitude > -200.0; }
};

struct QuakePoint {
    QString pref;
    QString addr;
    bool isArea = false;
    int scale = Scale::Unknown;
};

struct Quake {
    QString issueSource;
    QDateTime issueTime;
    QString issueType;       // ScalePrompt / Destination / ScaleAndDestination / DetailScale / Foreign / Other
    QString correct;         // None / Unknown / ScaleOnly / DestinationOnly / ScaleAndDestination
    QDateTime time;          // 発生日時
    Hypocenter hypocenter;
    int maxScale = Scale::Unknown;
    QString domesticTsunami;
    QString foreignTsunami;
    QList<QuakePoint> points;
    QString freeFormComment;
};

struct TsunamiArea {
    QString grade;           // MajorWarning / Warning / Watch / Unknown
    bool immediate = false;
    QString name;
    QDateTime arrivalTime;   // 無い場合は invalid
    QString condition;
    QString maxHeightDescription;
    double maxHeightValue = -1.0; // 無い場合は -1
};

struct Tsunami {
    bool cancelled = false;
    QString issueSource;
    QDateTime issueTime;
    QString issueType;
    QList<TsunamiArea> areas;
};

struct EewDetection {
    QString type; // Full / Chime
};

struct AreaPeer {
    int id = 0;
    int peer = 0;
};

struct Areapeers {
    QList<AreaPeer> areas;

    int total() const
    {
        int sum = 0;
        for (const AreaPeer &a : areas)
            sum += a.peer;
        return sum;
    }
};

struct EewArea {
    QString pref;
    QString name;
    int scaleFrom = Scale::Unknown;
    int scaleTo = Scale::Unknown;
    QString kindCode;        // 10 / 11 / 19
    QDateTime arrivalTime;   // 無い場合は invalid
};

struct Eew {
    bool test = false;
    bool cancelled = false;
    bool hasEarthquake = false; // 取消の場合 earthquake は設定されない
    QDateTime originTime;
    QDateTime arrivalTime;
    QString condition;
    Hypocenter hypocenter;
    QDateTime issueTime;
    QString eventId;
    QString serial;
    QList<EewArea> areas;
};

struct Userquake {
    int area = 0;
};

struct AreaConfidence {
    int area = 0;
    double confidence = 0.0;
    int count = 0;
    QString display; // A～F
};

struct UserquakeEvaluation {
    int count = 0;
    double confidence = 0.0;
    QDateTime startedAt; // イベントを一意に識別するキー
    QDateTime updatedAt;
    QList<AreaConfidence> areaConfidences;
};

using EventBody = std::variant<Quake, Tsunami, EewDetection, Areapeers, Eew, Userquake,
                               UserquakeEvaluation>;

struct Event {
    QString id;
    int code = 0;
    QDateTime time; // 受信日時
    EventBody body;

    template <typename T>
    const T *as() const { return std::get_if<T>(&body); }
};

} // namespace p2pquake
