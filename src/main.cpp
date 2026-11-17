#include "ui/MainWindow.h"

#include <QApplication>
#include <QDir>
#include <QLockFile>
#include <QMessageBox>
#include <QStandardPaths>

using namespace Qt::StringLiterals;

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    QApplication::setApplicationName(u"p2pquake-qt"_s);
    QApplication::setOrganizationName(u"p2pquake-qt"_s);
    QApplication::setApplicationDisplayName(u"P2P地震情報"_s);
    QApplication::setApplicationVersion(QStringLiteral(P2PQUAKE_QT_VERSION));
    QApplication::setDesktopFileName(u"p2pquake-qt"_s);
    // トレイに常駐している間はウィンドウが無くても終了しない。終了は MainWindow が行う。
    QApplication::setQuitOnLastWindowClosed(false);

    // WebSocket API は 1 つの IP アドレスにつき 2 接続までなので、多重起動を防ぐ。
    QString lockDir = QStandardPaths::writableLocation(QStandardPaths::RuntimeLocation);
    if (lockDir.isEmpty())
        lockDir = QDir::tempPath();
    QLockFile lock(lockDir + u"/p2pquake-qt.lock"_s);
    lock.setStaleLockTime(0);
    if (!lock.tryLock()) {
        QMessageBox::information(nullptr, QApplication::applicationDisplayName(),
                                 u"P2P地震情報はすでに起動しています。"_s);
        return 0;
    }

    p2pquake::ui::MainWindow window;
    window.show();
    window.start();

    return QApplication::exec();
}
