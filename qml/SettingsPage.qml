import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Page {
    id: page
    required property var session

    background: Rectangle { color: Theme.background }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 24
        spacing: 12

        ActionButton {
            text: qsTr("Back")
            fill: Theme.card
            onClicked: StackView.view.pop()
        }
        Label {
            text: qsTr("Settings")
            color: Theme.text
            font.pixelSize: Theme.nameSize
            font.bold: true
        }
        TextField {
            id: nameField
            Layout.fillWidth: true
            text: page.session.firstName
            placeholderText: qsTr("First name")
            color: Theme.text
            font.pixelSize: Theme.labelSize
            Accessible.name: placeholderText
            background: Rectangle { color: Theme.card; radius: 8 }
        }
        TextField {
            id: cityField
            Layout.fillWidth: true
            text: page.session.city
            placeholderText: qsTr("City")
            color: Theme.text
            font.pixelSize: Theme.labelSize
            Accessible.name: placeholderText
            background: Rectangle { color: Theme.card; radius: 8 }
        }
        Label {
            text: qsTr("Weather uses this city. Leave it empty to hide the weather.")
            color: Theme.muted
            font.pixelSize: Theme.secondarySize
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
            onClicked: {
                if (page.session.saveSettings(nameField.text, cityField.text))
                    StackView.view.pop()
            }
        }
        ActionButton {
            text: qsTr("Log out")
            fill: Theme.card
            Layout.fillWidth: true
            onClicked: page.session.signOut()
        }
        Item { Layout.fillHeight: true }
    }
}
