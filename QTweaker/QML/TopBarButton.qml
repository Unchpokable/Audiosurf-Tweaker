import QtQuick 2.15

Rectangle {
    id: root

    property int buttonSize: 100
    property string buttonText: "Button"

    property bool hovered: mouseArea.containsMouse
    property bool pressed: mouseArea.pressed

    signal clicked()

    width: buttonSize
    height: buttonSize

    color: "transparent"

    Rectangle {
        id: gradientBackground
        anchors.fill: parent

        gradient: Gradient {
            GradientStop {
                position: 0.0
                color: pressed ? "#5022ba" : "#8b5cf6"
            }
            GradientStop {
                position: 1.0
                color: pressed ? "#4c00bdbd" : "#4c00eded"
            }
        }

        opacity: 0
    }

    Text {
        id: textLabel
        anchors.centerIn: parent
        text: buttonText
        color: "#555555"
        font.pixelSize: Math.max(12, buttonSize * 0.15)
    }

    Rectangle {
        id: marker
        width: parent.width * 0.6
        height: 2
        color: "#8b5cf6"
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 4
        opacity: 1
    }

    MouseArea {
        id: mouseArea
        anchors.fill: parent
        hoverEnabled: true

        onClicked: root.clicked()
    }

    states: [
        State {
            name: "idle"
            when: !hovered && !pressed
            PropertyChanges {
                target: gradientBackground
                opacity: 0
            }
            PropertyChanges {
                target: textLabel
                color: "#555555"
            }
            PropertyChanges {
                target: marker
                opacity: 1
            }
        },
        State {
            name: "hovered"
            when: hovered && !pressed
            PropertyChanges {
                target: gradientBackground
                opacity: 1
            }
            PropertyChanges {
                target: textLabel
                color: "#ffffff"
            }
            PropertyChanges {
                target: marker
                opacity: 0
            }
        },
        State {
            name: "pressed"
            when: pressed
            PropertyChanges {
                target: gradientBackground
                opacity: 1
            }
            PropertyChanges {
                target: textLabel
                color: "#ffffff"
            }
            PropertyChanges {
                target: marker
                opacity: 0
            }
        }
    ]

    transitions: [
        Transition {
            from: "idle"
            to: "hovered"
            ParallelAnimation {
                NumberAnimation {
                    target: gradientBackground
                    properties: "opacity"
                    duration: 250
                    easing.type: Easing.OutCubic
                }
                ColorAnimation {
                    target: textLabel
                    properties: "color"
                    duration: 250
                    easing.type: Easing.OutCubic
                }
                NumberAnimation {
                    target: marker
                    properties: "opacity"
                    duration: 250
                    easing.type: Easing.OutCubic
                }
            }
        },

        Transition {
            from: "hovered"
            to: "idle"
            ParallelAnimation {
                NumberAnimation {
                    target: gradientBackground
                    properties: "opacity"
                    duration: 250
                    easing.type: Easing.OutCubic
                }
                ColorAnimation {
                    target: textLabel
                    properties: "color"
                    duration: 250
                    easing.type: Easing.OutCubic
                }
                NumberAnimation {
                    target: marker
                    properties: "opacity"
                    duration: 250
                    easing.type: Easing.OutCubic
                }
            }
        },

        Transition {
            to: "pressed"
            ParallelAnimation {
                NumberAnimation {
                    target: gradientBackground
                    properties: "opacity"
                    duration: 180
                    easing.type: Easing.InOutQuad
                }
                ColorAnimation {
                    target: textLabel
                    properties: "color"
                    duration: 180
                    easing.type: Easing.InOutQuad
                }
                NumberAnimation {
                    target: marker
                    properties: "opacity"
                    duration: 180
                    easing.type: Easing.InOutQuad
                }
            }
        },

        Transition {
            from: "pressed"
            ParallelAnimation {
                NumberAnimation {
                    target: gradientBackground
                    properties: "opacity"
                    duration: 180
                    easing.type: Easing.InOutQuad
                }
                ColorAnimation {
                    target: textLabel
                    properties: "color"
                    duration: 180
                    easing.type: Easing.InOutQuad
                }
                NumberAnimation {
                    target: marker
                    properties: "opacity"
                    duration: 180
                    easing.type: Easing.InOutQuad
                }
            }
        }
    ]
}
