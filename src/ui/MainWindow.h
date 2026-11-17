#pragma once

#include "core/Client.h"
#include "ui/EventListModel.h"
#include "ui/Presenter.h"

#include <QMainWindow>

class QAction;
class QLabel;
class QListView;
class QSystemTrayIcon;
class QTextBrowser;

namespace p2pquake::ui {

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

    // API への接続を開始する。
    void start();

public slots:
    // 履歴として届いた情報を一覧に加える (通知はしない)。
    void addHistory(const QList<p2pquake::Event> &events);
    // リアルタイムに届いた情報を一覧に加え、設定に応じて通知する。
    void addRealtime(const p2pquake::Event &event);

protected:
    void closeEvent(QCloseEvent *event) override;

private:
    void createActions();
    void createTray();
    void loadSettings();
    void saveSettings() const;

    void showCurrent();
    void selectTop();
    bool isFollowingTop() const;
    void updatePeers(const Areapeers &peers);
    void onStateChanged(Client::State state);
    void setSandbox(bool sandbox);
    void notify(const Event &event);
    void showAndRaise();
    void quitApplication();
    void showAbout();

    Client m_client;
    EventListModel m_model;
    CategoryFilterModel m_filter;
    NotifySettings m_notify;

    QListView *m_list = nullptr;
    QTextBrowser *m_detail = nullptr;
    QLabel *m_stateLabel = nullptr;
    QLabel *m_peerLabel = nullptr;
    QSystemTrayIcon *m_tray = nullptr;

    QList<QPair<Category, QAction *>> m_categoryActions;
    QList<QAction *> m_notifyActions;
    QAction *m_notifyUserquakeAction = nullptr;
    QAction *m_sandboxAction = nullptr;
    QAction *m_closeToTrayAction = nullptr;

    bool m_historyLoaded = false;
    bool m_quitting = false;
};

} // namespace p2pquake::ui
