#include "core/AreaTable.h"

#include <QFile>
#include <QHash>

using namespace Qt::StringLiterals;

namespace p2pquake {

namespace {

// 列: 地域コード(文字列型),地域コード(数値型),地方,都道府県,地域,緯度,経度
QHash<int, QString> loadAreas()
{
    QHash<int, QString> areas;
    QFile file(u":/epsp-area.csv"_s);
    if (!file.open(QIODevice::ReadOnly))
        return areas;

    file.readLine(); // ヘッダ行
    while (!file.atEnd()) {
        const QString line = QString::fromUtf8(file.readLine()).trimmed();
        const QStringList columns = line.split(u',');
        if (columns.size() < 5)
            continue;
        bool ok = false;
        const int code = columns.at(1).toInt(&ok);
        if (ok)
            areas.insert(code, columns.at(4));
    }
    return areas;
}

} // namespace

QString areaName(int code)
{
    static const QHash<int, QString> areas = loadAreas();
    const auto it = areas.constFind(code);
    return it != areas.constEnd() ? *it : u"地域コード %1"_s.arg(code);
}

} // namespace p2pquake
