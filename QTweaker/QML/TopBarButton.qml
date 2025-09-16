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
        visible: hovered

        gradient: Gradient {
            GradientStop {
                position: 0.0
                color: pressed ? "#26008ba0" : "#4c00eded"
            }
            GradientStop {
                position: 1.0
                color: pressed ? "#7a2c7a" : "#9932cc"
            }
        }

        opacity: 0

        Behavior on opacity {
            NumberAnimation {
                duration: 200
                easing.type: Easing.OutCubic
            }
        }
    }

    Text {
        id: textLabel
        anchors.centerIn: parent
        text: buttonText
        color: hovered ? "#ffffff" : "#555555"
        font.pixelSize: Math.max(12, buttonSize * 0.15)

        Behavior on color {
            ColorAnimation {
                duration: 150
                easing.type: pressed ? Easing.InOutQuad : Easing.OutCubic
            }
        }
    }

    Rectangle {
        id: marker
        width: parent.width * 0.6
        height: 2
        color: "#9932cc"
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 4

        opacity: hovered ? 0 : 1

        Behavior on opacity {
            NumberAnimation {
                duration: 200
                easing.type: Easing.OutCubic
            }
        }
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
            PropertyChanges { target: gradientBackground; opacity: 0 }
        },
        State {
            name: "hovered"
            when: hovered && !pressed
            PropertyChanges { target: gradientBackground; opacity: 1 }
        },
        State {
            name: "pressed"
            when: pressed
            PropertyChanges { target: gradientBackground; opacity: 1 }
        }
    ]

    transitions: [
        Transition {
            from: "idle"; to: "hovered"
            NumberAnimation {
                properties: "opacity"
                duration: 200
                easing.type: Easing.OutCubic
            }
        },
        Transition {
            from: "hovered"; to: "idle"
            NumberAnimation {
                properties: "opacity"
                duration: 200
                easing.type: Easing.OutCubic
            }
        },
        Transition {
            to: "pressed"
            NumberAnimation {
                properties: "opacity"
                duration: 130
                easing.type: Easing.InOutQuad
            }
        },
        Transition {
            from: "pressed"
            NumberAnimation {
                properties: "opacity"
                duration: 130
                easing.type: Easing.InOutQuad
            }
        }
    ]
}
