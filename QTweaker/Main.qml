import QtQuick
import QtQuick.Controls;

import "QML"

Window {
    id: window
    flags: Qt.Window | Qt.FramelessWindowHint

    width: 1200
    height: 700
    visible: true

    Rectangle {
        id: titleBar
        anchors.top: parent.top;
        anchors.left: parent.left
        anchors.right: parent.right;

        height: 40;
        color: "transparent";

        Image {
            id: appIcon;

            anchors.left: parent.left;
            anchors.leftMargin: 8;
            anchors.verticalCenter: parent.verticalCenter;

            smooth: true;
            width: 32;
            height: 32;
            fillMode: Image.PreserveAspectFit;
            source: "qrc:/icons/Icon.png"
        }

        Text {
            anchors.left: appIcon.right;
            anchors.leftMargin: 10;
            anchors.verticalCenter: parent.verticalCenter;
            text: "Audiosurf Tweaker";
            font.family: "Tahoma";
            font.bold: true;
            font.pointSize: 14;
            color: "#2d2d2d";
        }

        DragHandler {
            id: handler;
            onActiveChanged: if(active) window.startSystemMove();
        }

        Row {
            anchors.right: parent.right;
            anchors.verticalCenter: parent.verticalCenter;

            TopBarButton {
                buttonSize: titleBar.height;
                buttonText: "_";
                onClicked: window.showMinimized()
            }
            TopBarButton {
                buttonSize: titleBar.height;
                buttonText: "□"
                onClicked: window.visibility === Window.Maximized ?
                          window.showNormal() : window.showMaximized()
            }
            TopBarButton {
                buttonSize: titleBar.height;
                buttonText: "×"
                onClicked: window.close()
            }
        }
    }

    Rectangle {
        id: leftMenu;

        color: "#4cacacac";

        anchors.top: titleBar.bottom;
        anchors.left: titleBar.left;
        anchors.bottom: parent.bottom;

        width: parent.width * 0.2;

        ButtonGroup {
            id: navMenu
        }

        Column {
            anchors.fill: parent

            SideMenuRadioButton {
                id: nav_SkinChanger;
                text: "Skin Changer";
                font.bold: true;
                font.pixelSize: 14;
                ButtonGroup.group: navMenu
            }

            SideMenuRadioButton {
                id: nav_Colors;
                text: "Colors";
                font.bold: true;
                font.pixelSize: 14;
                ButtonGroup.group: navMenu
            }

            SideMenuRadioButton {
                id: nav_Tweaker;
                text: "Tweaks";
                font.bold: true;
                font.pixelSize: 14;
                ButtonGroup.group: navMenu
            }
        }
    }
}
