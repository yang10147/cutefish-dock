/*
 * Copyright (C) 2021 CutefishOS Team.
 *
 * Author:     Reion Wong <reionwong@gmail.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "mainwindow.h"
#include "processprovider.h"
#include "dockadaptor.h"
#include "xwindowinterface.h"
#include "iconthemeimageprovider.h"

#include <QGuiApplication>
#include <QScreen>
#include <QQmlEngine>
#include <QQmlContext>
#include <QQmlProperty>
#include <QQuickItem>

#include <KWindowEffects>

MainWindow::MainWindow(QQuickView *parent)
    : QQuickView(parent)
    , m_activity(Activity::self())
    , m_settings(DockSettings::self())
    , m_appModel(new ApplicationModel)
    , m_fakeWindow(nullptr)
    , m_trashManager(new TrashManager)
    , m_hideBlocked(false)
    , m_showTimer(new QTimer(this))
    , m_hideTimer(new QTimer(this))
{
    new DockAdaptor(this);

    installEventFilter(this);

    setDefaultAlphaBuffer(false);
    setColor(Qt::transparent);
    setFlags(Qt::FramelessWindowHint | Qt::WindowDoesNotAcceptFocus);

    // 注册图标 provider，QML 里用 "image://icontheme/图标名" 加载系统图标
    engine()->addImageProvider(QStringLiteral("icontheme"), new IconThemeImageProvider);

    engine()->rootContext()->setContextProperty(QStringLiteral("appModel"), m_appModel);
    engine()->rootContext()->setContextProperty(QStringLiteral("process"), new ProcessProvider);
    engine()->rootContext()->setContextProperty(QStringLiteral("Settings"), m_settings);
    engine()->rootContext()->setContextProperty(QStringLiteral("mainWindow"), this);
    engine()->rootContext()->setContextProperty(QStringLiteral("trash"), m_trashManager);

    setSource(QUrl(QStringLiteral("qrc:/qml/main.qml")));
    setScreen(qApp->primaryScreen());
    setResizeMode(QQuickView::SizeRootObjectToView);
    initScreens();

    initSlideWindow();
    resizeWindow();
    onVisibilityChanged();

    m_showTimer->setSingleShot(true);
    m_showTimer->setInterval(200);
    connect(m_showTimer, &QTimer::timeout, this, [this] { setVisible(true); });

    m_hideTimer->setSingleShot(true);
    m_hideTimer->setInterval(500);
    connect(m_hideTimer, &QTimer::timeout, this, &MainWindow::onHideTimeout);

    connect(m_activity, &Activity::launchPadChanged, this, &MainWindow::onVisibilityChanged);
    connect(m_activity, &Activity::existsWindowMaximizedChanged, this, &MainWindow::onVisibilityChanged);

    connect(qGuiApp, &QGuiApplication::primaryScreenChanged, this, &MainWindow::onPrimaryScreenChanged);
    connect(screen(), &QScreen::virtualGeometryChanged, this, &MainWindow::resizeWindow);
    connect(screen(), &QScreen::geometryChanged, this, &MainWindow::resizeWindow);

    connect(m_appModel, &ApplicationModel::countChanged, this, &MainWindow::resizeWindow);
    connect(m_settings, &DockSettings::directionChanged, this, &MainWindow::onPositionChanged);
    connect(m_settings, &DockSettings::iconSizeChanged, this, &MainWindow::onIconSizeChanged);
    connect(m_settings, &DockSettings::visibilityChanged, this, &MainWindow::onVisibilityChanged);
    connect(m_settings, &DockSettings::styleChanged, this, &MainWindow::resizeWindow);
}

MainWindow::~MainWindow()
{
}

void MainWindow::add(const QString &desktop)    { m_appModel->addItem(desktop); }
void MainWindow::remove(const QString &desktop) { m_appModel->removeItem(desktop); }
bool MainWindow::pinned(const QString &desktop) { return m_appModel->isDesktopPinned(desktop); }

QRect MainWindow::primaryGeometry() const { return geometry(); }
int MainWindow::direction() const         { return DockSettings::self()->direction(); }
int MainWindow::visibility() const        { return DockSettings::self()->visibility(); }
int MainWindow::style() const             { return DockSettings::self()->style(); }

void MainWindow::setDirection(int direction)
{
    DockSettings::self()->setDirection(static_cast<DockSettings::Direction>(direction));
}
void MainWindow::setIconSize(int iconSize)     { DockSettings::self()->setIconSize(iconSize); }
void MainWindow::setVisibility(int visibility)
{
    DockSettings::self()->setVisibility(static_cast<DockSettings::Visibility>(visibility));
}
void MainWindow::setStyle(int style)
{
    DockSettings::self()->setStyle(static_cast<DockSettings::Style>(style));
}
void MainWindow::updateSize() { resizeWindow(); }

QRect MainWindow::windowRect() const
{
    const QRect screenGeometry   = screen()->geometry();
    const QRect availableGeometry = screen()->availableGeometry();

    bool isHorizontal = m_settings->direction() == DockSettings::Bottom;
    bool compositing  = false;

    if (QQuickItem *item = qobject_cast<QQuickItem *>(rootObject()))
        compositing = item->property("compositing").toBool();

    QSize  newSize(0, 0);
    QPoint position(0, 0);
    int maxLength = isHorizontal
        ? screenGeometry.width()  - m_settings->edgeMargins()
        : availableGeometry.height() - m_settings->edgeMargins();

    int appCount = m_appModel->rowCount() + 1;
    int iconSize  = m_settings->iconSize();
    iconSize += iconSize * 0.1;
    int length  = appCount * iconSize;
    int margins = compositing ? DockSettings::self()->edgeMargins() / 2 : 0;

    if (length >= maxLength) {
        iconSize = (maxLength - (maxLength % appCount)) / appCount;
        length   = appCount * iconSize;
    }

    switch (m_settings->style()) {
    case DockSettings::Round:
        switch (m_settings->direction()) {
        case DockSettings::Left:
            newSize   = QSize(iconSize, length);
            position  = { screenGeometry.x() + margins,
                          availableGeometry.y() + (availableGeometry.height() - newSize.height()) / 2 };
            break;
        case DockSettings::Bottom:
            newSize   = QSize(length, iconSize);
            position  = { screenGeometry.x() + (screenGeometry.width() - newSize.width()) / 2,
                          screenGeometry.y() + screenGeometry.height() - newSize.height() - margins };
            break;
        case DockSettings::Right:
            newSize   = QSize(iconSize, length);
            position  = { screenGeometry.x() + screenGeometry.width() - newSize.width() - margins,
                          availableGeometry.y() + (availableGeometry.height() - newSize.height()) / 2 };
            break;
        default: break;
        }
        break;

    case DockSettings::Straight:
        switch (m_settings->direction()) {
        case DockSettings::Left:
            newSize  = QSize(iconSize, screenGeometry.height());
            position = { screenGeometry.x(), screenGeometry.y() };
            break;
        case DockSettings::Bottom:
            newSize  = QSize(screenGeometry.width(), iconSize);
            position = { screenGeometry.x(),
                         screenGeometry.y() + screenGeometry.height() - newSize.height() };
            break;
        case DockSettings::Right:
            newSize  = QSize(iconSize, screenGeometry.height());
            position = { screenGeometry.x() + screenGeometry.width() - newSize.width(),
                         screenGeometry.y() + screenGeometry.height() - newSize.height() };
            break;
        default: break;
        }
        break;

    default: break;
    }

    return QRect(position, newSize);
}

void MainWindow::resizeWindow()
{
    setGeometry(windowRect());
    updateViewStruts();
    emit resizingFished();
}

void MainWindow::initScreens()
{
    setScreen(qGuiApp->primaryScreen());

    if (m_fakeWindow) {
        m_fakeWindow->setScreen(screen());
        m_fakeWindow->updateGeometry();
    }
}

void MainWindow::initSlideWindow()
{
    KWindowEffects::SlideFromLocation location = KWindowEffects::NoEdge;

    switch (m_settings->direction()) {
    case DockSettings::Left:   location = KWindowEffects::LeftEdge;   break;
    case DockSettings::Right:  location = KWindowEffects::RightEdge;  break;
    case DockSettings::Bottom: location = KWindowEffects::BottomEdge; break;
    default: break;
    }

    // KF6：用 QWindow* 重载，Wayland 可用
    KWindowEffects::slideWindow(this, location);
}

void MainWindow::updateViewStruts()
{
    if (m_settings->visibility() == DockSettings::AlwaysShow || m_activity->launchPad()) {
        const QRect r = windowRect();
        int exclusiveSize = 0;

        // DockSettings 只有 Left / Bottom / Right 三个方向，没有 Top
        switch (m_settings->direction()) {
        case DockSettings::Left:
        case DockSettings::Right:
            exclusiveSize = r.width();
            break;
        case DockSettings::Bottom:
        default:
            exclusiveSize = r.height();
            break;
        }

        XWindowInterface::instance()->setViewStruts(this, exclusiveSize);
    } else {
        clearViewStruts();
    }
}

void MainWindow::clearViewStruts()
{
    XWindowInterface::instance()->clearViewStruts(this);
}

void MainWindow::createFakeWindow()
{
    if (!m_fakeWindow) {
        m_fakeWindow = new FakeWindow;
        m_fakeWindow->setScreen(screen());
        m_fakeWindow->updateGeometry();

        connect(m_fakeWindow, &FakeWindow::containsMouseChanged, this, [this](bool contains) {
            switch (m_settings->visibility()) {
            case DockSettings::AlwaysHide:
            case DockSettings::IntellHide:
                if (contains) {
                    m_hideTimer->stop();
                    if (!isVisible() && !m_showTimer->isActive())
                        m_showTimer->start();
                } else {
                    if (!m_hideBlocked)
                        m_hideTimer->start();
                }
                break;
            default:
                break;
            }
        });
    }
}

void MainWindow::deleteFakeWindow()
{
    if (m_fakeWindow) {
        disconnect(m_fakeWindow);
        m_fakeWindow->deleteLater();
        m_fakeWindow = nullptr;
    }
}

void MainWindow::onPrimaryScreenChanged(QScreen *screen)
{
    initScreens();
    setScreen(screen);
    resizeWindow();
}

void MainWindow::onPositionChanged()
{
    initScreens();

    const bool hiding = (m_settings->visibility() == DockSettings::AlwaysHide ||
                         m_settings->visibility() == DockSettings::IntellHide);

    setVisible(false);
    initSlideWindow();
    if (!hiding) setVisible(true);
    setGeometry(windowRect());
    updateViewStruts();
    if (hiding) m_hideTimer->start();

    emit directionChanged();
}

void MainWindow::onIconSizeChanged()
{
    setGeometry(windowRect());
    updateViewStruts();
    emit iconSizeChanged();
}

void MainWindow::onVisibilityChanged()
{
    emit visibilityChanged();

    if (m_activity->launchPad()) {
        m_hideTimer->stop();
        clearViewStruts();
        setVisible(true);
        return;
    }

    if (m_settings->visibility() == DockSettings::AlwaysShow) {
        m_hideTimer->stop();
        setGeometry(windowRect());
        setVisible(true);
        updateViewStruts();
        if (m_fakeWindow)
            deleteFakeWindow();
        return;
    }

    if (m_settings->visibility() == DockSettings::IntellHide) {
        clearViewStruts();
        setGeometry(windowRect());
        setVisible(!m_activity->existsWindowMaximized() || m_hideBlocked);
        if (!m_fakeWindow)
            createFakeWindow();
        return;
    }

    if (m_settings->visibility() == DockSettings::AlwaysHide) {
        clearViewStruts();
        setGeometry(windowRect());
        setVisible(m_hideBlocked);
        if (!m_fakeWindow)
            createFakeWindow();
    }
}

void MainWindow::onHideTimeout()
{
    if (m_activity->launchPad())
        return;
    if (m_settings->visibility() == DockSettings::IntellHide
            && !m_activity->existsWindowMaximized())
        return;
    setVisible(false);
}

bool MainWindow::eventFilter(QObject *obj, QEvent *e)
{
    switch (e->type()) {
    case QEvent::Enter:
        if (m_fakeWindow) m_hideTimer->stop();
        m_hideBlocked = true;
        break;
    case QEvent::Leave:
        if (m_fakeWindow) m_hideTimer->start();
        m_hideBlocked = false;
        break;
    case QEvent::DragEnter:
    case QEvent::DragMove:
        if (m_fakeWindow) m_hideTimer->stop();
        break;
    case QEvent::DragLeave:
    case QEvent::Drop:
        if (m_fakeWindow) m_hideTimer->stop();
        break;
    default:
        break;
    }
    return QQuickView::eventFilter(obj, e);
}

void MainWindow::resizeEvent(QResizeEvent *e)
{
    emit primaryGeometryChanged();
    QQuickView::resizeEvent(e);
}
