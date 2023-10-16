import QtQuick

Item {
    id: button
    width: 130
    height: 130

    property bool active: false
    property string room: ""
    property string device: ""
    property string deviceStatus: ""

    Rectangle {
        id: background
        anchors.fill: parent
        color: active ? "#536DED" : "#2F2F37"
        radius: 10
        border.color: Qt.lighter(color, 1.2)

        Behavior on color {
            ColorAnimation {
                duration: 50
            }
        }
    }

    Image {
        id: image
        x: 18
        y: 8
        width: 56
        height: 56
        source: active ? "assets/icons/light-on.svg" : "assets/icons/light-off.svg"

        Behavior on source {
            PropertyAnimation {
                duration: 50
            }
        }
    }

    Text {
        id: roomLabel
        x: 12
        y: 81
        text: button.room
        color: "#FFFFFF"
        font.pixelSize: 14
        font.bold: true
    }

    Text {
        id: statusLabel
        x: 12
        y: 104
        color: "#FFFFFF"
        font.pixelSize: 11
    }

    Image {
        id: icon
        x: 88
        y: 16
        width: 25
        height: 16
        source: active ? "assets/icons/toggle-on.svg" : "assets/icons/toggle-off.svg"

        Behavior on source {
            PropertyAnimation {
                duration: 50
            }
        }

        MouseArea {
            anchors.fill: parent

            onClicked: {
                active = !active

                icon.source = active ? "assets/icons/toggle-on.svg" : "assets/icons/toggle-off.svg"

                var deviceStatusText = button.deviceStatus.toLowerCase()

                if(deviceStatusText == "on") {
                    deviceStatus = "Off"
                } else {
                    deviceStatus = "On"
                }
            }
        }
    }

    onActiveChanged: {
        background.color = active ? "#536DED" : "#2F2F37"
    }

    onRoomChanged: {
        roomLabel.text = button.room
    }

    onDeviceChanged: {
        statusLabel.text = button.device + " " + button.deviceStatus
    }

    onDeviceStatusChanged: {
        statusLabel.text = button.device + " " + button.deviceStatus
    }
}
