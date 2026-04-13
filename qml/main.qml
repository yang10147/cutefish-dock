/*
 * Copyright (C) 2021 CutefishOS Team.
 * Ported to Qt6 / Wayland (no FishUI dependency)
 */

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Cutefish.Dock 1.0

Item {
    id: root
    visible: true

    property bool isHorizontal: Settings.direction === DockSettings.Bottom

    // 用系统颜色方案简单判断深色模式
    readonly property bool darkMode: Qt.styleHints.colorScheme === Qt.Dark

    DropArea {
        anchors.fill: parent
        enabled: true
    }

    // 背景
    Rectangle {
        id: _background
        anchors.fill: parent
        radius: Settings.style === 0 ? (isHorizontal ? root.height * 0.3 : root.width * 0.3) : 0
        color:   darkMode ? "#4D4D4D" : "#E6E6E6"
        opacity: 0.88

        border.width: 1
        border.color: darkMode ? Qt.rgba(1, 1, 1, 0.18) : Qt.rgba(0, 0, 0, 0.15)

        Behavior on color {
            ColorAnimation { duration: 200; easing.type: Easing.Linear }
        }
    }

    // 简单的 Tooltip popup（替代 FishUI.PopupTips）
    Rectangle {
        id: popupTips
        visible: false
        z: 999

        property string popupText: ""
        property point position: Qt.point(0, 0)

        x: position.x
        y: position.y

        width:  tipLabel.implicitWidth  + 16
        height: tipLabel.implicitHeight + 8
        radius: 4
        color:  darkMode ? "#333333" : "#F0F0F0"
        border.color: darkMode ? Qt.rgba(1,1,1,0.15) : Qt.rgba(0,0,0,0.12)
        border.width: 1

        Text {
            id: tipLabel
            anchors.centerIn: parent
            text:  popupTips.popupText
            color: darkMode ? "#FFFFFF" : "#1A1A1A"
            font.pixelSize: 12
        }

        function show() { visible = true  }
        function hide() { visible = false }
    }

    GridLayout {
        id: mainLayout
        anchors.fill: parent
        anchors.topMargin: Settings.style === 1
                           && (Settings.direction === DockSettings.Left
                               || Settings.direction === DockSettings.Right)
                           ? 28 : 0
        flow:          isHorizontal ? Grid.LeftToRight : Grid.TopToBottom
        columnSpacing: 0
        rowSpacing:    0

        ListView {
            id: appItemView
            orientation:  isHorizontal ? Qt.Horizontal : Qt.Vertical
            snapMode:     ListView.SnapToItem
            interactive:  false
            model:        appModel
            clip:         true

            Layout.fillHeight: true
            Layout.fillWidth:  true

            delegate: AppItem {
                id: appItemDelegate
                implicitWidth:  isHorizontal ? appItemView.height : appItemView.width
                implicitHeight: isHorizontal ? appItemView.height : appItemView.width
            }

            moveDisplaced: Transition {
                NumberAnimation {
                    properties: "x, y"
                    duration:   300
                    easing.type: Easing.InOutQuad
                }
            }
        }

        // 垃圾桶
        DockItem {
            id: trashItem
            implicitWidth:     isHorizontal ? root.height : root.width
            implicitHeight:    isHorizontal ? root.height : root.width
            popupText:         qsTr("Trash")
            enableActivateDot: false
            iconName:          trash.count === 0 ? "user-trash-empty" : "user-trash-full"
            onClicked:         trash.openTrash()
            onRightClicked:    trashMenu.popup()

            dropArea.enabled: true
            onDropped: (drop) => {
                if (drop.hasUrls)
                    trash.moveToTrash(drop.urls)
            }

            // 拖拽高亮边框
            Rectangle {
                anchors.fill:    parent
                anchors.margins: 3
                color:           "transparent"
                border.color:    darkMode ? "#FFFFFF" : "#1A1A1A"
                radius:          height * 0.3
                border.width:    1
                opacity:         trashItem.dropArea.containsDrag ? 0.5 : 0
                Behavior on opacity { NumberAnimation { duration: 200 } }
            }

            Menu {
                id: trashMenu
                MenuItem {
                    text: qsTr("Open")
                    onTriggered: trash.openTrash()
                }
                MenuItem {
                    text:    qsTr("Empty Trash")
                    visible: trash.count !== 0
                    onTriggered: trash.emptyTrash()
                }
            }
        }
    }

    Connections {
        target: Settings
        function onDirectionChanged() { popupTips.hide() }
    }

    Connections {
        target: mainWindow
        function onVisibleChanged() { popupTips.hide() }
    }
}
