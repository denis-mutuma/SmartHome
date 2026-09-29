pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtCore

ApplicationWindow {
    id: window
    visible: true
    width: 1100
    height: 800
    minimumWidth: 360
    minimumHeight: 640
    color: Theme.background
    title: qsTr("SmartHome")

    SessionController {
        id: session
    }

    Settings {
        id: geometry
        category: "window"
        property int windowX: 80
        property int windowY: 80
        property int windowWidth: 1100
        property int windowHeight: 800
    }

    Component.onCompleted: {
        x = geometry.windowX
        y = geometry.windowY
        width = geometry.windowWidth
        height = geometry.windowHeight
        stack.replace(null, session.signedIn ? homePage : loginPage)
    }

    onClosing: function(close) {
        geometry.windowX = x
        geometry.windowY = y
        geometry.windowWidth = width
        geometry.windowHeight = height
        close.accepted = true
    }

    Connections {
        target: session
        function onSignedInChanged() {
            stack.replace(null, session.signedIn ? homePage : loginPage)
        }
    }

    Connections {
        target: session
        function onApplicationActiveChanged() {
            if (session.applicationActive && session.signedIn)
                session.reload()
        }
    }

    Timer {
        interval: 20000
        repeat: true
        running: session.signedIn && session.applicationActive
        onTriggered: session.reload()
    }

    StackView {
        id: stack
        anchors.fill: parent
    }

    Component {
        id: loginPage
        LoginPage {
            session: session
            onOpenRegister: stack.push(registerPage)
        }
    }

    Component {
        id: registerPage
        RegisterPage {
            session: session
        }
    }

    Component {
        id: homePage
        HomePage {
            session: session
            onOpenSettings: stack.push(settingsPage)
            onOpenRoom: function(roomId, roomName) {
                stack.push(roomPage, { roomId: roomId, roomName: roomName })
            }
        }
    }

    Component {
        id: roomPage
        RoomPage {
            session: session
            onAddDevice: stack.push(devicePage, { roomId: roomId, deviceId: "", deviceName: "", deviceKind: "light" })
            onEditDevice: function(deviceId, deviceName, deviceKind) {
                stack.push(devicePage, {
                    roomId: roomId,
                    deviceId: deviceId,
                    deviceName: deviceName,
                    deviceKind: deviceKind
                })
            }
        }
    }

    Component {
        id: devicePage
        DeviceEditPage {
            session: session
        }
    }

    Component {
        id: settingsPage
        SettingsPage {
            session: session
        }
    }
}
