#include "kwinbridge.h"

#include <QDBusInterface>
#include <QDBusReply>
#include <QFile>
#include <QDir>
#include <QTextStream>
#include <QCryptographicHash>
#include <QDebug>
#include <QTimer>

KWinBridge *KWinBridge::instance()
{
    static KWinBridge inst;
    return &inst;
}

KWinBridge::KWinBridge(QObject *parent) : QObject(parent) {}

void KWinBridge::activateWindowByClass(const QString &resourceClass)
{
    QString escaped = resourceClass;
    escaped.replace("'", "\\'");

    QString js = QString(R"JS(
(function() {
    var wins = workspace.stackingOrder;
    for (var i = wins.length - 1; i >= 0; i--) {
        var w = wins[i];
        if (!w.normalWindow) continue;
        if (w.resourceClass === '%1' || w.resourceName === '%1') {
            if (w.minimized) w.minimized = false;
            workspace.activeWindow = w;
            return;
        }
    }
})();
)JS").arg(escaped);

    runKWinScript(js);
}

void KWinBridge::minimizeWindowByClass(const QString &resourceClass)
{
    QString escaped = resourceClass;
    escaped.replace("'", "\\'");

    QString js = QString(R"JS(
(function() {
    var wins = workspace.stackingOrder;
    for (var i = wins.length - 1; i >= 0; i--) {
        var w = wins[i];
        if (!w.normalWindow) continue;
        if (w.resourceClass === '%1' || w.resourceName === '%1') {
            w.minimized = true;
            return;
        }
    }
})();
)JS").arg(escaped);

    runKWinScript(js);
}

void KWinBridge::runKWinScript(const QString &js)
{
    // 写脚本到临时文件
    QString hash = QCryptographicHash::hash(js.toUtf8(), QCryptographicHash::Md5).toHex();
    QString scriptDir = QDir::homePath() + "/.cache/cutefish-dock-scripts";
    QDir().mkpath(scriptDir);
    QString path = scriptDir + "/" + hash + ".js";

    {
        QFile f(path);
        if (!f.open(QFile::WriteOnly | QFile::Truncate)) {
            qWarning() << "[KWinBridge] cannot write script:" << path;
            return;
        }
        QTextStream s(&f);
        s << js;
    }

    QString name = "cutefish-dock-" + hash;
    int id = loadScript(path, name);
    if (id < 0) {
        // 已存在则先卸载再加载
        QDBusInterface scripting("org.kde.KWin", "/Scripting",
                                 "org.kde.kwin.Scripting",
                                 QDBusConnection::sessionBus());
        scripting.call("unloadScript", name);
        id = loadScript(path, name);
    }
    if (id < 0) {
        qWarning() << "[KWinBridge] loadScript failed for" << name;
        return;
    }

    QString scriptPath = QString("/Scripting/Script%1").arg(id);
    QDBusInterface script("org.kde.KWin", scriptPath,
                          "org.kde.kwin.Script",
                          QDBusConnection::sessionBus());
    script.call("run");

    // 异步卸载（给 run 一点时间）
    QTimer::singleShot(200, [name]() {
        QDBusInterface scripting("org.kde.KWin", "/Scripting",
                                 "org.kde.kwin.Scripting",
                                 QDBusConnection::sessionBus());
        scripting.call("unloadScript", name);
    });
}

int KWinBridge::loadScript(const QString &path, const QString &name)
{
    QDBusInterface scripting("org.kde.KWin", "/Scripting",
                             "org.kde.kwin.Scripting",
                             QDBusConnection::sessionBus());
    QDBusReply<int> reply = scripting.call("loadScript", path, name);
    if (!reply.isValid()) {
        qWarning() << "[KWinBridge] loadScript DBus error:" << reply.error().message();
        return -1;
    }
    return reply.value();
}
