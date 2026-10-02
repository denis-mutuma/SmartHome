import QtQuick
import QtQuick.Controls

Button {
    id: control
    property color fill: Theme.active
    property color labelColor: Theme.text

    focusPolicy: Qt.StrongFocus
    Accessible.name: text
    font.pixelSize: Theme.labelSize
    background: Rectangle {
        color: control.enabled ? control.fill : Theme.card
        radius: 8
        border.width: 2
        border.color: control.visualFocus ? Theme.active : "transparent"
    }
    contentItem: Text {
        text: control.text
        color: control.enabled ? control.labelColor : Theme.muted
        font: control.font
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
    }
}
