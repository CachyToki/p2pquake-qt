#include "core/Client.h"

#include "core/Parser.h"

#include <QLoggingCategory>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QUrlQuery>

#include <algorithm>
#include <chrono>

using namespace Qt::StringLiterals;
using namespace std::chrono_literals;

namespace p2pquake {

namespace {

Q_LOGGING_CATEGORY(lcClient, "p2pquake.client")

constexpr int kSeenIdLimit = 2000;
constexpr auto kPingInterval = 30s;
constexpr auto kPongTimeout = 20s;
constexpr int kHistoryTimeoutMs = 15000;
constexpr int kMaxReconnectDelaySec = 60;

QByteArray userAgent()
{
    return QByteArrayLiteral("p2pquake-qt/" P2PQUAKE_QT_VERSION);
}

} // namespace

Client::Client(QObject *parent)
    : QObject(parent)
{
    m_reconnectTimer.setSingleShot(true);
    connect(&m_reconnectTimer, &QTimer::timeout, this, &Client::openSocket);

    // 無通信のまま切れた接続を検出するため、定期的に ping を送り pong を待つ。
    m_pingTimer.setInterval(kPingInterval);
    connect(&m_pingTimer, &QTimer::timeout, this, [this] {
        m_socket.ping();
        if (!m_pongTimeout.isActive())
            m_pongTimeout.start();
    });
    m_pongTimeout.setSingleShot(true);
    m_pongTimeout.setInterval(kPongTimeout);
    connect(&m_pongTimeout, &QTimer::timeout, this, [this] {
        qCWarning(lcClient) << "pong が返ってこないため再接続します";
        m_socket.abort(); // disconnected シグナル経由で再接続される
    });
    connect(&m_socket, &QWebSocket::pong, &m_pongTimeout, &QTimer::stop);

    connect(&m_socket, &QWebSocket::connected, this, [this] {
        qCInfo(lcClient) << "WebSocket 接続:" << m_socket.requestUrl().toString();
        m_reconnectAttempts = 0;
        m_pingTimer.start();
        setState(State::Connected);
        // 再接続の場合、切断中に発表された情報を履歴で補う。
        if (m_connectedOnce)
            refreshHistory();
        m_connectedOnce = true;
    });
    connect(&m_socket, &QWebSocket::disconnected, this, [this] {
        m_pingTimer.stop();
        m_pongTimeout.stop();
        if (!m_running) {
            setState(State::Stopped);
            return;
        }
        qCInfo(lcClient) << "WebSocket 切断:" << m_socket.closeCode() << m_socket.closeReason();
        scheduleReconnect();
    });
    connect(&m_socket, &QWebSocket::errorOccurred, this, [this](QAbstractSocket::SocketError) {
        qCWarning(lcClient) << "WebSocket エラー:" << m_socket.errorString();
        emit errorOccurred(u"リアルタイム接続エラー: %1"_s.arg(m_socket.errorString()));
        // 接続確立前の失敗では disconnected が来ないことがあるため、ここでも再接続を予約する。
        if (m_running && m_socket.state() == QAbstractSocket::UnconnectedState)
            scheduleReconnect();
    });
    connect(&m_socket, &QWebSocket::textMessageReceived, this, &Client::onTextMessage);
}

void Client::setSandbox(bool sandbox)
{
    m_sandbox = sandbox;
}

QUrl Client::historyUrl(const QList<int> &codes, int limit) const
{
    QUrl url(m_sandbox ? u"https://api-v2-sandbox.p2pquake.net/v2/history"_s
                       : u"https://api.p2pquake.net/v2/history"_s);
    QUrlQuery query;
    for (int code : codes)
        query.addQueryItem(u"codes"_s, QString::number(code));
    query.addQueryItem(u"limit"_s, QString::number(std::clamp(limit, 1, 100)));
    url.setQuery(query);
    return url;
}

QUrl Client::webSocketUrl() const
{
    return QUrl(m_sandbox ? u"wss://api-realtime-sandbox.p2pquake.net/v2/ws"_s
                          : u"wss://api.p2pquake.net/v2/ws"_s);
}

void Client::start()
{
    if (m_running)
        return;
    m_running = true;
    m_connectedOnce = false;
    m_reconnectAttempts = 0;
    refreshHistory();
    openSocket();
}

void Client::stop()
{
    m_running = false;
    ++m_generation;
    m_reconnectTimer.stop();
    m_pingTimer.stop();
    m_pongTimeout.stop();
    if (m_socket.state() != QAbstractSocket::UnconnectedState)
        m_socket.abort();
    setState(State::Stopped);
}

void Client::refreshHistory()
{
    // 地震感知の解析結果は 1 回の地震で何十件も更新が流れるため、他の情報を
    // 押し出さないよう別のリクエストで取得する。(/history は 60 リクエスト/分まで)
    requestHistory({Code::Quake, Code::Tsunami, Code::EewDetection, Code::Eew}, 100);
    requestHistory({Code::UserquakeEvaluation}, 100);
    requestHistory({Code::Areapeers}, 1);
}

void Client::setState(State state)
{
    if (m_state == state)
        return;
    m_state = state;
    emit stateChanged(state);
}

void Client::openSocket()
{
    if (!m_running || m_socket.state() != QAbstractSocket::UnconnectedState)
        return;
    setState(State::Connecting);

    QNetworkRequest request(webSocketUrl());
    request.setHeader(QNetworkRequest::UserAgentHeader, userAgent());
    m_socket.open(request);
}

void Client::scheduleReconnect()
{
    if (m_reconnectTimer.isActive())
        return;
    // 2, 4, 8 ... 最大 60 秒。接続数制限 (2 接続/IP) のあるサーバに負荷を掛けない。
    const int delaySec = std::min(kMaxReconnectDelaySec, 2 << std::min(m_reconnectAttempts, 5));
    ++m_reconnectAttempts;
    qCInfo(lcClient) << delaySec << "秒後に再接続します";
    setState(State::Waiting);
    m_reconnectTimer.start(delaySec * 1000);
}

void Client::requestHistory(const QList<int> &codes, int limit)
{
    QNetworkRequest request(historyUrl(codes, limit));
    request.setHeader(QNetworkRequest::UserAgentHeader, userAgent());
    request.setTransferTimeout(kHistoryTimeoutMs);

    QNetworkReply *reply = m_network.get(request);
    connect(reply, &QNetworkReply::finished, this, [this, reply, generation = m_generation] {
        reply->deleteLater();
        if (generation != m_generation)
            return;
        if (reply->error() != QNetworkReply::NoError) {
            qCWarning(lcClient) << "履歴の取得に失敗:" << reply->url().toString()
                                << reply->errorString();
            emit errorOccurred(u"履歴の取得に失敗しました: %1"_s.arg(reply->errorString()));
            return;
        }

        QString parseError;
        const QList<Event> events = parseHistory(reply->readAll(), &parseError);
        if (!parseError.isEmpty()) {
            qCWarning(lcClient) << "履歴の解析に失敗:" << parseError;
            emit errorOccurred(u"履歴の解析に失敗しました: %1"_s.arg(parseError));
            return;
        }
        qCInfo(lcClient) << "履歴を取得:" << reply->url().query() << events.size() << "件";
        emit historyReceived(events);
    });
}

void Client::onTextMessage(const QString &message)
{
    const std::optional<Event> event = parseEvent(message.toUtf8());
    if (!event) {
        qCDebug(lcClient) << "解釈できないメッセージ:" << message.left(200);
        return;
    }
    if (!rememberId(event->id))
        return;
    qCDebug(lcClient) << "受信:" << event->code << event->id;
    emit eventReceived(*event);
}

// 初めて見る id なら記録して true を返す。
bool Client::rememberId(const QString &id)
{
    if (m_seenIds.contains(id))
        return false;
    m_seenIds.insert(id);
    m_seenOrder.enqueue(id);
    while (m_seenOrder.size() > kSeenIdLimit)
        m_seenIds.remove(m_seenOrder.dequeue());
    return true;
}

} // namespace p2pquake
