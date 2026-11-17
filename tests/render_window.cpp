// 開発用ツール: メインウィンドウを描画して PNG に保存する。
//   render_window <出力.png> [--live | --sandbox] [--wait 秒] [--row N]
// --live は本番の API、--sandbox はサンドボックスに接続し、--wait 秒 (既定 6) 待ってから保存する。
// どちらも付けない場合は tests/fixtures の内容を表示する。
#include "core/Parser.h"
#include "ui/MainWindow.h"

#include <QApplication>
#include <QDir>
#include <QFile>
#include <QListView>
#include <QSettings>
#include <QTemporaryDir>
#include <QTimer>

using namespace Qt::StringLiterals;

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    // 利用者の設定ファイルを読み書きしないよう、設定の保存先を一時ディレクトリに向ける。
    QTemporaryDir settingsDir;
    QSettings::setPath(QSettings::NativeFormat, QSettings::UserScope, settingsDir.path());
    QApplication::setApplicationName(u"p2pquake-qt-render"_s);
    QApplication::setOrganizationName(u"p2pquake-qt-render"_s);

    const QStringList args = QApplication::arguments();
    if (args.size() < 2) {
        qWarning("usage: render_window <output.png> [--live | --sandbox] [--wait SEC] [--row N]");
        return 2;
    }
    const QString output = args.at(1);
    const auto intOption = [&](const QString &name, int fallback) {
        const qsizetype i = args.indexOf(name);
        return i > 0 && i + 1 < args.size() ? args.at(i + 1).toInt() : fallback;
    };
    const bool sandbox = args.contains(u"--sandbox"_s);
    const bool live = sandbox || args.contains(u"--live"_s);
    const int row = intOption(u"--row"_s, 0);
    const int waitMs = live ? intOption(u"--wait"_s, 6) * 1000 : 300;
    if (sandbox)
        QSettings().setValue("api/sandbox", true);

    p2pquake::ui::MainWindow window;
    window.resize(1000, 680);
    window.show();

    if (live) {
        window.start();
    } else {
        QList<p2pquake::Event> events;
        const QDir fixtures(QStringLiteral(FIXTURE_DIR));
        for (const QString &name : fixtures.entryList({u"*.json"_s}, QDir::Files)) {
            QFile file(fixtures.filePath(name));
            if (file.open(QIODevice::ReadOnly))
                events += p2pquake::parseHistory(file.readAll());
        }
        window.addHistory(events);
    }

    QTimer::singleShot(waitMs, &app, [&] {
        if (auto *list = window.findChild<QListView *>()) {
            const QModelIndex index = list->model()->index(row, 0);
            if (index.isValid())
                list->setCurrentIndex(index);
        }
        QTimer::singleShot(200, &app, [&] {
            const bool ok = window.grab().save(output);
            qInfo("%s: %s (%d rows)", ok ? "saved" : "FAILED", qPrintable(output),
                  window.findChild<QListView *>()->model()->rowCount());
            QApplication::exit(ok ? 0 : 1);
        });
    });
    return QApplication::exec();
}
