import QtQuick
import QtQuick.Controls

Button {
    id: control
    property color fill: Theme.active
    property color labelColor: Theme.text

    Accessible.name: text
    font.pixelSize: Theme.labelSize
    background: Rectangle {
        color: control.fill
        radius: 8
    }
    contentItem: Text {
        text: control.text
        color: control.labelColor
        font: control.font
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
    }
}
