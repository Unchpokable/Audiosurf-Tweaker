import QtQuick 2.15
import QtQuick.Controls 2.15

RadioButton {
    id: control

    width: parent.width

    property color idleTextColor: "#4a4a4a"
    property color hoveredTextColor: "#7a6bb5"
    property color checkedTextColor: "#e0e0e0"
    property color indicatorColor: "#8b5cf6"

    indicator: Item {}

    background: Rectangle {
        id: backgroundRect
        color: "transparent"

        Rectangle {
            id: verticalIndicator
            width: 4
            color: control.indicatorColor
            anchors {
                left: parent.left
                leftMargin: 8
                verticalCenter: parent.verticalCenter
            }

            radius: 2;
            height: parent.height * 0.2
        }

        Rectangle {
            id: checkedBackground
            anchors.fill: parent
            opacity: 0.0

            gradient: Gradient {
                orientation: Gradient.Horizontal
                GradientStop {
                    position: 0.0
                    color: "#8b5cf6"
                }
                GradientStop {
                    position: 1.0
                    color: "#06b6d4"
                }
            }
        }
    }

    contentItem: Text {
        id: textItem
        text: control.text
        font: control.font
        color: control.idleTextColor

        anchors {
            left: parent.left
            leftMargin: 20
            right: parent.right
            rightMargin: 10
            verticalCenter: parent.verticalCenter
        }
    }

    MouseArea {
        anchors.fill: parent
        cursorShape: control.hovered ? Qt.PointingHandCursor : Qt.ArrowCursor
        onClicked: control.toggle()
    }

    states: [
        State {
            name: "idle"
            when: !control.hovered && !control.checked
            PropertyChanges { target: verticalIndicator; height: control.height * 0.2 }
            PropertyChanges { target: textItem; color: control.idleTextColor }
            PropertyChanges { target: checkedBackground; opacity: 0.0 }
        },
        State {
            name: "hovered"
            when: control.hovered && !control.checked
            PropertyChanges { target: verticalIndicator; height: control.height * 0.6 }
            PropertyChanges { target: textItem; color: control.hoveredTextColor }
            PropertyChanges { target: checkedBackground; opacity: 0.0 }
        },
        State {
            name: "checked"
            when: control.checked
            PropertyChanges { target: verticalIndicator; height: control.height * 0.6 }
            PropertyChanges { target: textItem; color: control.checkedTextColor }
            PropertyChanges { target: checkedBackground; opacity: 1.0 }
        }
    ]

    transitions: [
        Transition {
            from: "idle"
            to: "hovered"
            NumberAnimation {
                target: verticalIndicator
                property: "height"
                duration: 100
                easing.type: Easing.OutCubic
            }
            ColorAnimation {
                target: textItem
                property: "color"
                duration: 100
                easing.type: Easing.OutCubic
            }
        },
        Transition {
            from: "hovered"
            to: "idle"
            NumberAnimation {
                target: verticalIndicator
                property: "height"
                duration: 100
                easing.type: Easing.OutCubic
            }
            ColorAnimation {
                target: textItem
                property: "color"
                duration: 100
                easing.type: Easing.OutCubic
            }
        },
        Transition {
            to: "checked"
            ParallelAnimation {
                NumberAnimation {
                    target: verticalIndicator
                    property: "height"
                    duration: 300
                    easing.type: Easing.OutCubic
                }
                ColorAnimation {
                    target: textItem
                    property: "color"
                    duration: 300
                    easing.type: Easing.OutCubic
                }
                NumberAnimation {
                    target: checkedBackground
                    property: "opacity"
                    duration: 300
                    easing.type: Easing.OutCubic
                }
            }
        },
        Transition {
            from: "checked"
            ParallelAnimation {
                NumberAnimation {
                    target: verticalIndicator
                    property: "height"
                    duration: 300
                    easing.type: Easing.OutCubic
                }
                ColorAnimation {
                    target: textItem
                    property: "color"
                    duration: 300
                    easing.type: Easing.OutCubic
                }
                NumberAnimation {
                    target: checkedBackground
                    property: "opacity"
                    duration: 300
                    easing.type: Easing.OutCubic
                }
            }
        }
    ]
}
