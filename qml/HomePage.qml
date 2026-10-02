pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Page {
    id: page
    required property var session
    signal openRoom(string roomId, string roomName)
    signal openSettings()

    background: Rectangle { color: Theme.background }

    header: Item {
        implicitHeight: headerRow.implicitHeight
        width: parent.width

        RowLayout {
            id: headerRow
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.leftMargin: 16
            anchors.rightMargin: 16
            spacing: 12

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 4
                Label {
                    text: page.session.greeting
                    color: Theme.text
                    font.pixelSize: Theme.nameSize
                    font.bold: true
                    wrapMode: Text.WordWrap
                    Layout.fillWidth: true
                }
                RowLayout {
                    visible: page.session.weatherLine.length > 0
                    spacing: 8
                    Image {
                        source: page.session.weatherIcon
                        sourceSize.width: 24
                        sourceSize.height: 24
                        Accessible.name: qsTr("Weather")
                    }
                    Label {
                        text: page.session.weatherLine
                        color: Theme.text
                        font.pixelSize: Theme.labelSize
                    }
                }
            }
            ActionButton {
                text: qsTr("Settings")
                fill: Theme.card
                onClicked: page.openSettings()
            }
        }
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 16
        spacing: 12

        Label {
            text: page.session.statusMessage
            color: Theme.danger
            font.pixelSize: Theme.secondarySize
            wrapMode: Text.WordWrap
            visible: text.length > 0
            Layout.fillWidth: true
        }

        GridLayout {
            columns: page.width >= 700 ? 2 : 1
            columnSpacing: 12
            rowSpacing: 12
            Layout.fillWidth: true
            Layout.fillHeight: true

            Repeater {
                model: page.session.rooms
                delegate: Button {
                    id: roomButton
                    required property var modelData
                    Layout.fillWidth: true
                    Layout.preferredHeight: 96
                    Accessible.name: modelData.name
                    onClicked: page.openRoom(modelData.id, modelData.name)
                    background: Rectangle { color: Theme.card; radius: 12 }
                    contentItem: ColumnLayout {
                        anchors.fill: parent
                        anchors.margins: 16
                        Label {
                            text: roomButton.modelData.name
                            color: Theme.text
                            font.pixelSize: Theme.labelSize
                            font.bold: true
                        }
                        Label {
                            text: qsTr("%1 devices").arg(roomButton.modelData.devices.length)
                            color: Theme.muted
                            font.pixelSize: Theme.secondarySize
                        }
                    }
                }
            }

            ActionButton {
                text: qsTr("Add room")
                Layout.fillWidth: true
                Layout.preferredHeight: 96
                enabled: page.session.pendingMutation === ""
                onClicked: addDialog.open()
            }
        }
    }

    Dialog {
        id: addDialog
        title: qsTr("Add room")
        modal: true
        anchors.centerIn: parent
        standardButtons: Dialog.Ok | Dialog.Cancel
        background: Rectangle { color: Theme.card; radius: 12 }
        contentItem: TextField {
            id: roomName
            placeholderText: qsTr("Room name")
            color: Theme.text
            font.pixelSize: Theme.labelSize
            Accessible.name: placeholderText
            background: Rectangle { color: Theme.background; radius: 8 }
        }
        onAccepted: {
            if (!page.session.createRoom(roomName.text))
                addDialog.open()
        }
    }

    Connections {
        target: page.session
        function onMutationFinished(op, success) {
            if (op !== "room-insert")
                return
            if (success)
                roomName.clear()
            else
                addDialog.open()
        }
    }
}
