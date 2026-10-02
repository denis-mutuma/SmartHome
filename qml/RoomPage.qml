pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Page {
    id: page
    required property var session
    required property string roomId
    property string roomName: ""
    signal editDevice(string deviceId, string deviceName, string deviceKind)
    signal addDevice()

    readonly property var room: {
        const rows = page.session.rooms
        for (let i = 0; i < rows.length; ++i) {
            if (rows[i].id === page.roomId)
                return rows[i]
        }
        return null
    }

    background: Rectangle { color: Theme.background }

    header: RowLayout {
        spacing: 12
        ActionButton {
            text: qsTr("Back")
            fill: Theme.card
            enabled: page.session.pendingMutation === ""
            onClicked: StackView.view.pop()
        }
        Label {
            text: page.room ? page.room.name : page.roomName
            color: Theme.text
            font.pixelSize: Theme.labelSize
            font.bold: true
            Layout.fillWidth: true
            elide: Text.ElideRight
        }
        ActionButton {
            text: qsTr("Rename")
            fill: Theme.card
            enabled: page.session.pendingMutation === ""
            onClicked: renameDialog.open()
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
            visible: text.length > 0
            wrapMode: Text.WordWrap
            Layout.fillWidth: true
        }

        Repeater {
            model: page.room ? page.room.devices : []
            delegate: Rectangle {
                id: deviceCard
                required property var modelData
                Layout.fillWidth: true
                implicitHeight: 72
                radius: 12
                color: Theme.card

                RowLayout {
                    anchors.fill: parent
                    anchors.margins: 12
                    spacing: 12
                    Image {
                        source: {
                            if (deviceCard.modelData.kind === "plug")
                                return "qrc:/qt/qml/SmartHome/assets/icons/plug.svg"
                            if (deviceCard.modelData.kind === "thermometer")
                                return "qrc:/qt/qml/SmartHome/assets/icons/thermometer.svg"
                            return deviceCard.modelData.isOn === true
                                    ? "qrc:/qt/qml/SmartHome/assets/icons/light-on.svg"
                                    : "qrc:/qt/qml/SmartHome/assets/icons/light-off.svg"
                        }
                        sourceSize.width: 24
                        sourceSize.height: 24
                        Accessible.name: deviceCard.modelData.kind
                    }
                    ColumnLayout {
                        Layout.fillWidth: true
                        Label {
                            text: deviceCard.modelData.name
                            color: Theme.text
                            font.pixelSize: Theme.labelSize
                        }
                        Label {
                            text: deviceCard.modelData.kind === "thermometer"
                                  ? qsTr("%1°C").arg(Number(deviceCard.modelData.celsius).toFixed(1))
                                  : (deviceCard.modelData.isOn === true ? qsTr("On") : qsTr("Off"))
                            color: Theme.muted
                            font.pixelSize: Theme.secondarySize
                        }
                    }
                    AbstractButton {
                        visible: deviceCard.modelData.kind !== "thermometer"
                        Accessible.name: deviceCard.modelData.isOn === true ? qsTr("Turn off") : qsTr("Turn on")
                        implicitWidth: 48
                        implicitHeight: 24
                        onClicked: page.session.setDeviceOn(deviceCard.modelData.id, deviceCard.modelData.isOn !== true)
                        contentItem: Image {
                            source: deviceCard.modelData.isOn === true
                                    ? "qrc:/qt/qml/SmartHome/assets/icons/toggle-on.svg"
                                    : "qrc:/qt/qml/SmartHome/assets/icons/toggle-off.svg"
                            sourceSize.width: 48
                            sourceSize.height: 24
                        }
                    }
                    ActionButton {
                        text: qsTr("Edit")
                        fill: Theme.background
                        onClicked: page.editDevice(deviceCard.modelData.id, deviceCard.modelData.name, deviceCard.modelData.kind)
                    }
                }
            }
        }

        ActionButton {
            text: qsTr("Add device")
            Layout.fillWidth: true
            enabled: page.session.pendingMutation === ""
            onClicked: page.addDevice()
        }
        ActionButton {
            text: qsTr("Delete room")
            fill: Theme.card
            labelColor: Theme.danger
            Layout.fillWidth: true
            enabled: page.session.pendingMutation === ""
            onClicked: deleteDialog.open()
        }
        Item { Layout.fillHeight: true }
    }

    Dialog {
        id: renameDialog
        title: qsTr("Rename room")
        modal: true
        anchors.centerIn: parent
        standardButtons: Dialog.Ok | Dialog.Cancel
        background: Rectangle { color: Theme.card; radius: 12 }
        contentItem: TextField {
            id: renameField
            text: page.room ? page.room.name : ""
            color: Theme.text
            font.pixelSize: Theme.labelSize
            Accessible.name: qsTr("Room name")
            background: Rectangle { color: Theme.background; radius: 8 }
        }
        onAccepted: {
            if (!page.session.renameRoom(page.roomId, renameField.text))
                renameDialog.open()
        }
    }

    Dialog {
        id: deleteDialog
        title: qsTr("Delete this room and its devices?")
        modal: true
        anchors.centerIn: parent
        standardButtons: Dialog.Ok | Dialog.Cancel
        background: Rectangle { color: Theme.card; radius: 12 }
        onAccepted: {
            page.session.deleteRoom(page.roomId)
        }
    }

    Connections {
        target: page.session
        function onMutationFinished(op, success) {
            if (op === "room-update" && !success)
                renameDialog.open()
            if (op !== "room-delete")
                return
            if (success)
                page.StackView.view.pop()
            else
                deleteDialog.open()
        }
    }
}
