import QtQuick 2.15
import QtQuick.Controls 2.15

RadioButton {
    id: control

    // Занимаем всю доступную ширину
    width: parent.width

    // Внешние свойства для кастомизации
    property color idleTextColor: "#4a4a4a"      // тёмно-серый
    property color hoveredTextColor: "#7a6bb5"   // серо-фиолетовый
    property color checkedTextColor: "#e0e0e0"   // светло-серый
    property color indicatorColor: "#8b5cf6"     // фиолетовый

    // Убираем стандартный indicator
    indicator: Item {}

    background: Rectangle {
        id: backgroundRect
        color: "transparent"

        // Индикатор - вертикальная черта
        Rectangle {
            id: verticalIndicator
            width: 2
            color: control.indicatorColor
            anchors {
                left: parent.left
                leftMargin: 8
                verticalCenter: parent.verticalCenter
            }
            height: parent.height * 0.1
        }

        // Градиентная заливка для checked состояния
        Rectangle {
            id: checkedBackground
            anchors.fill: parent
            opacity: 0.0

            gradient: Gradient {
                orientation: Gradient.Horizontal
                GradientStop {
                    position: 0.0
                    color: "#8b5cf6" // фиолетовый
                }
                GradientStop {
                    position: 1.0
                    color: "#06b6d4" // бирюзовый
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
            leftMargin: 20 // отступ от индикатора
            right: parent.right
            rightMargin: 10
            verticalCenter: parent.verticalCenter
        }
    }

    // Курсор
    MouseArea {
        anchors.fill: parent
        cursorShape: control.hovered ? Qt.PointingHandCursor : Qt.ArrowCursor
        onClicked: control.toggle()
    }

    // Состояния
    states: [
        State {
            name: "idle"
            when: !control.hovered && !control.checked
            PropertyChanges { target: verticalIndicator; height: control.height * 0.1 }
            PropertyChanges { target: textItem; color: control.idleTextColor }
            PropertyChanges { target: checkedBackground; opacity: 0.0 }
        },
        State {
            name: "hovered"
            when: control.hovered && !control.checked
            PropertyChanges { target: verticalIndicator; height: control.height * 0.5 }
            PropertyChanges { target: textItem; color: control.hoveredTextColor }
            PropertyChanges { target: checkedBackground; opacity: 0.0 }
        },
        State {
            name: "checked"
            when: control.checked
            PropertyChanges { target: verticalIndicator; height: control.height * 0.5 }
            PropertyChanges { target: textItem; color: control.checkedTextColor }
            PropertyChanges { target: checkedBackground; opacity: 1.0 }
        }
    ]

    // Переходы
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
