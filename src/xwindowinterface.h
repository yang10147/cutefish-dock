#pragma once

#include <QObject>
#include <QWindow>
#include <QRect>
#include <QVariant>
#include <QMap>
#include <QString>

#include <LayerShellQt/Window>
#include <LayerShellQt/Shell>
#include <KWindowEffects>

class XWindowInterface : public QObject
{
    Q_OBJECT

public:
    static XWindowInterface *instance();
    ~XWindowInterface() override = default;

    void setViewStruts(QWindow *view, int dockHeight);
    void clearViewStruts(QWindow *view);
    void updateExclusiveZone(QWindow *view, int dockHeight);

    WId activeWindow();
    void minimizeWindow(WId win);
    void closeWindow(WId win);
    void forceActiveWindow(WId win);

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

private:
    explicit XWindowInterface(QObject *parent = nullptr);
    LayerShellQt::Window *layerShellWindow(QWindow *view);
};
