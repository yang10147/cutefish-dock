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

// LayerShellQt::Shell 头文件保留，但 Qt 6.5+ 不需要手动调用 useLayerShell()
#include <LayerShellQt/Shell>

#include <QApplication>
#include <QQmlApplicationEngine>
#include <QQuickView>
#include <QTranslator>
#include <QLocale>
#include <QFile>
#include <QDBusConnection>

#include "applicationmodel.h"
#include "docksettings.h"
#include "mainwindow.h"
#include "xwindowinterface.h"
#include "dockadaptor.h"

int main(int argc, char *argv[])
{
    // Qt6 已内置高 DPI 支持，这两行废弃了，删掉
    // QCoreApplication::setAttribute(Qt::AA_EnableHighDpiScaling, true);
    // QCoreApplication::setAttribute(Qt::AA_UseHighDpiPixmaps, true);

    QApplication app(argc, argv);

    if (!QDBusConnection::sessionBus().registerService(QStringLiteral("com.cutefish.Dock"))) {
        return -1;
    }

    qmlRegisterType<DockSettings>(QStringLiteral("Cutefish.Dock").toLatin1(), 1, 0, "DockSettings");

    const QString qmFilePath = QStringLiteral("%1/%2.qm")
        .arg(QStringLiteral("/usr/share/cutefish-dock/translations/"))
        .arg(QLocale::system().name());

    if (QFile::exists(qmFilePath)) {
        QTranslator *translator = new QTranslator(QApplication::instance());
        if (translator->load(qmFilePath)) {
            QGuiApplication::installTranslator(translator);
        } else {
            translator->deleteLater();
        }
    }

    MainWindow w;

    // /Dock 挂 DockAdaptor（供 launcher 的 pinned/add/remove 调用）
    new DockAdaptor(&w);
    if (!QDBusConnection::sessionBus().registerObject(QStringLiteral("/Dock"), &w)) {
        return -1;
    }
    // /DockXWin 挂 XWindowInterface（供 KWin 脚本 DBus 调用）
    if (!QDBusConnection::sessionBus().registerObject(
            QStringLiteral("/DockXWin"),
            XWindowInterface::instance(),
            QDBusConnection::ExportAllSlots)) {
        qWarning("Failed to register /DockXWin object");
    }

    return app.exec();
}
