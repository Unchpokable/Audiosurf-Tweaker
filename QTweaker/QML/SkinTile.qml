import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import QtGraphicalEffects 1.15

Component {
    id: skinTileDelegate

    Rectangle {
        id: root

        width: ListView.view.width
        height: 60

        color: "white"

        property bool isSelected: false

        // Background image for selected state
        Image {
            id: backgroundImage
            anchors.fill: parent
            source: "image://backgrounds/" + model.name
            opacity: 0
            fillMode: Image.PreserveAspectCrop
        }

        // Content container
        Item {
            id: contentContainer
            anchors {
                fill: parent
                margins: 8
            }

            // Install button
            Button {
                id: installButton
                width: 44
                height: 44
                anchors {
                    left: parent.left
                    verticalCenter: parent.verticalCenter
                }

                background: Rectangle {
                    color: installButton.pressed ? "#e0e0e0" : "#f5f5f5"
                    radius: 4
                    border.color: "#cccccc"
                    border.width: 1
                }

                Image {
                    anchors.centerIn: parent
                    source: "image://icons/install"
                    width: 24
                    height: 24
                }
            }

            // Text block with name and author
            Item {
                id: textBlock
                anchors {
                    left: installButton.right
                    leftMargin: 12
                    verticalCenter: parent.verticalCenter
                }
                width: nameText.width
                height: authorText.visible ? nameText.height + authorText.height + 4 : nameText.height

                Text {
                    id: nameText
                    text: model.name
                    font.pixelSize: 16
                    font.bold: true
                    color: "#333333"

                    style: Text.Normal
                }

                Text {
                    id: authorText
                    anchors {
                        top: nameText.bottom
                        topMargin: 4
                    }
                    text: "by " + model.author
                    font.pixelSize: 12
                    color: "#666666"

                    style: Text.Normal
                }
            }

            // Action buttons (rename, edit, remove)
            Row {
                id: actionButtons
                anchors {
                    right: parent.right
                    verticalCenter: parent.verticalCenter
                }
                spacing: 8

                Button {
                    id: renameButton
                    width: 36
                    height: 36

                    background: Rectangle {
                        color: renameButton.pressed ? "#e0e0e0" : "#f5f5f5"
                        radius: 4
                        border.color: "#cccccc"
                        border.width: 1
                    }

                    Image {
                        anchors.centerIn: parent
                        source: "image://icons/rename"
                        width: 20
                        height: 20
                    }
                }

                Button {
                    id: editButton
                    width: 36
                    height: 36

                    background: Rectangle {
                        color: editButton.pressed ? "#e0e0e0" : "#f5f5f5"
                        radius: 4
                        border.color: "#cccccc"
                        border.width: 1
                    }

                    Image {
                        anchors.centerIn: parent
                        source: "image://icons/edit"
                        width: 20
                        height: 20
                    }
                }

                Button {
                    id: removeButton
                    width: 36
                    height: 36

                    background: Rectangle {
                        color: removeButton.pressed ? "#e0e0e0" : "#f5f5f5"
                        radius: 4
                        border.color: "#cccccc"
                        border.width: 1
                    }

                    Image {
                        anchors.centerIn: parent
                        source: "image://icons/remove"
                        width: 20
                        height: 20
                    }
                }
            }
        }

        // States
        states: [
            State {
                name: "normal"
                when: !root.isSelected

                PropertyChanges {
                    target: root
                    height: 60
                }

                PropertyChanges {
                    target: backgroundImage
                    opacity: 0
                }

                PropertyChanges {
                    target: contentContainer
                    anchors.bottomMargin: 8
                }

                PropertyChanges {
                    target: installButton
                    anchors.verticalCenterOffset: 0
                    anchors.bottomMargin: 0
                }

                AnchorChanges {
                    target: installButton
                    anchors.bottom: undefined
                    anchors.verticalCenter: contentContainer.verticalCenter
                }

                PropertyChanges {
                    target: textBlock
                    anchors.verticalCenterOffset: 0
                    anchors.bottomMargin: 0
                    height: nameText.height + authorText.height + 4
                }

                AnchorChanges {
                    target: textBlock
                    anchors.bottom: undefined
                    anchors.verticalCenter: contentContainer.verticalCenter
                }

                PropertyChanges {
                    target: nameText
                    color: "#333333"
                    style: Text.Normal
                }

                PropertyChanges {
                    target: authorText
                    color: "#666666"
                    visible: true
                    style: Text.Normal
                }

                AnchorChanges {
                    target: authorText
                    anchors.left: undefined
                    anchors.top: nameText.bottom
                }

                PropertyChanges {
                    target: authorText
                    anchors.topMargin: 4
                    anchors.leftMargin: 0
                }

                PropertyChanges {
                    target: actionButtons
                    anchors.verticalCenterOffset: 0
                    anchors.bottomMargin: 0
                }

                AnchorChanges {
                    target: actionButtons
                    anchors.bottom: undefined
                    anchors.verticalCenter: contentContainer.verticalCenter
                    anchors.right: contentContainer.right
                    anchors.left: undefined
                }
            },

            State {
                name: "selected"
                when: root.isSelected

                PropertyChanges {
                    target: root
                    height: 120
                }

                PropertyChanges {
                    target: backgroundImage
                    opacity: 1
                }

                PropertyChanges {
                    target: contentContainer
                    anchors.bottomMargin: 8
                }

                AnchorChanges {
                    target: installButton
                    anchors.verticalCenter: undefined
                    anchors.bottom: contentContainer.bottom
                }

                AnchorChanges {
                    target: textBlock
                    anchors.verticalCenter: undefined
                    anchors.bottom: installButton.top
                }

                PropertyChanges {
                    target: textBlock
                    anchors.bottomMargin: 8
                    height: nameText.height
                }

                PropertyChanges {
                    target: nameText
                    color: "white"
                    style: Text.Outline
                    styleColor: "black"
                }

                AnchorChanges {
                    target: authorText
                    anchors.top: undefined
                    anchors.left: nameText.right
                }

                PropertyChanges {
                    target: authorText
                    color: "white"
                    visible: true
                    style: Text.Outline
                    styleColor: "black"
                    anchors.topMargin: 0
                    anchors.leftMargin: 8
                }

                AnchorChanges {
                    target: actionButtons
                    anchors.verticalCenter: undefined
                    anchors.bottom: contentContainer.bottom
                    anchors.right: undefined
                    anchors.left: installButton.right
                }

                PropertyChanges {
                    target: actionButtons
                    anchors.leftMargin: 8
                }
            }
        ]

        // Transitions
        transitions: [
            Transition {
                from: "normal"
                to: "selected"

                NumberAnimation {
                    target: root
                    property: "height"
                    duration: 300
                    easing.type: Easing.InOutQuad
                }

                NumberAnimation {
                    target: backgroundImage
                    property: "opacity"
                    duration: 300
                    easing.type: Easing.InOutQuad
                }

                AnchorAnimation {
                    duration: 300
                    easing.type: Easing.InOutQuad
                }

                ColorAnimation {
                    targets: [nameText, authorText]
                    duration: 300
                    easing.type: Easing.InOutQuad
                }
            },

            Transition {
                from: "selected"
                to: "normal"

                NumberAnimation {
                    target: root
                    property: "height"
                    duration: 300
                    easing.type: Easing.InOutQuad
                }

                NumberAnimation {
                    target: backgroundImage
                    property: "opacity"
                    duration: 300
                    easing.type: Easing.InOutQuad
                }

                AnchorAnimation {
                    duration: 300
                    easing.type: Easing.InOutQuad
                }

                ColorAnimation {
                    targets: [nameText, authorText]
                    duration: 300
                    easing.type: Easing.InOutQuad
                }
            }
        ]

        // Mouse interaction for selection
        MouseArea {
            anchors.fill: parent
            onClicked: root.isSelected = !root.isSelected
        }
    }
}