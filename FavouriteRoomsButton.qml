import QtQuick

Item {
    id: button
    width: 163
    height: 127

    property string room: ""
    property int deviceCount: 0
    property alias source: img.source

    Rectangle {
        id: background
        anchors.fill: parent
        color: "transparent"
        radius: 10

        Image {
            id: img
            anchors.fill: parent
            fillMode: Image.PreserveAspectFit
            mipmap: true
            antialiasing: true
            opacity: 0.5
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
        id: deviceCountLabel
        x: 12
        y: 104
        text: button.text
        color: "#FFFFFF"
        font.pixelSize: 11
    }

    onRoomChanged: {
        roomLabel.text = button.room
    }

    onDeviceCountChanged: {
        deviceCountLabel.text = button.deviceCount + " " + "Devices"
    }
}
