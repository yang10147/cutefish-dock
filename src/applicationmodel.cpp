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

#include "applicationmodel.h"
#include "processprovider.h"
#include "utils.h"
#include <QDir>
#include <QFile>

#include <QProcess>

// resourceClass → item->id 的别名映射（处理 pinned app id 与 resourceClass 不同的情况）
static QMap<QString, QString> s_classToId;

ApplicationModel::ApplicationModel(QObject *parent)
    : QAbstractListModel(parent)
    , m_iface(XWindowInterface::instance())
    , m_sysAppMonitor(SystemAppMonitor::self())
{
    // Wayland: 使用 KWin 脚本 DBus 桥接，基于 resourceClass
    connect(m_iface, &XWindowInterface::windowAddedByClass, this, &ApplicationModel::onWindowAddedByClass);
    connect(m_iface, &XWindowInterface::windowRemovedByClass, this, &ApplicationModel::onWindowRemovedByClass);
    connect(m_iface, &XWindowInterface::windowActivatedByClass, this, &ApplicationModel::onWindowActivatedByClass);

    initPinnedApplications();

    QTimer::singleShot(100, m_iface, &XWindowInterface::startInitWindows);
}

int ApplicationModel::rowCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent)

    return m_appItems.size();
}

QHash<int, QByteArray> ApplicationModel::roleNames() const
{
    QHash<int, QByteArray> roles;
    roles[AppIdRole] = "appId";
    roles[IconNameRole] = "iconName";
    roles[VisibleNameRole] = "visibleName";
    roles[ActiveRole] = "isActive";
    roles[WindowCountRole] = "windowCount";
    roles[IsPinnedRole] = "isPinned";
    roles[DesktopFileRole] = "desktopFile";
    roles[FixedItemRole] = "fixed";
    return roles;
}

QVariant ApplicationModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid())
        return QVariant();

    ApplicationItem *item = m_appItems.at(index.row());

    switch (role) {
    case AppIdRole:
        return item->id;
    case IconNameRole:
        return item->iconName;
    case VisibleNameRole:
        return item->visibleName;
    case ActiveRole:
        return item->isActive;
    case WindowCountRole:
        return item->wids.count();
    case IsPinnedRole:
        return item->isPinned;
    case DesktopFileRole:
        return item->desktopPath;
    case FixedItemRole:
        return item->fixed;
    default:
        return QVariant();
    }

    return QVariant();
}

void ApplicationModel::addItem(const QString &desktopFile)
{
    ApplicationItem *existsItem = findItemByDesktop(desktopFile);

    if (existsItem) {
        existsItem->isPinned = true;
        return;
    }

    beginInsertRows(QModelIndex(), rowCount(), rowCount());
    ApplicationItem *item = new ApplicationItem;
    QMap<QString, QString> desktopInfo = Utils::instance()->readInfoFromDesktop(desktopFile);
    item->iconName = desktopInfo.value("Icon");
    item->visibleName = desktopInfo.value("Name");
    item->exec = desktopInfo.value("Exec");
    item->desktopPath = desktopFile;
    item->isPinned = true;

    // First use filename as the id of the item.
    // Why not use exec? Because exec contains the file path,
    // QSettings will have problems, resulting in unrecognized next time.
    QFileInfo fi(desktopFile);
    item->id = fi.baseName();

    m_appItems << item;
    endInsertRows();

    savePinAndUnPinList();

    emit itemAdded();
    emit countChanged();
}

void ApplicationModel::removeItem(const QString &desktopFile)
{
    ApplicationItem *item = findItemByDesktop(desktopFile);

    if (item) {
        ApplicationModel::unPin(item->id);
    }
}

bool ApplicationModel::desktopContains(const QString &desktopFile)
{
    if (desktopFile.isEmpty())
        return false;

    return findItemByDesktop(desktopFile) != nullptr;
}

bool ApplicationModel::isDesktopPinned(const QString &desktopFile)
{
    ApplicationItem *item = findItemByDesktop(desktopFile);

    if (item) {
        return item->isPinned;
    }

    return false;
}

void ApplicationModel::clicked(const QString &id)
{
    ApplicationItem *item = findItemById(id);

    if (!item)
        return;

    // Application Item that has been pinned,
    // We need to open it.
    if (item->wids.isEmpty()) {
        // open application
        openNewInstance(item->id);
    }
    // Wayland: 用 resourceClass 激活窗口
    else if (item->wids.count() > 1) {
        // 多窗口轮换（wids 存 pid，仅用于计数）
        item->currentActive++;
        if (item->currentActive == item->wids.count())
            item->currentActive = 0;
        m_iface->forceActiveWindowByClass(item->id);
    } else if (m_iface->activeWindowClass() == item->id ||
               s_classToId.value(m_iface->activeWindowClass()) == item->id) {
        // 已是活动窗口 → 最小化（考虑 resourceClass 与 id 的别名关系）
        m_iface->minimizeWindowByClass(item->id);
    } else {
        m_iface->forceActiveWindowByClass(item->id);
    }
}

void ApplicationModel::raiseWindow(const QString &id)
{
    ApplicationItem *item = findItemById(id);

    if (!item)
        return;

    m_iface->forceActiveWindowByClass(item->id);
}

bool ApplicationModel::openNewInstance(const QString &appId)
{
    ApplicationItem *item = findItemById(appId);

    if (!item)
        return false;

    if (!item->exec.isEmpty()) {
        QStringList args = item->exec.split(" ");
        QString exec = args.first();
        args.removeFirst();

        if (!args.isEmpty()) {
            ProcessProvider::startDetached(exec, args);
        } else {
            ProcessProvider::startDetached(exec);
        }
    } else {
        ProcessProvider::startDetached(appId);
    }

    return true;
}

void ApplicationModel::closeAllByAppId(const QString &appId)
{
    ApplicationItem *item = findItemById(appId);

    if (!item)
        return;

    for (quint64 wid : item->wids) {
        m_iface->closeWindow(wid);
    }
}

void ApplicationModel::pin(const QString &appId)
{
    ApplicationItem *item = findItemById(appId);

    if (!item)
        return;

    item->isPinned = true;

    handleDataChangedFromItem(item);
    savePinAndUnPinList();
}

void ApplicationModel::unPin(const QString &appId)
{
    ApplicationItem *item = findItemById(appId);

    if (!item)
        return;

    item->isPinned = false;
    handleDataChangedFromItem(item);

    // Need to be removed after unpin
    if (item->wids.isEmpty()) {
        int index = indexOf(item->id);
        if (index != -1) {
            beginRemoveRows(QModelIndex(), index, index);
            m_appItems.removeAll(item);
            endRemoveRows();

            emit itemRemoved();
            emit countChanged();
        }
    }

    savePinAndUnPinList();
}

void ApplicationModel::updateGeometries(const QString &id, QRect rect)
{
    ApplicationItem *item = findItemById(id);

    // If not found
    if (!item)
        return;

    for (quint64 id : item->wids) {
        m_iface->setIconGeometry(id, rect);
    }
}

void ApplicationModel::move(int from, int to)
{
    if (from == to)
        return;

    m_appItems.move(from, to);

    if (from < to)
        beginMoveRows(QModelIndex(), from, from, QModelIndex(), to + 1);
    else
        beginMoveRows(QModelIndex(), from, from, QModelIndex(), to);

    endMoveRows();
}

ApplicationItem *ApplicationModel::findItemByWId(quint64 wid)
{
    for (ApplicationItem *item : m_appItems) {
        for (quint64 winId : item->wids) {
            if (winId == wid)
                return item;
        }
    }

    return nullptr;
}

ApplicationItem *ApplicationModel::findItemById(const QString &id)
{
    for (ApplicationItem *item : m_appItems) {
        if (item->id == id)
            return item;
    }

    return nullptr;
}

ApplicationItem *ApplicationModel::findItemByDesktop(const QString &desktop)
{
    for (ApplicationItem *item : m_appItems) {
        if (item->desktopPath == desktop)
            return item;
    }

    return nullptr;
}

bool ApplicationModel::contains(const QString &id)
{
    for (ApplicationItem *item : qAsConst(m_appItems)) {
        if (item->id == id)
            return true;
    }

    return false;
}

int ApplicationModel::indexOf(const QString &id)
{
    for (ApplicationItem *item : m_appItems) {
        if (item->id == id)
            return m_appItems.indexOf(item);
    }

    return -1;
}

void ApplicationModel::initPinnedApplications()
{
    QSettings settings(QSettings::UserScope, "cutefishos", "dock_pinned");
    QSettings systemSettings("/etc/cutefish-dock-list.conf", QSettings::IniFormat);
    QSettings *set = (QFile(settings.fileName()).exists()) ? &settings
                                                           : &systemSettings;
    QStringList groups = set->childGroups();

    // Launcher
    ApplicationItem *item = new ApplicationItem;
    item->id = "cutefish-launcher";
    item->exec = "cutefish-launcher";
    item->iconName = "launcher";
    item->visibleName = tr("Launcher");
    item->fixed = true;
    m_appItems.append(item);

    // Pinned Apps
    for (int i = 0; i < groups.size(); ++i) {
        for (const QString &id : groups) {
            set->beginGroup(id);
            int index = set->value("Index").toInt();

            if (index == i) {
                beginInsertRows(QModelIndex(), rowCount(), rowCount());
                ApplicationItem *item = new ApplicationItem;

                item->desktopPath = set->value("DesktopPath").toString();
                item->id = id;
                item->isPinned = true;

                if (!QFile(item->desktopPath).exists()) {
                    set->endGroup();
                    continue;
                }

                // Read from desktop file.
                if (!item->desktopPath.isEmpty()) {
                    QMap<QString, QString> desktopInfo = Utils::instance()->readInfoFromDesktop(item->desktopPath);
                    item->iconName = desktopInfo.value("Icon");
                    item->visibleName = desktopInfo.value("Name");
                    item->exec = desktopInfo.value("Exec");
                }

                // Read from config file.
                if (item->iconName.isEmpty())
                    item->iconName = set->value("Icon").toString();

                if (item->visibleName.isEmpty())
                    item->visibleName = set->value("VisibleName").toString();

                if (item->exec.isEmpty())
                    item->exec = set->value("Exec").toString();

                m_appItems.append(item);
                endInsertRows();

                emit itemAdded();
                emit countChanged();

                set->endGroup();
                break;
            } else {
                set->endGroup();
            }
        }
    }
}

void ApplicationModel::savePinAndUnPinList()
{
    QSettings settings(QSettings::UserScope, "cutefishos", "dock_pinned");
    settings.clear();

    int index = 0;

    for (ApplicationItem *item : m_appItems) {
        if (item->isPinned) {
            settings.beginGroup(item->id);
            settings.setValue("Index", index);
            settings.setValue("Icon", item->iconName);
            settings.setValue("VisibleName", item->visibleName);
            settings.setValue("Exec", item->exec);
            settings.setValue("DesktopPath", item->desktopPath);
            settings.endGroup();
            ++index;
        }
    }

    settings.sync();
}

void ApplicationModel::handleDataChangedFromItem(ApplicationItem *item)
{
    if (!item)
        return;

    QModelIndex idx = index(indexOf(item->id), 0, QModelIndex());

    if (idx.isValid()) {
        emit dataChanged(idx, idx);
    }
}

void ApplicationModel::onWindowAdded(quint64 wid)
{
    // 旧 X11 接口，Wayland 下不使用
    Q_UNUSED(wid)
}

void ApplicationModel::onWindowRemoved(quint64 wid)
{
    Q_UNUSED(wid)
}

void ApplicationModel::onActiveChanged(quint64 wid)
{
    Q_UNUSED(wid)
}

// ── KWin 脚本 DBus 桥接 ───────────────────────────────────────────────────

// 根据 resourceClass 找到 desktop 文件路径
static QString findDesktopByClass(const QString &resourceClass)
{
    // 搜索常见 desktop 目录
    QStringList dirs = {
        "/usr/share/applications",
        QDir::homePath() + "/.local/share/applications",
        "/usr/local/share/applications",
    };
    // 先尝试精确匹配 <resourceClass>.desktop
    for (const QString &dir : dirs) {
        QString path = dir + "/" + resourceClass + ".desktop";
        if (QFile::exists(path)) return path;
        // org.kde.dolphin → dolphin.desktop
        QString simple = resourceClass.section('.', -1).toLower() + ".desktop";
        path = dir + "/" + simple;
        if (QFile::exists(path)) return path;
    }
    return {};
}

void ApplicationModel::onWindowAddedByClass(const QString &resourceClass, quint64 pid)
{
    // 过滤无效事件和系统窗口
    static const QStringList kIgnored = {
        "cutefish-launcher",
        "cutefish-dock",
        "cutefish-statusbar",
    };
    if (resourceClass.isEmpty() || pid <= 0 || kIgnored.contains(resourceClass))
        return;

    // 检查 resourceClass 是否已有别名映射到某个 item->id
    if (s_classToId.contains(resourceClass)) {
        ApplicationItem *item = findItemById(s_classToId.value(resourceClass));
        if (item) {
            if (!item->wids.contains(pid)) {
                item->wids.append(pid);
                handleDataChangedFromItem(item);
            }
            return;
        }
        s_classToId.remove(resourceClass);
    }

    // 已存在同名 item（resourceClass == item->id）则只追加 pid
    if (contains(resourceClass)) {
        ApplicationItem *item = findItemById(resourceClass);
        if (item && !item->wids.contains(pid)) {
            item->wids.append(pid);
            handleDataChangedFromItem(item);
        }
        return;
    }

    // 用 desktopPath 匹配：看是否有 pinned app 对应这个 resourceClass
    QString desktopPath = findDesktopByClass(resourceClass);
    ApplicationItem *desktopItem = findItemByDesktop(desktopPath);
    if (!desktopPath.isEmpty() && desktopItem) {
        // 建立别名映射，不改 item->id
        s_classToId.insert(resourceClass, desktopItem->id);
        if (!desktopItem->wids.contains(pid)) {
            desktopItem->wids.append(pid);
        }
        desktopItem->isActive = false;
        handleDataChangedFromItem(desktopItem);
        return;
    }

    // 全新的未 pinned 窗口，创建临时条目
    beginInsertRows(QModelIndex(), rowCount(), rowCount());
    ApplicationItem *item = new ApplicationItem;
    item->id = resourceClass;
    item->wids.append(pid);

    if (!desktopPath.isEmpty()) {
        QMap<QString, QString> info = Utils::instance()->readInfoFromDesktop(desktopPath);
        item->iconName = info.value("Icon");
        item->visibleName = info.value("Name");
        item->exec = info.value("Exec");
        item->desktopPath = desktopPath;
    } else {
        item->iconName = resourceClass.section('.', -1).toLower();
        item->visibleName = item->iconName;
    }

    m_appItems << item;
    endInsertRows();

    emit itemAdded();
    emit countChanged();
}

void ApplicationModel::onWindowRemovedByClass(const QString &resourceClass, quint64 pid)
{
    // 过滤无效事件和系统窗口（与 Added 保持一致）
    static const QStringList kIgnored = {
        "cutefish-launcher",
        "cutefish-dock",
        "cutefish-statusbar",
    };
    if (resourceClass.isEmpty() || pid <= 0 || kIgnored.contains(resourceClass))
        return;

    // 先查别名映射，再直接查 id
    QString itemId = s_classToId.value(resourceClass, resourceClass);
    ApplicationItem *item = findItemById(itemId);
    if (!item) return;

    // fixed item（launcher图标等）永远不移除
    if (item->fixed) return;

    item->wids.removeOne(pid);
    if (item->currentActive >= item->wids.size())
        item->currentActive = 0;
    if (item->isActive)
        item->isActive = false;

    handleDataChangedFromItem(item);

    if (item->wids.isEmpty()) {
        s_classToId.remove(resourceClass);

        if (!item->isPinned) {
            int idx = indexOf(item->id);
            if (idx == -1) return;
            beginRemoveRows(QModelIndex(), idx, idx);
            m_appItems.removeAll(item);
            endRemoveRows();
            emit itemRemoved();
            emit countChanged();
        }
    }
}

void ApplicationModel::onWindowActivatedByClass(const QString &resourceClass, quint64 pid)
{
    Q_UNUSED(pid)
    // 过滤系统窗口（launcher激活时不应影响dock图标高亮状态）
    static const QStringList kIgnored = {
        "cutefish-launcher",
        "cutefish-dock",
        "cutefish-statusbar",
    };
    if (kIgnored.contains(resourceClass)) return;

    // 空 resourceClass 表示无窗口激活，把所有 item 设为非激活
    if (resourceClass.isEmpty()) {
        for (ApplicationItem *item : m_appItems) {
            if (item->isActive) {
                item->isActive = false;
                QModelIndex idx = index(indexOf(item->id), 0, QModelIndex());
                if (idx.isValid()) emit dataChanged(idx, idx);
            }
        }
        return;
    }

    // 通过别名映射找到真正的 item->id
    QString activeId = s_classToId.value(resourceClass, resourceClass);
    for (ApplicationItem *item : m_appItems) {
        bool shouldBeActive = (item->id == activeId);
        if (item->isActive != shouldBeActive) {
            item->isActive = shouldBeActive;
            QModelIndex idx = index(indexOf(item->id), 0, QModelIndex());
            if (idx.isValid())
                emit dataChanged(idx, idx);
        }
    }
}
