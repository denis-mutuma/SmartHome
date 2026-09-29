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
            text: qsTr("Create account")
            color: Theme.text
            font.pixelSize: Theme.nameSize
            font.bold: true
        }
        TextField {
            id: nameField
            Layout.fillWidth: true
            placeholderText: qsTr("First name")
            color: Theme.text
            font.pixelSize: Theme.labelSize
            Accessible.name: placeholderText
            background: Rectangle { color: Theme.card; radius: 8 }
        }
        TextField {
            id: emailField
            Layout.fillWidth: true
            placeholderText: qsTr("Email")
            color: Theme.text
            font.pixelSize: Theme.labelSize
            inputMethodHints: Qt.ImhEmailCharactersOnly
            Accessible.name: placeholderText
            background: Rectangle { color: Theme.card; radius: 8 }
        }
        TextField {
            id: passwordField
            Layout.fillWidth: true
            placeholderText: qsTr("Password")
            echoMode: TextInput.Password
            color: Theme.text
            font.pixelSize: Theme.labelSize
            Accessible.name: placeholderText
            background: Rectangle { color: Theme.card; radius: 8 }
        }
        Label {
            text: page.session.statusMessage
            color: Theme.danger
            font.pixelSize: Theme.secondarySize
            wrapMode: Text.WordWrap
            visible: text.length > 0
            Layout.fillWidth: true
        }
        ActionButton {
            text: qsTr("Create account")
            Layout.fillWidth: true
            onClicked: page.session.registerAccount(nameField.text, emailField.text, passwordField.text)
        }
        Item { Layout.fillHeight: true }
    }
}
