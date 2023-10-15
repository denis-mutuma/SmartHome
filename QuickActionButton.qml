import QtQuick
import QtQuick.Layouts

Item {
    id: button
    width: 98
    height: 50

    property bool active: false
    property string text: ""
    property alias color: rect.color
    property alias source: img.source

    signal clicked

    Rectangle {
        id: rect
        anchors.fill: parent
        radius: 10
        border.color: Qt.lighter(color, 1.2)

        Behavior on color {
            ColorAnimation {
                duration: 50
            }
        }
    }

    RowLayout {
        anchors.fill: parent
        spacing: 5

        Text {
            id: textLabel
            Layout.fillWidth: true
            Layout.fillHeight: true
            wrapMode: Text.WordWrap
            text: button.text
            color: "#FFFFFF"
            font.pixelSize: 14
            font.bold: true
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
        }

        Image {
            id: img
            width: 25
            height: 25
            fillMode: Image.PreserveAspectFit
            source: button.source
            horizontalAlignment: Image.AlignHCenter
            verticalAlignment: Image.AlignVCenter
        }
    }

    MouseArea {
        anchors.fill: parent
        onClicked: button.clicked()
    }

    onActiveChanged: {
        if (active) {
            button.color = "#536DED"
        } else {
            button.color = "#2F2F37"
        }
    }

    onTextChanged: {
        textLabel.text = button.text
    }
}
