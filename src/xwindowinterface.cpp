#include "xwindowinterface.h"
#include <QDebug>

XWindowInterface *XWindowInterface::instance()
{
    static XWindowInterface inst;
    return &inst;
}

XWindowInterface::XWindowInterface(QObject *parent)
    : QObject(parent)
{
    // 窗口管理在 KDE Plasma 6 Wayland 下受系统限制：
    // org_kde_plasma_window_management 协议仅对 plasmashell 开放。
    // 待 cutefish 使用自己的 compositor 时可接入。
}

LayerShellQt::Window *XWindowInterface::layerShellWindow(QWindow *view)
{
    if (!view) return nullptr;
    return LayerShellQt::Window::get(view);
}

void XWindowInterface::setViewStruts(QWindow *view, int dockHeight)
{
    auto *lsw = layerShellWindow(view);
    if (!lsw) return;

    lsw->setLayer(LayerShellQt::Window::LayerTop);

    LayerShellQt::Window::Anchors anchors;
    anchors.setFlag(LayerShellQt::Window::AnchorBottom);
    anchors.setFlag(LayerShellQt::Window::AnchorLeft);
    anchors.setFlag(LayerShellQt::Window::AnchorRight);
    lsw->setAnchors(anchors);

    lsw->setExclusiveZone(dockHeight);
    lsw->setKeyboardInteractivity(LayerShellQt::Window::KeyboardInteractivityNone);
    lsw->setMargins(QMargins(0, 0, 0, 0));

    qDebug() << "[XWindowInterface] setViewStruts ok, exclusiveZone=" << dockHeight;
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

QMap<QString, QVariant> XWindowInterface::requestInfo(quint64)  { return {}; }
QString XWindowInterface::desktopFilePath(quint64)              { return {}; }
void XWindowInterface::setIconGeometry(quint64, const QRect &)  {}

void XWindowInterface::startInitWindows()
{
    qDebug() << "[XWindowInterface] startInitWindows: no-op (plasma_window_management unavailable)";
}

void XWindowInterface::enableBlurBehind(QWindow *view, bool enable, const QRegion &region)
{
    if (!view) return;
    KWindowEffects::enableBlurBehind(view, enable, region);
}
