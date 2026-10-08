pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import SmartHome

Window {
    id: root

    width: 440
    height: 720
    minimumWidth: 320
    minimumHeight: 560
    visible: true
    title: qsTr("SmartHome")
    color: "#18171C"
    property string selectedRoomId: ""
    property bool settingsOpen: false
    property bool roomCreatePending: false
    property bool deviceCreatePending: false
    property string actionEntityType: ""
    property string actionEntityId: ""
    property string actionEntityName: ""

    function roomById(roomId) {
        for (let index = 0; index < session.rooms.length; ++index) {
            if (session.rooms[index].id === roomId) {
                return session.rooms[index]
            }
        }
        return null
    }

    readonly property var selectedRoom: roomById(selectedRoomId)

    function openEntityActions(entityType, entityId, entityName) {
        actionEntityType = entityType
        actionEntityId = entityId
        actionEntityName = entityName
        entityMenu.popup()
    }

    SessionController {
        id: session
    }

    Connections {
        target: session

        function onSignedInChanged() {
            passwordInput.clear()
            root.selectedRoomId = ""
            root.settingsOpen = false
            if (!session.signedIn) {
                root.roomCreatePending = false
                root.deviceCreatePending = false
                root.actionEntityType = ""
                root.actionEntityId = ""
                root.actionEntityName = ""
                entityMenu.close()
                createDeviceDialog.close()
                renameDialog.close()
                deleteDialog.close()
            }
        }

        function onRoomCreated(name) {
            root.roomCreatePending = false
            if (roomNameInput.text.trim() === name) {
                roomNameInput.clear()
            }
        }

        function onRoomCreateFailed() {
            root.roomCreatePending = false
        }

        function onDeviceCreated(roomId, name) {
            root.deviceCreatePending = false
            if (root.selectedRoom && root.selectedRoom.id === roomId
                    && createDeviceDialog.visible && deviceNameInput.text.trim() === name) {
                createDeviceDialog.close()
                deviceNameInput.clear()
            }
        }

        function onDeviceCreateFailed() {
            root.deviceCreatePending = false
        }

        function onRenameFinished(entityKey, success) {
            if (entityKey !== renameDialog.pendingKey) {
                return
            }
            renameDialog.pendingKey = ""
            if (success) {
                renameDialog.close()
                renameInput.clear()
            } else {
                renameDialog.errorMessage = session.statusMessage
            }
        }
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 28
        spacing: 0

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 4
            color: "#536DED"
        }

        Item {
            Layout.fillHeight: true
        }

        StackLayout {
            Layout.fillWidth: true
            Layout.preferredHeight: 520
            currentIndex: !session.signedIn ? 0 : (root.settingsOpen ? 3 : (root.selectedRoom ? 2 : 1))

            Item {
                ColumnLayout {
                    anchors.centerIn: parent
                    width: Math.min(parent.width, 380)
                    spacing: 16

                    Label {
                        Layout.fillWidth: true
                        text: qsTr("SmartHome")
                        color: "#FFFFFF"
                        font.pixelSize: 32
                        font.bold: true
                    }

                    TabBar {
                        id: accountMode
                        Layout.fillWidth: true
                        Accessible.name: qsTr("Account mode")

                        TabButton {
                            text: qsTr("Sign in")
                            Accessible.name: qsTr("Sign in")
                        }
                        TabButton {
                            text: qsTr("Register")
                            Accessible.name: qsTr("Register")
                        }
                    }

                    TextField {
                        id: firstNameInput
                        visible: accountMode.currentIndex === 1
                        Layout.fillWidth: true
                        Layout.preferredHeight: visible ? 50 : 0
                        placeholderText: qsTr("First name")
                        Accessible.name: qsTr("First name")
                        color: "#FFFFFF"

                        background: Rectangle {
                            color: "#2F2F37"
                            radius: 4
                        }
                    }

                    TextField {
                        id: emailInput
                        Layout.fillWidth: true
                        Layout.preferredHeight: 50
                        placeholderText: qsTr("Email")
                        inputMethodHints: Qt.ImhEmailCharactersOnly
                        Accessible.name: qsTr("Email")
                        color: "#FFFFFF"

                        background: Rectangle {
                            color: "#2F2F37"
                            radius: 4
                        }
                    }

                    TextField {
                        id: passwordInput
                        Layout.fillWidth: true
                        Layout.preferredHeight: 50
                        placeholderText: qsTr("Password")
                        echoMode: TextInput.Password
                        Accessible.name: qsTr("Password")
                        color: "#FFFFFF"
                        onAccepted: submitButton.clicked()

                        background: Rectangle {
                            color: "#2F2F37"
                            radius: 4
                        }
                    }

                    Label {
                        Layout.fillWidth: true
                        visible: session.statusMessage.length > 0
                        text: session.statusMessage
                        color: "#FFFFFF"
                        wrapMode: Text.Wrap
                        Accessible.name: qsTr("Account status")
                    }

                    Button {
                        id: submitButton
                        Layout.fillWidth: true
                        Layout.preferredHeight: 50
                        text: accountMode.currentIndex === 0 ? qsTr("Sign in") : qsTr("Create account")
                        Accessible.name: text
                        onClicked: {
                            if (accountMode.currentIndex === 0) {
                                session.signIn(emailInput.text, passwordInput.text)
                            } else {
                                session.registerAccount(firstNameInput.text, emailInput.text, passwordInput.text)
                            }
                        }

                        background: Rectangle {
                            color: "#536DED"
                            radius: 4
                        }
                    }
                }
            }

            Item {
                ColumnLayout {
                    anchors.fill: parent
                    spacing: 12

                    Label {
                        Layout.fillWidth: true
                        text: session.greeting
                        color: "#FFFFFF"
                        font.pixelSize: 26
                        font.bold: true
                        Accessible.name: qsTr("Greeting")
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 10

                        Image {
                            visible: session.weatherIcon.length > 0
                            source: session.weatherIcon
                            sourceSize.width: 24
                            sourceSize.height: 24
                            fillMode: Image.PreserveAspectFit
                            Accessible.name: qsTr("Weather icon")
                        }

                        Label {
                            Layout.fillWidth: true
                            text: session.weatherLine.length > 0 ? session.weatherLine : qsTr("Weather unavailable")
                            color: "#FFFFFF"
                            wrapMode: Text.Wrap
                            Accessible.name: qsTr("Current weather")
                        }

                        Label {
                            text: session.city
                            color: "#FFFFFF"
                            Accessible.name: qsTr("Saved city")
                        }
                    }

                    Label {
                        Layout.fillWidth: true
                        text: qsTr("Rooms")
                        color: "#FFFFFF"
                        font.pixelSize: 16
                        font.bold: true
                    }

                    Label {
                        Layout.fillWidth: true
                        visible: session.rooms.length === 0
                        text: qsTr("No rooms yet. Add your first room below.")
                        color: "#FFFFFF"
                        wrapMode: Text.Wrap
                        Accessible.name: qsTr("Empty rooms message")
                    }

                    ListView {
                        id: roomList
                        visible: session.rooms.length > 0
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        Layout.minimumHeight: 0
                        clip: true
                        spacing: 8
                        model: session.rooms

                        delegate: Rectangle {
                            id: roomDelegate
                            required property var modelData
                            width: roomList.width
                            height: 58
                            color: "#2F2F37"
                            radius: 4

                            RowLayout {
                                anchors.fill: parent
                                anchors.leftMargin: 10
                                anchors.rightMargin: 4
                                spacing: 10

                                Button {
                                    Layout.fillWidth: true
                                    text: roomDelegate.modelData.name
                                    flat: true
                                    Accessible.name: qsTr("Open room %1").arg(roomDelegate.modelData.name)
                                    onClicked: {
                                        root.settingsOpen = false
                                        root.selectedRoomId = roomDelegate.modelData.id
                                        session.reload()
                                    }

                                    contentItem: Label {
                                        text: roomDelegate.modelData.name
                                        color: "#FFFFFF"
                                        elide: Text.ElideRight
                                    }
                                }

                                Label {
                                    text: qsTr("%1 devices").arg(roomDelegate.modelData.deviceCount)
                                    color: "#FFFFFF"
                                    Accessible.name: qsTr("Device count")
                                }

                                ToolButton {
                                    text: "..."
                                    Accessible.name: qsTr("Room actions for %1").arg(roomDelegate.modelData.name)
                                    onClicked: root.openEntityActions("room", roomDelegate.modelData.id,
                                        roomDelegate.modelData.name)
                                }
                            }
                        }
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 8

                        TextField {
                            id: roomNameInput
                            Layout.fillWidth: true
                            Layout.preferredHeight: 46
                            placeholderText: qsTr("Room name")
                            Accessible.name: qsTr("Room name")
                            color: "#FFFFFF"

                            background: Rectangle {
                                color: "#2F2F37"
                                radius: 4
                            }
                        }

                        Button {
                            Layout.preferredHeight: 46
                            text: qsTr("Add room")
                            Accessible.name: qsTr("Add room")
                            enabled: !root.roomCreatePending
                            onClicked: {
                                root.roomCreatePending = true
                                if (!session.createRoom(roomNameInput.text)) {
                                    root.roomCreatePending = false
                                }
                            }

                            background: Rectangle {
                                color: "#536DED"
                                radius: 4
                            }
                        }
                    }

                    Label {
                        Layout.fillWidth: true
                        visible: session.statusMessage.length > 0
                        text: session.statusMessage
                        color: "#FFFFFF"
                        wrapMode: Text.Wrap
                        Accessible.name: qsTr("Account status")
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 8

                        Label {
                            Layout.fillWidth: true
                            text: session.email
                            color: "#FFFFFF"
                            elide: Text.ElideRight
                            Accessible.name: qsTr("Signed-in email")
                        }

                        Button {
                            text: qsTr("Settings")
                            Accessible.name: qsTr("Settings")
                            onClicked: root.settingsOpen = true

                            background: Rectangle {
                                color: "#2F2F37"
                                radius: 4
                            }
                        }

                        Button {
                            text: qsTr("Sign out")
                            Accessible.name: qsTr("Sign out")
                            onClicked: session.signOut()

                            background: Rectangle {
                                color: "#2F2F37"
                                radius: 4
                            }
                        }
                    }
                }
            }

            Item {
                ColumnLayout {
                    anchors.fill: parent
                    spacing: 12

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 12

                        Button {
                            text: qsTr("Back")
                            Accessible.name: qsTr("Back to rooms")
                            onClicked: root.selectedRoomId = ""

                            background: Rectangle {
                                color: "#2F2F37"
                                radius: 4
                            }
                        }

                        Label {
                            Layout.fillWidth: true
                            text: root.selectedRoom ? root.selectedRoom.name : qsTr("Room")
                            color: "#FFFFFF"
                            font.pixelSize: 22
                            font.bold: true
                            elide: Text.ElideRight
                            Accessible.name: qsTr("Room name")
                        }

                        Button {
                            text: qsTr("Add device")
                            Accessible.name: qsTr("Add device")
                            enabled: !root.deviceCreatePending
                            onClicked: {
                                deviceNameInput.clear()
                                deviceKind.currentIndex = 0
                                createDeviceDialog.open()
                            }

                            background: Rectangle {
                                color: "#536DED"
                                radius: 4
                            }
                        }
                    }

                    Label {
                        visible: root.selectedRoom && root.selectedRoom.devices.length === 0
                        Layout.fillWidth: true
                        text: qsTr("No devices in this room yet.")
                        color: "#FFFFFF"
                        wrapMode: Text.Wrap
                        Accessible.name: qsTr("Empty devices message")
                    }

                    ListView {
                        id: deviceList
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        Layout.minimumHeight: 0
                        clip: true
                        spacing: 8
                        model: root.selectedRoom ? root.selectedRoom.devices : []

                        delegate: Rectangle {
                            id: deviceDelegate
                            required property var modelData
                            width: deviceList.width
                            height: 66
                            color: "#2F2F37"
                            radius: 4

                            RowLayout {
                                anchors.fill: parent
                                anchors.leftMargin: 14
                                anchors.rightMargin: 14
                                spacing: 12

                                ColumnLayout {
                                    Layout.fillWidth: true
                                    spacing: 2

                                    Label {
                                        Layout.fillWidth: true
                                        text: deviceDelegate.modelData.name
                                        color: "#FFFFFF"
                                        elide: Text.ElideRight
                                        Accessible.name: qsTr("Device name")
                                    }

                                    Label {
                                        Layout.fillWidth: true
                                        text: deviceDelegate.modelData.kind
                                        color: "#FFFFFF"
                                        Accessible.name: qsTr("Device type")
                                    }
                                }

                                Label {
                                    visible: deviceDelegate.modelData.kind === "thermometer"
                                    text: deviceDelegate.modelData.celsius === null
                                        ? qsTr("No reading")
                                        : qsTr("%1 °C").arg(Number(deviceDelegate.modelData.celsius).toFixed(1))
                                    color: "#FFFFFF"
                                    Accessible.name: qsTr("Temperature reading")
                                }

                                Switch {
                                    id: deviceSwitch
                                    visible: deviceDelegate.modelData.kind !== "thermometer"
                                    Accessible.name: qsTr("%1 power").arg(deviceDelegate.modelData.name)
                                    onToggled: session.setDeviceOn(deviceDelegate.modelData.deviceId, checked)

                                    Binding {
                                        target: deviceSwitch
                                        property: "checked"
                                        value: Boolean(deviceDelegate.modelData.isOn)
                                        when: !deviceSwitch.down
                                    }
                                }

                                ToolButton {
                                    text: "..."
                                    Accessible.name: qsTr("Device actions for %1").arg(deviceDelegate.modelData.name)
                                    onClicked: root.openEntityActions("device", deviceDelegate.modelData.deviceId,
                                        deviceDelegate.modelData.name)
                                }
                            }
                        }
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 8

                        Label {
                            Layout.fillWidth: true
                            text: session.statusMessage
                            visible: session.statusMessage.length > 0
                            color: "#FFFFFF"
                            wrapMode: Text.Wrap
                            Accessible.name: qsTr("Account status")
                        }

                        Button {
                            text: qsTr("Sign out")
                            Accessible.name: qsTr("Sign out")
                            onClicked: session.signOut()

                            background: Rectangle {
                                color: "#2F2F37"
                                radius: 4
                            }
                        }
                    }
                }
            }

            Item {
                onVisibleChanged: {
                    if (visible) {
                        settingsFirstName.text = session.firstName
                        settingsCity.text = session.city
                    }
                }

                ColumnLayout {
                    anchors.fill: parent
                    spacing: 14

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 12

                        Button {
                            text: qsTr("Back")
                            Accessible.name: qsTr("Back to home")
                            onClicked: root.settingsOpen = false

                            background: Rectangle {
                                color: "#2F2F37"
                                radius: 4
                            }
                        }

                        Label {
                            Layout.fillWidth: true
                            text: qsTr("Settings")
                            color: "#FFFFFF"
                            font.pixelSize: 22
                            font.bold: true
                        }
                    }

                    TextField {
                        id: settingsFirstName
                        Layout.fillWidth: true
                        Layout.preferredHeight: 50
                        placeholderText: qsTr("First name")
                        Accessible.name: qsTr("First name")
                        color: "#FFFFFF"

                        background: Rectangle {
                            color: "#2F2F37"
                            radius: 4
                        }
                    }

                    TextField {
                        id: settingsCity
                        Layout.fillWidth: true
                        Layout.preferredHeight: 50
                        placeholderText: qsTr("City")
                        Accessible.name: qsTr("Saved city")
                        color: "#FFFFFF"

                        background: Rectangle {
                            color: "#2F2F37"
                            radius: 4
                        }
                    }

                    Label {
                        Layout.fillWidth: true
                        visible: session.statusMessage.length > 0
                        text: session.statusMessage
                        color: "#FFFFFF"
                        wrapMode: Text.Wrap
                        Accessible.name: qsTr("Settings status")
                    }

                    Button {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 50
                        text: qsTr("Save settings")
                        Accessible.name: qsTr("Save settings")
                        onClicked: session.saveSettings(settingsFirstName.text, settingsCity.text)

                        background: Rectangle {
                            color: "#536DED"
                            radius: 4
                        }
                    }

                    Item {
                        Layout.fillHeight: true
                    }

                    Button {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 46
                        text: qsTr("Sign out")
                        Accessible.name: qsTr("Sign out")
                        onClicked: session.signOut()

                        background: Rectangle {
                            color: "#2F2F37"
                            radius: 4
                        }
                    }
                }
            }
        }

        Item {
            Layout.fillHeight: true
        }
    }

    Menu {
        id: entityMenu

        MenuItem {
            text: qsTr("Rename")
            onTriggered: {
                renameInput.text = root.actionEntityName
                renameDialog.open()
            }
        }

        MenuItem {
            text: qsTr("Delete")
            onTriggered: deleteDialog.open()
        }
    }

    Dialog {
        id: createDeviceDialog
        anchors.centerIn: Overlay.overlay
        width: Math.min(root.width - 32, 360)
        modal: true
        title: qsTr("Add device")

        contentItem: ColumnLayout {
            spacing: 10

            TextField {
                id: deviceNameInput
                Layout.fillWidth: true
                placeholderText: qsTr("Device name")
                Accessible.name: qsTr("Device name")
                color: "#FFFFFF"

                background: Rectangle {
                    color: "#2F2F37"
                    radius: 4
                }
            }

            ComboBox {
                id: deviceKind
                Layout.fillWidth: true
                model: [qsTr("Light"), qsTr("Plug"), qsTr("Thermometer")]
                Accessible.name: qsTr("Device type")
            }

            Label {
                Layout.fillWidth: true
                visible: session.statusMessage.length > 0
                text: session.statusMessage
                color: "#FFFFFF"
                wrapMode: Text.Wrap
                Accessible.name: qsTr("Create device status")
            }
        }

        footer: DialogButtonBox {
            Button {
                text: root.deviceCreatePending ? qsTr("Saving...") : qsTr("Save")
                Accessible.name: qsTr("Save device")
                DialogButtonBox.buttonRole: DialogButtonBox.ActionRole
                enabled: !root.deviceCreatePending
                onClicked: {
                    if (!root.selectedRoom) {
                        return
                    }
                    var kind = deviceKind.currentIndex === 0 ? "light"
                        : deviceKind.currentIndex === 1 ? "plug" : "thermometer"
                    root.deviceCreatePending = true
                    if (!session.createDevice(root.selectedRoom.id, deviceNameInput.text, kind)) {
                        root.deviceCreatePending = false
                    }
                }
            }

            Button {
                text: qsTr("Cancel")
                Accessible.name: qsTr("Cancel adding device")
                DialogButtonBox.buttonRole: DialogButtonBox.RejectRole
            }
        }
    }

    Dialog {
        id: renameDialog
        property string pendingKey: ""
        property string errorMessage: ""
        anchors.centerIn: Overlay.overlay
        width: Math.min(root.width - 32, 360)
        modal: true
        title: root.actionEntityType === "room" ? qsTr("Rename room") : qsTr("Rename device")
        closePolicy: pendingKey.length > 0 ? Popup.NoAutoClose : Popup.CloseOnEscape | Popup.CloseOnPressOutside
        onClosed: { pendingKey = ""; errorMessage = "" }

        contentItem: ColumnLayout {
            TextField {
                id: renameInput
                Layout.fillWidth: true
                enabled: renameDialog.pendingKey.length === 0
                onAccepted: renameSave.clicked()
                placeholderText: root.actionEntityType === "room" ? qsTr("Room name") : qsTr("Device name")
                Accessible.name: placeholderText
                color: "#FFFFFF"

                background: Rectangle {
                    color: "#2F2F37"
                    radius: 4
                }
            }
            Label {
                Layout.fillWidth: true
                visible: renameDialog.errorMessage.length > 0
                text: renameDialog.errorMessage
                color: "#FFFFFF"
                wrapMode: Text.Wrap
                Accessible.name: qsTr("Rename status")
            }
        }

        footer: DialogButtonBox {
            Button {
                id: renameSave
                text: renameDialog.pendingKey.length > 0 ? qsTr("Saving...") : qsTr("Save")
                Accessible.name: qsTr("Save name")
                DialogButtonBox.buttonRole: DialogButtonBox.ActionRole
                enabled: renameDialog.pendingKey.length === 0
                onClicked: {
                    if (renameDialog.pendingKey.length > 0) {
                        return
                    }
                    renameDialog.pendingKey = root.actionEntityType + ":" + root.actionEntityId
                    renameDialog.errorMessage = ""
                    const accepted = root.actionEntityType === "room"
                        ? session.renameRoom(root.actionEntityId, renameInput.text)
                        : session.renameDevice(root.actionEntityId, renameInput.text)
                    if (!accepted) {
                        renameDialog.pendingKey = ""
                        renameDialog.errorMessage = session.statusMessage
                    }
                }
            }
            Button {
                text: qsTr("Cancel")
                Accessible.name: qsTr("Cancel rename")
                DialogButtonBox.buttonRole: DialogButtonBox.RejectRole
                enabled: renameDialog.pendingKey.length === 0
            }
        }
    }

    Dialog {
        id: deleteDialog
        anchors.centerIn: Overlay.overlay
        width: Math.min(root.width - 32, 360)
        modal: true
        title: qsTr("Delete %1?").arg(root.actionEntityType === "room" ? qsTr("room") : qsTr("device"))
        standardButtons: Dialog.Yes | Dialog.Cancel

        contentItem: Label {
            text: root.actionEntityType === "room"
                ? qsTr("Delete %1 and its devices?").arg(root.actionEntityName)
                : qsTr("Delete %1?").arg(root.actionEntityName)
            color: "#FFFFFF"
            wrapMode: Text.Wrap
        }

        onAccepted: {
            if (root.actionEntityType === "room") {
                session.deleteRoom(root.actionEntityId)
            } else {
                session.deleteDevice(root.actionEntityId)
            }
        }
    }
}