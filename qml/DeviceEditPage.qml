import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Page {
    id: page
    required property var session
    required property string roomId
    property string deviceId: ""
    property string deviceName: ""
    property string deviceKind: "light"

    background: Rectangle { color: Theme.background }

    function kindValue() {
        if (page.deviceId !== "")
            return page.deviceKind
        if (kindBox.currentIndex === 1)
            return "plug"
        if (kindBox.currentIndex === 2)
            return "thermometer"
        return "light"
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 24
        spacing: 12

        ActionButton {
            text: qsTr("Back")
            fill: Theme.card
            enabled: page.session.pendingMutation === ""
            onClicked: StackView.view.pop()
        }
        Label {
            text: page.deviceId === "" ? qsTr("Add device") : qsTr("Edit device")
            color: Theme.text
            font.pixelSize: Theme.nameSize
            font.bold: true
        }
        TextField {
            id: nameField
            Layout.fillWidth: true
            text: page.deviceName
            placeholderText: qsTr("Device name")
            color: Theme.text
            font.pixelSize: Theme.labelSize
            Accessible.name: placeholderText
            background: Rectangle { color: Theme.card; radius: 8 }
        }
        ComboBox {
            id: kindBox
            Layout.fillWidth: true
            visible: page.deviceId === ""
            model: [qsTr("Light"), qsTr("Plug")]
            Accessible.name: qsTr("Device kind")
        }
        Label {
            text: qsTr("Sensor setup is unavailable until its telemetry contract is confirmed.")
            color: Theme.muted
            font.pixelSize: Theme.secondarySize
            visible: page.deviceId === ""
            wrapMode: Text.WordWrap
            Layout.fillWidth: true
        }
        Label {
            text: page.session.statusMessage
            color: Theme.danger
            font.pixelSize: Theme.secondarySize
            visible: text.length > 0
            wrapMode: Text.WordWrap
            Layout.fillWidth: true
        }
        ActionButton {
            text: qsTr("Save")
            Layout.fillWidth: true
            enabled: page.session.pendingMutation === ""
            onClicked: {
                if (page.deviceId === "")
                    page.session.createDevice(page.roomId, nameField.text, page.kindValue())
                else
                    page.session.renameDevice(page.deviceId, nameField.text)
            }
        }
        ActionButton {
            visible: page.deviceId !== ""
            text: qsTr("Delete device")
            fill: Theme.card
            labelColor: Theme.danger
            Layout.fillWidth: true
            enabled: page.session.pendingMutation === ""
            onClicked: deleteDialog.open()
        }
        Item { Layout.fillHeight: true }
    }

    Dialog {
        id: deleteDialog
        title: qsTr("Delete this device?")
        modal: true
        anchors.centerIn: parent
        standardButtons: Dialog.Ok | Dialog.Cancel
        background: Rectangle { color: Theme.card; radius: 12 }
        onAccepted: page.session.deleteDevice(page.deviceId)
    }

    Connections {
        target: page.session
        function onMutationFinished(op, success) {
            if (!success)
                return
            if ((page.deviceId === "" && op === "device-insert")
                    || (page.deviceId !== "" && (op === "device-update" || op === "device-delete")))
                page.StackView.view.pop()
        }
    }
}
