import QtQuick

Item {

    width: 78
    height: 40
    property alias text: textLabel.text
    property alias color: rect.color

    Rectangle {
        id: rect
        color: "#2F2F37"
        anchors.fill: parent
        radius: 10
        border.color: Qt.lighter(color, 1.2)

        Text {
            id: textLabel
            font.pixelSize: 14
            font.bold: true
            text: qsTr("Home")
            color: "#FFFFFF"
            anchors.centerIn: parent
        }
    }
}
