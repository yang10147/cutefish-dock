#pragma once

#include <QObject>
#include <QWindow>
#include <QRect>
#include <QVariant>
#include <QMap>
#include <QString>
#include <QDBusConnection>

#include <LayerShellQt/Window>
#include <LayerShellQt/Shell>
#include <KWindowEffects>

class XWindowInterface : public QObject
{
    Q_OBJECT

public:
    static XWindowInterface *instance();
    ~XWindowInterface() override = default;

    void setViewStruts(QWindow *view, int dockHeight, int style = 0);
    void clearViewStruts(QWindow *view);
    void updateExclusiveZone(QWindow *view, int dockHeight);

    WId activeWindow();
    void minimizeWindow(WId win);
    void closeWindow(WId win);
    void forceActiveWindow(WId win);

    // 新增：通过 resourceClass 操作窗口（Wayland 下使用）
    void forceActiveWindowByClass(const QString &resourceClass);
    void minimizeWindowByClass(const QString &resourceClass);

    // 当前激活窗口的 resourceClass
    QString activeWindowClass() const { return m_activeWindowClass; }

    QMap<QString, QVariant> requestInfo(quint64 wid);
    QString desktopFilePath(quint64 wid);
    void setIconGeometry(quint64 wid, const QRect &rect);

    Q_INVOKABLE void startInitWindows();

    void enableBlurBehind(QWindow *view, bool enable = true,
                          const QRegion &region = QRegion());

signals:
    void activeWindowChanged(WId win);
    void windowAdded(quint64 wid);
    void windowRemoved(quint64 wid);
    void activeChanged(quint64 wid);

    // 新增：基于 resourceClass 的信号
    void windowAddedByClass(const QString &resourceClass, quint64 pid);
    void windowRemovedByClass(const QString &resourceClass, quint64 pid);
    void windowActivatedByClass(const QString &resourceClass, quint64 pid);

public slots:
    // KWin 脚本通过 DBus 调用这些槽
    void onKWinWindowAdded(const QString &resourceClass, int pid);
    void onKWinWindowRemoved(const QString &resourceClass, int pid);
    void onKWinWindowActivated(const QString &resourceClass, int pid);

private:
    explicit XWindowInterface(QObject *parent = nullptr);
    LayerShellQt::Window *layerShellWindow(QWindow *view);
    void registerDBusService();

    QString m_activeWindowClass;
};
