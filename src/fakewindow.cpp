/*
 * Copyright (C) 2021 CutefishOS Team.
 *
 * Author:     rekols <revenmartin@gmail.com>
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

#include "fakewindow.h"
#include "docksettings.h"

#include <QApplication>
#include <QScreen>
#include <QTimer>
#include <QDebug>

// LayerShellQt：让 FakeWindow 也成为 layer-shell 表面，
// 这样它能贴在屏幕边缘感知鼠标进入，不会被其他窗口遮挡
#include <LayerShellQt/Window>

FakeWindow::FakeWindow(QQuickView *parent)
    : QQuickView(parent)
    , m_delayedContainsMouse(false)
    , m_containsMouse(false)
{
    setColor(Qt::transparent);
    setDefaultAlphaBuffer(true);
    setFlags(Qt::FramelessWindowHint |
             Qt::WindowStaysOnTopHint |
             Qt::NoDropShadowWindowHint |
             Qt::WindowDoesNotAcceptFocus);
    show();

    m_delayedMouseTimer.setSingleShot(true);
    m_delayedMouseTimer.setInterval(50);
    connect(&m_delayedMouseTimer, &QTimer::timeout, this, [this]() {
        setContainsMouse(m_delayedContainsMouse);
    });

    connect(DockSettings::self(), &DockSettings::directionChanged,
            this, &FakeWindow::updateGeometry);
}

bool FakeWindow::containsMouse() const
{
    return m_containsMouse;
}

bool FakeWindow::event(QEvent *e)
{
    if (e->type() == QEvent::Show) {
        // Wayland 下用 LayerShellQt 让 FakeWindow 贴边，代替 NET::SkipTaskbar 等 X11 标志
        if (auto *lsw = LayerShellQt::Window::get(this)) {
            lsw->setLayer(LayerShellQt::Window::LayerTop);

            // FakeWindow 跟随主 Dock 方向贴边
            LayerShellQt::Window::Anchors anchors;
            switch (DockSettings::self()->direction()) {
            case DockSettings::Left:
                anchors.setFlag(LayerShellQt::Window::AnchorLeft);
                break;
            case DockSettings::Right:
                anchors.setFlag(LayerShellQt::Window::AnchorRight);
                break;
            case DockSettings::Bottom:
            default:
                anchors.setFlag(LayerShellQt::Window::AnchorBottom);
                break;
            }
            lsw->setAnchors(anchors);

            // exclusive zone = 0：不占用空间，只感知鼠标
            lsw->setExclusiveZone(0);
            lsw->setKeyboardInteractivity(LayerShellQt::Window::KeyboardInteractivityNone);
        }

    } else if (e->type() == QEvent::DragEnter || e->type() == QEvent::DragMove) {
        if (!m_containsMouse) {
            m_delayedContainsMouse = false;
            m_delayedMouseTimer.stop();
            setContainsMouse(true);
            emit dragEntered();
        }
    } else if (e->type() == QEvent::Enter) {
        m_delayedContainsMouse = true;
        if (!m_delayedMouseTimer.isActive())
            m_delayedMouseTimer.start();
    } else if (e->type() == QEvent::Leave || e->type() == QEvent::DragLeave) {
        m_delayedContainsMouse = false;
        if (!m_delayedMouseTimer.isActive())
            m_delayedMouseTimer.start();
    }

    return QQuickView::event(e);
}

void FakeWindow::setContainsMouse(bool contains)
{
    if (m_containsMouse != contains) {
        m_containsMouse = contains;
        emit containsMouseChanged(contains);
    }
}

void FakeWindow::updateGeometry()
{
    const int length = 5;
    const QRect screenRect = qApp->primaryScreen()->geometry();
    QRect newRect;

    switch (DockSettings::self()->direction()) {
    case DockSettings::Left:
        newRect = QRect(screenRect.x(),
                        screenRect.y(),
                        length, screenRect.height());
        break;
    case DockSettings::Right:
        newRect = QRect(screenRect.x() + screenRect.width() - length,
                        screenRect.y(),
                        length, screenRect.height());
        break;
    case DockSettings::Bottom:
    default:
        newRect = QRect(screenRect.x(),
                        screenRect.y() + screenRect.height() - length,
                        screenRect.width(), length);
        break;
    }

    setGeometry(newRect);
}
