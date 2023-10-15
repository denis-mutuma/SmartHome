import QtQuick

Rectangle {
    id: rect
    color: "transparent"

    property alias source: image.source

    signal clicked

    Image {
        id: image
        anchors.fill: parent
        fillMode: Image.PreserveAspectFit
        mipmap: true
        antialiasing: true

        MouseArea {
            anchors.fill: parent
            onClicked: rect.clicked()
        }
    }
}
