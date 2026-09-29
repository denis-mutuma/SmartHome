import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Page {
    id: page
    signal openRegister()
    required property var session

    background: Rectangle { color: Theme.background }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 24
        spacing: 12

        Label {
            text: qsTr("SmartHome")
            color: Theme.text
            font.pixelSize: Theme.nameSize
            font.bold: true
            Accessible.name: text
        }
        Label {
            text: qsTr("Sign in to see your home.")
            color: Theme.muted
            font.pixelSize: Theme.labelSize
            wrapMode: Text.WordWrap
            Layout.fillWidth: true
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
            text: qsTr("Sign in")
            Layout.fillWidth: true
            onClicked: page.session.signIn(emailField.text, passwordField.text)
        }
        ActionButton {
            text: qsTr("Create account")
            fill: Theme.card
            Layout.fillWidth: true
            onClicked: page.openRegister()
        }
        Item { Layout.fillHeight: true }
    }
}
