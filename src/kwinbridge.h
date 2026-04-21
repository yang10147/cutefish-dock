#pragma once
#include <QObject>
#include <QString>

// 通过 KWin Scripting DBus 激活指定 resourceClass 的窗口
class KWinBridge : public QObject
{
    Q_OBJECT
public:
    static KWinBridge *instance();

    // 激活 resourceClass 匹配的窗口（resourceClass 即 item->id）
    void activateWindowByClass(const QString &resourceClass);

    // 最小化 resourceClass 匹配的窗口
    void minimizeWindowByClass(const QString &resourceClass);

private:
    explicit KWinBridge(QObject *parent = nullptr);
    void runKWinScript(const QString &js);
    int loadScript(const QString &path, const QString &name);
};
