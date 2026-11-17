#include "ui/MainWindow.h"

#include "core/Labels.h"
#include "ui/EventDelegate.h"

#include <QActionGroup>
#include <QApplication>
#include <QCloseEvent>
#include <QLabel>
#include <QListView>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QScrollBar>
#include <QSettings>
#include <QSplitter>
#include <QStatusBar>
#include <QSystemTrayIcon>
#include <QTextBrowser>
#include <QToolBar>

using namespace Qt::StringLiterals;

namespace p2pquake::ui {

namespace {

constexpr int kErrorMessageMs = 8000;
constexpr int kNotificationMs = 15000;

QString placeholderHtml(const QString &text)
{
    return u"<p style=\"color:#808890;\">%1</p>"_s.arg(text.toHtmlEscaped());
}

} // namespace

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    setWindowTitle(u"P2P地震情報"_s);
    setWindowIcon(QIcon(u":/p2pquake-qt.svg"_s));
    resize(980, 640);

    m_filter.setSourceModel(&m_model);

    m_list = new QListView(this);
    m_list->setModel(&m_filter);
    m_list->setItemDelegate(new EventDelegate(m_list));
    m_list->setUniformItemSizes(true);
    m_list->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    m_list->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_list->setMinimumWidth(280);

    m_detail = new QTextBrowser(this);
    m_detail->setOpenExternalLinks(true);
    m_detail->document()->setDocumentMargin(16);
    m_detail->setHtml(placeholderHtml(u"情報を読み込んでいます…"_s));

    auto *splitter = new QSplitter(this);
    splitter->addWidget(m_list);
    splitter->addWidget(m_detail);
    splitter->setStretchFactor(0, 0);
    splitter->setStretchFactor(1, 1);
    splitter->setSizes({360, 620});
    splitter->setChildrenCollapsible(false);
    setCentralWidget(splitter);

    m_stateLabel = new QLabel(this);
    m_peerLabel = new QLabel(this);
    statusBar()->addPermanentWidget(m_peerLabel);
    statusBar()->addPermanentWidget(m_stateLabel);

    createActions();
    loadSettings();
    createTray();

    connect(m_list->selectionModel(), &QItemSelectionModel::currentChanged, this,
            &MainWindow::showCurrent);
    // 表示中の地震感知情報が更新されたら、詳細も描き直す。
    connect(&m_filter, &QAbstractItemModel::dataChanged, this,
            [this](const QModelIndex &topLeft, const QModelIndex &bottomRight) {
                const int row = m_list->currentIndex().row();
                if (row < topLeft.row() || row > bottomRight.row())
                    return;
                // 読んでいる途中で先頭に戻されないよう、スクロール位置を保つ。
                QScrollBar *bar = m_detail->verticalScrollBar();
                const int position = bar->value();
                showCurrent();
                bar->setValue(position);
            });

    connect(&m_client, &Client::historyReceived, this, &MainWindow::addHistory);
    connect(&m_client, &Client::eventReceived, this, &MainWindow::addRealtime);
    connect(&m_client, &Client::stateChanged, this, &MainWindow::onStateChanged);
    connect(&m_client, &Client::errorOccurred, this,
            [this](const QString &message) { statusBar()->showMessage(message, kErrorMessageMs); });
    onStateChanged(m_client.state());
}

MainWindow::~MainWindow()
{
    // メンバの破棄中にシグナルが届かないよう、先に通信を止める。
    m_client.disconnect(this);
    m_client.stop();
}

void MainWindow::start()
{
    m_client.setSandbox(m_sandboxAction->isChecked());
    m_client.start();
}

void MainWindow::addHistory(const QList<Event> &events)
{
    const bool followTop = isFollowingTop();
    for (const Event &event : events) {
        if (const auto *peers = event.as<Areapeers>())
            updatePeers(*peers);
        else
            m_model.addEvent(event);
    }
    m_historyLoaded = true;
    if (followTop)
        selectTop();
    if (!m_list->currentIndex().isValid())
        showCurrent();
}

void MainWindow::addRealtime(const Event &event)
{
    if (const auto *peers = event.as<Areapeers>()) {
        updatePeers(*peers);
        return;
    }

    const bool followTop = isFollowingTop();
    if (m_model.addEvent(event) != EventListModel::AddResult::Inserted)
        return;
    if (followTop)
        selectTop();
    notify(event);
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    saveSettings();
    if (!m_quitting && m_tray && m_tray->isVisible() && m_closeToTrayAction->isChecked()) {
        hide();
        event->ignore();
        return;
    }
    event->accept();
    QApplication::quit();
}

void MainWindow::quitApplication()
{
    m_quitting = true;
    close(); // closeEvent() で設定を保存して終了する
}

void MainWindow::createActions()
{
    // ---- ファイル
    QMenu *fileMenu = menuBar()->addMenu(u"ファイル(&F)"_s);
    QAction *refreshAction = fileMenu->addAction(
            QIcon::fromTheme(u"view-refresh"_s), u"再読み込み(&R)"_s);
    refreshAction->setShortcut(QKeySequence::Refresh);
    connect(refreshAction, &QAction::triggered, &m_client, &Client::refreshHistory);
    fileMenu->addSeparator();
    QAction *quitAction = fileMenu->addAction(
            QIcon::fromTheme(u"application-exit"_s), u"終了(&Q)"_s);
    quitAction->setShortcut(QKeySequence::Quit);
    connect(quitAction, &QAction::triggered, this, &MainWindow::quitApplication);

    // ---- 表示 (種類ごとの絞り込み。ツールバーにも同じものを置く)
    QMenu *viewMenu = menuBar()->addMenu(u"表示(&V)"_s);
    QToolBar *toolBar = addToolBar(u"絞り込み"_s);
    toolBar->setObjectName(u"filterToolBar"_s);
    toolBar->setMovable(false);
    toolBar->setToolButtonStyle(Qt::ToolButtonTextOnly);

    const QPair<Category, QString> categories[] = {
        {Category::Quake, u"地震情報"_s},
        {Category::Tsunami, u"津波予報"_s},
        {Category::Eew, u"緊急地震速報"_s},
        {Category::Userquake, u"地震感知情報"_s},
    };
    for (const auto &[category, label] : categories) {
        auto *action = new QAction(label, this);
        action->setCheckable(true);
        action->setChecked(true);
        // toggled は設定の読み込み時にも飛ぶので、表示への反映だけを行う。
        connect(action, &QAction::toggled, this, [this, category = category](bool visible) {
            m_filter.setCategoryVisible(category, visible);
            if (!m_list->currentIndex().isValid())
                selectTop();
            showCurrent();
        });
        connect(action, &QAction::triggered, this, &MainWindow::saveSettings);
        viewMenu->addAction(action);
        toolBar->addAction(action);
        m_categoryActions.append({category, action});
    }
    toolBar->addSeparator();
    toolBar->addAction(refreshAction);

    // ---- 設定
    QMenu *settingsMenu = menuBar()->addMenu(u"設定(&S)"_s);
    QMenu *notifyMenu = settingsMenu->addMenu(u"地震情報の通知"_s);
    auto *notifyGroup = new QActionGroup(this);
    const QPair<int, QString> thresholds[] = {
        {NotifySettings::Disabled, u"通知しない（津波・緊急地震速報も含む）"_s},
        {NotifySettings::AllQuakes, u"すべての地震情報"_s},
        {Scale::S1, u"震度1以上"_s},
        {Scale::S3, u"震度3以上"_s},
        {Scale::S4, u"震度4以上"_s},
        {Scale::S5Lower, u"震度5弱以上"_s},
    };
    for (const auto &[minScale, label] : thresholds) {
        QAction *action = notifyMenu->addAction(label);
        action->setCheckable(true);
        action->setData(minScale);
        notifyGroup->addAction(action);
        connect(action, &QAction::triggered, this, [this, minScale = minScale] {
            m_notify.minScale = minScale;
            saveSettings();
        });
        m_notifyActions.append(action);
    }
    m_notifyUserquakeAction = settingsMenu->addAction(u"地震感知情報も通知する"_s);
    m_notifyUserquakeAction->setCheckable(true);
    connect(m_notifyUserquakeAction, &QAction::triggered, this, [this](bool on) {
        m_notify.userquake = on;
        saveSettings();
    });

    settingsMenu->addSeparator();
    m_closeToTrayAction = settingsMenu->addAction(u"ウィンドウを閉じてもトレイに常駐する"_s);
    m_closeToTrayAction->setCheckable(true);
    connect(m_closeToTrayAction, &QAction::triggered, this, &MainWindow::saveSettings);

    settingsMenu->addSeparator();
    m_sandboxAction = settingsMenu->addAction(u"サンドボックスに接続する（動作確認用）"_s);
    m_sandboxAction->setCheckable(true);
    m_sandboxAction->setToolTip(u"過去の情報を約 30 秒おきに配信するテスト用サーバに接続します"_s);
    connect(m_sandboxAction, &QAction::triggered, this, &MainWindow::setSandbox);

    // ---- ヘルプ
    QMenu *helpMenu = menuBar()->addMenu(u"ヘルプ(&H)"_s);
    connect(helpMenu->addAction(u"このアプリについて(&A)"_s), &QAction::triggered, this,
            &MainWindow::showAbout);
}

void MainWindow::createTray()
{
    if (!QSystemTrayIcon::isSystemTrayAvailable())
        return;

    m_tray = new QSystemTrayIcon(windowIcon(), this);
    m_tray->setToolTip(windowTitle());

    auto *menu = new QMenu(this);
    connect(menu->addAction(u"ウィンドウを表示"_s), &QAction::triggered, this,
            &MainWindow::showAndRaise);
    menu->addSeparator();
    connect(menu->addAction(u"終了"_s), &QAction::triggered, this, &MainWindow::quitApplication);
    m_tray->setContextMenu(menu);

    connect(m_tray, &QSystemTrayIcon::activated, this,
            [this](QSystemTrayIcon::ActivationReason reason) {
                if (reason == QSystemTrayIcon::Trigger)
                    showAndRaise();
            });
    connect(m_tray, &QSystemTrayIcon::messageClicked, this, &MainWindow::showAndRaise);
    m_tray->show();
}

void MainWindow::loadSettings()
{
    QSettings settings;
    m_notify.minScale = settings.value("notify/minScale", m_notify.minScale).toInt();
    m_notify.userquake = settings.value("notify/userquake", m_notify.userquake).toBool();

    bool matched = false;
    for (QAction *action : std::as_const(m_notifyActions)) {
        const bool match = action->data().toInt() == m_notify.minScale;
        action->setChecked(match);
        matched |= match;
    }
    if (!matched) { // 設定ファイルに未知の値が入っていた場合は既定値に戻す
        m_notify.minScale = NotifySettings().minScale;
        for (QAction *action : std::as_const(m_notifyActions))
            action->setChecked(action->data().toInt() == m_notify.minScale);
    }

    // 設定の保存は triggered (ユーザーの操作) でのみ行うので、ここでの setChecked() では保存されない。
    m_notifyUserquakeAction->setChecked(m_notify.userquake);
    m_closeToTrayAction->setChecked(settings.value("window/closeToTray", false).toBool());
    m_sandboxAction->setChecked(settings.value("api/sandbox", false).toBool());
    for (const auto &[category, action] : std::as_const(m_categoryActions)) {
        const QString key = u"view/category%1"_s.arg(static_cast<int>(category));
        action->setChecked(settings.value(key, true).toBool());
    }
    const QByteArray geometry = settings.value("window/geometry").toByteArray();
    if (!geometry.isEmpty())
        restoreGeometry(geometry);
}

void MainWindow::saveSettings() const
{
    QSettings settings;
    settings.setValue("notify/minScale", m_notify.minScale);
    settings.setValue("notify/userquake", m_notify.userquake);
    settings.setValue("window/closeToTray", m_closeToTrayAction->isChecked());
    settings.setValue("window/geometry", saveGeometry());
    settings.setValue("api/sandbox", m_sandboxAction->isChecked());
    for (const auto &[category, action] : m_categoryActions)
        settings.setValue(u"view/category%1"_s.arg(static_cast<int>(category)), action->isChecked());
}

void MainWindow::showCurrent()
{
    const QModelIndex current = m_list->currentIndex();
    const Event *event = current.isValid()
            ? m_model.eventAt(m_filter.mapToSource(current).row()) : nullptr;
    if (event) {
        m_detail->setHtml(detailHtml(*event));
    } else if (!m_historyLoaded) {
        m_detail->setHtml(placeholderHtml(u"情報を読み込んでいます…"_s));
    } else if (m_filter.rowCount() == 0) {
        m_detail->setHtml(placeholderHtml(u"表示できる情報がありません。"_s));
    } else {
        m_detail->setHtml(placeholderHtml(u"左の一覧から情報を選択してください。"_s));
    }
}

void MainWindow::selectTop()
{
    const QModelIndex top = m_filter.index(0, 0);
    if (top.isValid()) {
        m_list->setCurrentIndex(top);
        m_list->scrollToTop();
    }
}

// 最新の情報を見ている (または何も選んでいない) 場合、新着が来たらそれを表示する。
// 過去の情報を読んでいる最中は選択を動かさない。
bool MainWindow::isFollowingTop() const
{
    const QModelIndex current = m_list->currentIndex();
    return !current.isValid() || current.row() == 0;
}

void MainWindow::updatePeers(const Areapeers &peers)
{
    m_peerLabel->setText(u"参加ピア数: %L1"_s.arg(peers.total()));
}

void MainWindow::onStateChanged(Client::State state)
{
    QString color;
    QString text;
    switch (state) {
    case Client::State::Connected:
        color = u"#2e9e4f"_s;
        text = u"リアルタイム受信中"_s;
        break;
    case Client::State::Connecting:
        color = u"#d99a00"_s;
        text = u"接続中…"_s;
        break;
    case Client::State::Waiting:
        color = u"#d32f2f"_s;
        text = u"切断（自動で再接続します）"_s;
        break;
    case Client::State::Stopped:
        color = u"#808890"_s;
        text = u"停止中"_s;
        break;
    }
    if (m_client.isSandbox())
        text += u"［サンドボックス］"_s;
    m_stateLabel->setText(u"<span style=\"color:%1;\">●</span> %2"_s.arg(color, text));
    if (m_tray)
        m_tray->setToolTip(u"%1 - %2"_s.arg(windowTitle(), text));
}

void MainWindow::setSandbox(bool sandbox)
{
    // 本番とサンドボックスの情報が混ざらないよう、一覧を空にしてつなぎ直す。
    m_client.stop();
    m_model.clear();
    m_historyLoaded = false;
    m_peerLabel->clear();
    showCurrent();
    saveSettings();

    m_client.setSandbox(sandbox);
    m_client.start();
}

void MainWindow::notify(const Event &event)
{
    if (!m_tray)
        return;
    const std::optional<Notification> notification = notificationFor(event, m_notify);
    if (!notification)
        return;
    m_tray->showMessage(notification->title, notification->body,
                        notification->critical ? QSystemTrayIcon::Warning
                                               : QSystemTrayIcon::Information,
                        kNotificationMs);
}

void MainWindow::showAndRaise()
{
    showNormal();
    raise();
    activateWindow();
}

void MainWindow::showAbout()
{
    QMessageBox::about(
            this, u"このアプリについて"_s,
            u"<h3>P2P地震情報 Qt クライアント %1</h3>"
            "<p>P2P地震情報の JSON API / WebSocket API を利用して、気象庁の地震情報・津波予報・"
            "緊急地震速報（警報）と、P2P地震情報ユーザーの地震感知情報を表示します。</p>"
            "<p>情報提供: <a href=\"https://www.p2pquake.net/\">P2P地震情報</a>"
            "（<a href=\"https://www.p2pquake.net/develop/json_api_v2/\">JSON API v2</a>）、"
            "気象庁</p>"
            ""
            "<p>このアプリは非公式のクライアントです。配信の遅延や欠落があり得るため、"
            "身を守るための情報源としては使用しないでください。</p>"
            ""
            "<p>© 2026 CachyToki</p>"
            "<p>Licensed under the MIT License</p>"_s
                    .arg(QStringLiteral(P2PQUAKE_QT_VERSION)));
}

} // namespace p2pquake::ui
