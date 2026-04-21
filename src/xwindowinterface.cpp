#include "xwindowinterface.h"
#include "kwinbridge.h"

#include <QDBusConnection>
#include <QDBusInterface>
#include <QDebug>
#include <QTimer>

XWindowInterface *XWindowInterface::instance()
{
    static XWindowInterface inst;
    return &inst;
}

XWindowInterface::XWindowInterface(QObject *parent)
    : QObject(parent)
{
    registerDBusService();
}

void XWindowInterface::registerDBusService()
{
    // 注册 DBus 服务名（路径由外部注册）
    bool ok = QDBusConnection::sessionBus().registerService("org.kde.cutefish.Dock");
    if (!ok) {
        qWarning() << "[XWindowInterface] registerService failed, maybe already registered";
    }
    // 对象路径在 main.cpp 里注册，避免与 /Dock 冲突
}

LayerShellQt::Window *XWindowInterface::layerShellWindow(QWindow *view)
{
    if (!view) return nullptr;
    return LayerShellQt::Window::get(view);
}

void XWindowInterface::setViewStruts(QWindow *view, int dockHeight, int style)
{
    auto *lsw = layerShellWindow(view);
    if (!lsw) return;

    lsw->setLayer(LayerShellQt::Window::LayerTop);

    LayerShellQt::Window::Anchors anchors;
    anchors.setFlag(LayerShellQt::Window::AnchorBottom);

    if (style == 1) {
        // Straight（延伸全宽）：三面锚定，推开底部空间
        anchors.setFlag(LayerShellQt::Window::AnchorLeft);
        anchors.setFlag(LayerShellQt::Window::AnchorRight);
        lsw->setAnchors(anchors);
        lsw->setExclusiveZone(dockHeight);
    } else {
        // Round（紧凑浮动）：只锚底部，不推开空间，dock 浮在壁纸上方
        lsw->setAnchors(anchors);
        lsw->setExclusiveZone(0);
    }

    lsw->setKeyboardInteractivity(LayerShellQt::Window::KeyboardInteractivityNone);
    lsw->setMargins(QMargins(0, 0, 0, 0));

    qDebug() << "[XWindowInterface] setViewStruts ok, exclusiveZone="
             << (style == 1 ? dockHeight : 0) << "style=" << style;
}

void XWindowInterface::clearViewStruts(QWindow *view)
{
    auto *lsw = layerShellWindow(view);
    if (!lsw) return;
    lsw->setExclusiveZone(0);
}

void XWindowInterface::updateExclusiveZone(QWindow *view, int dockHeight)
{
    auto *lsw = layerShellWindow(view);
    if (!lsw) return;
    lsw->setExclusiveZone(dockHeight);
}

WId XWindowInterface::activeWindow()          { return 0; }
void XWindowInterface::minimizeWindow(WId)    {}
void XWindowInterface::closeWindow(WId)       {}
void XWindowInterface::forceActiveWindow(WId) {}

void XWindowInterface::forceActiveWindowByClass(const QString &resourceClass)
{
    KWinBridge::instance()->activateWindowByClass(resourceClass);
}

void XWindowInterface::minimizeWindowByClass(const QString &resourceClass)
{
    KWinBridge::instance()->minimizeWindowByClass(resourceClass);
}

QMap<QString, QVariant> XWindowInterface::requestInfo(quint64)  { return {}; }
QString XWindowInterface::desktopFilePath(quint64)              { return {}; }
void XWindowInterface::setIconGeometry(quint64, const QRect &)  {}

void XWindowInterface::startInitWindows()
{
    qDebug() << "[XWindowInterface] startInitWindows: KWin script bridge active";
}

void XWindowInterface::enableBlurBehind(QWindow *view, bool enable, const QRegion &region)
{
    if (!view) return;
    KWindowEffects::enableBlurBehind(view, enable, region);
}

// ── KWin 脚本 → DBus → 这里 ───────────────────────────────────────────────

void XWindowInterface::onKWinWindowAdded(const QString &resourceClass, int pid)
{
    qDebug() << "[XWindowInterface] windowAdded:" << resourceClass << "pid:" << pid;
    emit windowAddedByClass(resourceClass, (quint64)pid);
}

void XWindowInterface::onKWinWindowRemoved(const QString &resourceClass, int pid)
{
    qDebug() << "[XWindowInterface] windowRemoved:" << resourceClass << "pid:" << pid;
    emit windowRemovedByClass(resourceClass, (quint64)pid);
}

void XWindowInterface::onKWinWindowActivated(const QString &resourceClass, int pid)
{
    qDebug() << "[XWindowInterface] windowActivated:" << resourceClass << "pid:" << pid;
    m_activeWindowClass = resourceClass;
    emit windowActivatedByClass(resourceClass, (quint64)pid);
}
