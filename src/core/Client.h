#pragma once

#include "core/Types.h"

#include <QNetworkAccessManager>
#include <QObject>
#include <QQueue>
#include <QSet>
#include <QTimer>
#include <QUrl>
#include <QWebSocket>

namespace p2pquake {

// P2P地震情報 JSON API v2 のクライアント。
//  - /history で過去の情報を取得する
//  - /ws (WebSocket) でリアルタイムの情報を受信し、切断時は自動で再接続する
class Client : public QObject
{
    Q_OBJECT

public:
    enum class State { Stopped, Connecting, Connected, Waiting };
    Q_ENUM(State)

    explicit Client(QObject *parent = nullptr);

    // サンドボックス (動作確認用エンドポイント) を使うかどうか。start() の前に設定する。
    void setSandbox(bool sandbox);
    bool isSandbox() const { return m_sandbox; }

    State state() const { return m_state; }

    QUrl historyUrl(const QList<int> &codes, int limit) const;
    QUrl webSocketUrl() const;

public slots:
    // 履歴を取得し、WebSocket に接続する。
    void start();
    void stop();
    // 履歴を取り直す。結果は historyReceived で届く。
    void refreshHistory();

signals:
    void stateChanged(p2pquake::Client::State state);
    // /history の結果。新しい順。
    void historyReceived(const QList<p2pquake::Event> &events);
    // WebSocket で受信した情報。同一 id の重複は取り除き済み。
    void eventReceived(const p2pquake::Event &event);
    void errorOccurred(const QString &message);

private:
    void setState(State state);
    void openSocket();
    void scheduleReconnect();
    void requestHistory(const QList<int> &codes, int limit);
    void onTextMessage(const QString &message);
    bool rememberId(const QString &id);

    QNetworkAccessManager m_network;
    QWebSocket m_socket;
    QTimer m_reconnectTimer;
    QTimer m_pingTimer;
    QTimer m_pongTimeout;

    bool m_sandbox = false;
    bool m_running = false;
    bool m_connectedOnce = false;
    int m_reconnectAttempts = 0;
    // stop() のたびに進める。切り替え前のエンドポイントから遅れて届いた履歴を捨てるために使う。
    int m_generation = 0;
    State m_state = State::Stopped;

    // WebSocket は同一の情報を複数回配信し得るため、直近の id を覚えて重複を除く。
    QSet<QString> m_seenIds;
    QQueue<QString> m_seenOrder;
};

} // namespace p2pquake
