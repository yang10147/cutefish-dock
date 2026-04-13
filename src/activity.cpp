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

#include "activity.h"
#include "docksettings.h"

// KF6 Wayland 下，KWindowSystem 已移除所有跨进程窗口枚举 API：
//   activeWindowChanged / windowChanged / windows() / activeWindow()
// 这些都是 X11 Only，Wayland 安全模型不允许普通应用枚举其他进程的窗口。
// Activity 在此阶段降级为静态默认值，后续通过 PlasmaWindowManagement 协议补全。

static Activity *SELF = nullptr;

Activity *Activity::self()
{
    if (!SELF)
        SELF = new Activity;
    return SELF;
}

Activity::Activity(QObject *parent)
    : QObject(parent)
    , m_existsWindowMaximized(false)
    , m_launchPad(false)
    , m_pid(0)
{
    // Wayland 下暂无公开 API 监听窗口变化，保持默认值
    // existsWindowMaximized = false → IntellHide 模式下 Dock 始终显示
    // launchPad = false → 不触发 launchpad 特殊逻辑
}

bool Activity::existsWindowMaximized() const
{
    return m_existsWindowMaximized;
}

bool Activity::launchPad() const
{
    return m_launchPad;
}

void Activity::onActiveWindowChanged()
{
    // Wayland 下无法获取活动窗口信息，此槽为空
}
