pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import SmartHome

Window {
    id: root

    width: 440
    height: 720
    minimumWidth: 320
    minimumHeight: 560
    visible: true
    title: qsTr("SmartHome")
    color: "#18171C"

    SessionController {
        id: session
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 28
        spacing: 0

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 4
            color: "#536DED"
        }

        Item {
            Layout.fillHeight: true
        }

        StackLayout {
            Layout.fillWidth: true
            Layout.preferredHeight: 520
            currentIndex: session.signedIn ? 1 : 0

            Item {
                ColumnLayout {
                    anchors.centerIn: parent
                    width: Math.min(parent.width, 380)
                    spacing: 16

                    Label {
                        Layout.fillWidth: true
                        text: qsTr("SmartHome")
                        color: "#FFFFFF"
                        font.pixelSize: 32
                        font.bold: true
                    }

                    TabBar {
                        id: accountMode
                        Layout.fillWidth: true
                        Accessible.name: qsTr("Account mode")

                        TabButton {
                            text: qsTr("Sign in")
                            Accessible.name: qsTr("Sign in")
                        }

                        TabButton {
                            text: qsTr("Register")
                            Accessible.name: qsTr("Register")
                        }
                    }

                    TextField {
                        id: firstNameInput
                        visible: accountMode.currentIndex === 1
                        Layout.fillWidth: true
                        Layout.preferredHeight: visible ? 50 : 0
                        placeholderText: qsTr("First name")
                        Accessible.name: qsTr("First name")
                        color: "#FFFFFF"

                        background: Rectangle {
                            color: "#2F2F37"
                            radius: 4
                        }
                    }

                    TextField {
                        id: emailInput
                        Layout.fillWidth: true
                        Layout.preferredHeight: 50
                        placeholderText: qsTr("Email")
                        inputMethodHints: Qt.ImhEmailCharactersOnly
                        Accessible.name: qsTr("Email")
                        color: "#FFFFFF"

                        background: Rectangle {
                            color: "#2F2F37"
                            radius: 4
                        }
                    }

                    TextField {
                        id: passwordInput
                        Layout.fillWidth: true
                        Layout.preferredHeight: 50
                        placeholderText: qsTr("Password")
                        echoMode: TextInput.Password
                        Accessible.name: qsTr("Password")
                        color: "#FFFFFF"
                        onAccepted: submitButton.clicked()

                        background: Rectangle {
                            color: "#2F2F37"
                            radius: 4
                        }
                    }

                    Label {
                        Layout.fillWidth: true
                        visible: session.statusMessage.length > 0
                        text: session.statusMessage
                        color: "#FFFFFF"
                        wrapMode: Text.Wrap
                        Accessible.name: qsTr("Account status")
                    }

                    Button {
                        id: submitButton
                        Layout.fillWidth: true
                        Layout.preferredHeight: 50
                        text: accountMode.currentIndex === 0 ? qsTr("Sign in") : qsTr("Create account")
                        Accessible.name: text
                        onClicked: {
                            if (accountMode.currentIndex === 0) {
                                session.signIn(emailInput.text, passwordInput.text)
                            } else {
                                session.registerAccount(firstNameInput.text, emailInput.text, passwordInput.text)
                            }
                        }

                        background: Rectangle {
                            color: "#536DED"
                            radius: 4
                        }
                    }
                }
            }

            Item {
                ColumnLayout {
                    anchors.fill: parent
                    spacing: 12

                    Label {
                        Layout.fillWidth: true
                        text: session.greeting
                        color: "#FFFFFF"
                        font.pixelSize: 26
                        font.bold: true
                        Accessible.name: qsTr("Greeting")
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 10

                        Image {
                            visible: session.weatherIcon.length > 0
                            source: session.weatherIcon
                            sourceSize.width: 24
                            sourceSize.height: 24
                            fillMode: Image.PreserveAspectFit
                            Accessible.name: qsTr("Weather icon")
                        }

                        Label {
                            Layout.fillWidth: true
                            text: session.weatherLine.length > 0 ? session.weatherLine : qsTr("Weather unavailable")
                            color: "#FFFFFF"
                            wrapMode: Text.Wrap
                            Accessible.name: qsTr("Current weather")
                        }

                        Label {
                            text: session.city
                            color: "#FFFFFF"
                            Accessible.name: qsTr("Saved city")
                        }
                    }

                    Label {
                        Layout.fillWidth: true
                        text: qsTr("Rooms")
                        color: "#FFFFFF"
                        font.pixelSize: 16
                        font.bold: true
                    }

                    Label {
                        Layout.fillWidth: true
                        visible: session.rooms.length === 0
                        text: qsTr("No rooms yet. Add your first room below.")
                        color: "#FFFFFF"
                        wrapMode: Text.Wrap
                        Accessible.name: qsTr("Empty rooms message")
                    }

                    ListView {
                        id: roomList
                        visible: session.rooms.length > 0
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        Layout.minimumHeight: 0
                        clip: true
                        spacing: 8
                        model: session.rooms

                        delegate: Rectangle {
                            id: roomDelegate
                            required property var modelData
                            width: roomList.width
                            height: 58
                            color: "#2F2F37"
                            radius: 4

                            RowLayout {
                                anchors.fill: parent
                                anchors.leftMargin: 14
                                anchors.rightMargin: 14

                                Label {
                                    Layout.fillWidth: true
                                    text: roomDelegate.modelData.name
                                    color: "#FFFFFF"
                                    Accessible.name: qsTr("Room name")
                                }

                                Label {
                                    text: qsTr("%1 devices").arg(roomDelegate.modelData.deviceCount)
                                    color: "#FFFFFF"
                                    Accessible.name: qsTr("Device count")
                                }
                            }
                        }
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 8

                        TextField {
                            id: roomNameInput
                            Layout.fillWidth: true
                            Layout.preferredHeight: 46
                            placeholderText: qsTr("Room name")
                            Accessible.name: qsTr("Room name")
                            color: "#FFFFFF"

                            background: Rectangle {
                                color: "#2F2F37"
                                radius: 4
                            }
                        }

                        Button {
                            Layout.preferredHeight: 46
                            text: qsTr("Add room")
                            Accessible.name: qsTr("Add room")
                            onClicked: {
                                if (session.createRoom(roomNameInput.text)) {
                                    roomNameInput.clear()
                                }
                            }

                            background: Rectangle {
                                color: "#536DED"
                                radius: 4
                            }
                        }
                    }

                    Label {
                        Layout.fillWidth: true
                        visible: session.statusMessage.length > 0
                        text: session.statusMessage
                        color: "#FFFFFF"
                        wrapMode: Text.Wrap
                        Accessible.name: qsTr("Account status")
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 8

                        Label {
                            Layout.fillWidth: true
                            text: session.email
                            color: "#FFFFFF"
                            elide: Text.ElideRight
                            Accessible.name: qsTr("Signed-in email")
                        }

                        Button {
                            text: qsTr("Sign out")
                            Accessible.name: qsTr("Sign out")
                            onClicked: session.signOut()

                            background: Rectangle {
                                color: "#2F2F37"
                                radius: 4
                            }
                        }
                    }
                }
            }
        }

        Item {
            Layout.fillHeight: true
        }
    }
}