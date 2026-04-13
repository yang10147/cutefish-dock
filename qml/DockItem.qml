/*
 * Copyright (C) 2021 CutefishOS Team.
 * Ported to Qt6 / Wayland (no FishUI dependency)
 */

import QtQuick
import QtQuick.Controls
import Cutefish.Dock 1.0

Item {
    id: control

    property bool isLeft:   Settings.direction === DockSettings.Left
    property bool isRight:  Settings.direction === DockSettings.Right
    property bool isBottom: Settings.direction === DockSettings.Bottom

    property var iconSize: root.isHorizontal ? control.height * iconSizeRatio
                                             : control.width  * iconSizeRatio

    property bool draggable:    false
    property int  dragItemIndex

    property alias icon:      icon
    property alias mouseArea: iconArea
    property alias dropArea:  iconDropArea

    property bool enableActivateDot: true
    property bool isActive:  false

    property var  popupText
    property double iconSizeRatio: 0.8
    property var  iconName

    property bool dragStarted: false

    // Spacing used for popupTips positioning (replaces FishUI.Units)
    readonly property int smallSpacing: 4
    readonly property int largeSpacing: 12

    signal positionChanged()
    signal released()
    signal pressed(var mouse)
    signal pressAndHold(var mouse)
    signal clicked(var mouse)
    signal rightClicked(var mouse)
    signal doubleClicked(var mouse)
    signal dropped(var drop)

    Drag.active:           mouseArea.drag.active && control.draggable
    Drag.dragType:         Drag.Automatic
    Drag.supportedActions: Qt.MoveAction
    Drag.hotSpot.x:        icon.width  / 2
    Drag.hotSpot.y:        icon.height / 2

    Drag.onDragStarted:  { dragStarted = true  }
    Drag.onDragFinished: { dragStarted = false }

    // Icon
    Image {
        id: icon
        anchors.centerIn: parent
        width:  control.iconSize
        height: control.iconSize
        source: control.iconName ? "image://icontheme/" + control.iconName : ""
        sourceSize: Qt.size(width, height)
        fillMode: Image.PreserveAspectFit
        visible: !dragStarted
        smooth: true
        asynchronous: true

        // Dim on press (replaces ColorOverlay)
        Rectangle {
            anchors.fill: parent
            color: "#000000"
            opacity: iconArea.pressed && !mouseArea.drag.active ? 0.35 : 0
            radius: 4
        }
    }

    DropArea {
        id: iconDropArea
        anchors.fill: icon
        enabled: draggable
        onDropped: control.dropped(drop)
    }

    MouseArea {
        id: iconArea
        anchors.fill: icon
        hoverEnabled: true
        acceptedButtons: Qt.LeftButton | Qt.RightButton | Qt.MiddleButton
        drag.axis: Drag.XAndYAxis

        onClicked: (mouse) => {
            if (mouse.button === Qt.RightButton)
                control.rightClicked(mouse)
            else
                control.clicked(mouse)
        }

        onPressed: (mouse) => {
            control.pressed(mouse)
            popupTips.hide()
        }

        onPositionChanged: (mouse) => {
            if (pressed) {
                if (control.draggable && mouse.source !== Qt.MouseEventSynthesizedByQt) {
                    drag.target = icon
                    icon.grabToImage(function(result) {
                        control.Drag.imageSource = result.url
                    })
                } else {
                    drag.target = null
                }
            }
            control.positionChanged()
        }

        onPressAndHold: (mouse) => control.pressAndHold(mouse)

        onReleased: {
            drag.target = null
            control.released()
        }

        onContainsMouseChanged: {
            if (containsMouse && control.popupText !== "") {
                popupTips.popupText = control.popupText

                if (Settings.direction === DockSettings.Left)
                    popupTips.position = Qt.point(
                        root.width + control.largeSpacing,
                        control.mapToGlobal(0, 0).y + (control.height / 2 - popupTips.height / 2))
                else if (Settings.direction === DockSettings.Right)
                    popupTips.position = Qt.point(
                        control.mapToGlobal(0, 0).x - popupTips.width - control.smallSpacing / 2,
                        control.mapToGlobal(0, 0).y + (control.height / 2 - popupTips.height / 2))
                else
                    popupTips.position = Qt.point(
                        control.mapToGlobal(0, 0).x + (control.width / 2 - popupTips.width / 2),
                        control.mapToGlobal(0, 0).y - popupTips.height - control.smallSpacing)

                popupTips.show()
            } else {
                popupTips.hide()
            }
        }
    }

    // Active indicator dot
    Rectangle {
        id: activeRect

        property var circleSize:  isBottom ? control.height * 0.06 : control.width  * 0.06
        property var activeLength: control.height * 0.5

        width:   !isBottom ? circleSize : (isActive ? activeLength : circleSize)
        height:  !isBottom ? (isActive ? activeLength : circleSize) : circleSize
        radius:  !isBottom ? width / 2 : height / 2
        visible: enableActivateDot && !dragStarted
        color:   "#888888"

        x: isLeft   ? 2
         : isBottom ? (parent.width  - width)  / 2
         :             icon.x + icon.width + activeRect.width / 2 - 1

        y: isLeft   ? (parent.height - height) / 2
         : isBottom ? icon.y + icon.height + activeRect.height / 2 - 1
         :            (parent.height - height) / 2

        Behavior on width  { NumberAnimation { duration: isBottom  ? 250 : 0; easing.type: Easing.InOutSine } }
        Behavior on height { NumberAnimation { duration: !isBottom ? 250 : 0; easing.type: Easing.InOutSine } }
    }

    // Simple popup tooltip (replaces FishUI.PopupTips calls)
    // popupTips is defined in main.qml and accessed via id
}
