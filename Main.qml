import QtQuick
import QtQuick.Window
import QtQuick.Layouts
import QtQuick.Controls

Window {
    id: root

    width: Screen.width
    height: Screen.height
    visible: true
    title: qsTr("Smart Home")

    Rectangle {
        id: homePage
        anchors.fill: parent
        color: "#18171C"

        // homepage layout
        ColumnLayout {
            width: parent.width
            spacing: 22

            // topbar
            Rectangle {
                id: topBar
                width: parent.width
                height: 100
                color: "#2F2F37"

                RowLayout {
                    anchors.fill: parent
                    spacing: 22

                    ColumnLayout {
                        Layout.alignment: Qt.AlignVCenter

                        Text {
                            id: salutation
                            font.pixelSize: 14
                            font.bold: true
                            text: qsTr("Good Morning,")
                            color: "#FFFFFF"
                        }

                        Text {
                            id: userName
                            font.pixelSize: 32
                            font.bold: true
                            text: qsTr("Mutuma")
                            color: "#FFFFFF"
                        }
                    }

                    ColumnLayout {
                        Layout.alignment: Qt.AlignVCenter

                        Rectangle {
                            width: 30
                            height: 30
                            color: "transparent"

                            Image {
                                id: weatherIcon
                                mipmap: true
                                antialiasing: true
                                anchors.fill: parent
                                source: "assets/icons/sun-cloud.svg"
                            }
                        }

                        Text {
                            id: weatherDetails
                            text: qsTr("Partly Cloudy")
                            font.pixelSize: 20
                            color: "#FFFFFF"
                        }

                        Text {
                            id: cityName
                            font.pixelSize: 14
                            text: qsTr("Nairobi")
                            color: "#FFFFFF"
                        }
                    }

                    ColumnLayout {
                        Layout.alignment: Qt.AlignVCenter

                        RowLayout {
                            spacing: 30

                            ClickableImage {
                                id: settingsIcon
                                width: 30
                                height: 30
                                source: "assets/icons/settings.svg"

                                onClicked: {
                                    console.log("settings clicked")
                                }
                            }

                            ClickableImage {
                                id: notificationsIcon
                                width: 30
                                height: 30
                                source: "assets/icons/settings.svg"

                                onClicked: {
                                    console.log("notifications clicked")
                                }
                            }

                        }
                    }
                }
            }

            // quick actions
            Rectangle {
                id: quickActions
                width: parent.width
                height: 100
                color: "#18171C"

                ColumnLayout {
                    width: parent.width
                    spacing: 22

                    RowLayout {
                        width: parent.width

                        Text {
                            id: quickActionsText
                            font.pixelSize: 14
                            font.bold: true
                            text: qsTr("Quick Actions")
                            color: "#FFFFFF"
                            Layout.alignment: Qt.AlignLeft
                        }

                        Text {
                            id: quickActionsEditText
                            font.pixelSize: 14
                            font.bold: true
                            text: qsTr("Edit")
                            color: "#FFFFFF"
                            Layout.alignment: Qt.AlignRight
                        }
                    }

                    ScrollView {
                        id: quickActionScrollView
                        width: parent.width
                        height: parent.height
                        clip: true
                        contentHeight: quickActionsButtons.height + 18

                        ScrollBar.horizontal: ScrollBar {
                            id: quickActionScrollBar
                            active: true
                            orientation: Qt.Horizontal
                            interactive: true

                            anchors {
                                left: parent.left
                                right: parent.right
                                bottom: parent.bottom
                            }

                            contentItem: Rectangle {
                                implicitWidth: 100
                                implicitHeight: 2
                                radius: 1
                                color: "#FFFFFF"
                            }

                            background: Rectangle {
                                color: "transparent"
                            }
                        }

                        RowLayout {
                            id: quickActionsButtons
                            spacing: 22
                            width: parent.width

                            QuickActionButton {
                                id: goodMorning
                                width: 98
                                height: 50
                                text: qsTr("Good Morning")
                                color: "#2F2F37"
                                active: false
                                source: active ? "assets/icons/yellow-sun.svg" : "assets/icons/white-sun.svg"

                                onClicked: {
                                    active = !active
                                }

                            }

                            QuickActionButton {
                                id: leaveHome
                                width: 98
                                height: 50
                                text: qsTr("Leave Home")
                                color: "#2F2F37"
                                active: false
                                source: active ? "assets/icons/green-home.svg" : "assets/icons/white-home.svg"

                                onClicked: {
                                    active = !active
                                }
                            }

                            QuickActionButton {
                                id: arriveHome
                                width: 78
                                height: 50
                                text: qsTr("Arrive Home")
                                color: "#2F2F37"
                                active: false
                                source: active ? "assets/icons/green-home.svg" : "assets/icons/white-home.svg"

                                onClicked: {
                                    active = !active
                                }

                            }

                            QuickActionButton {
                                id: goodNight
                                width: 78
                                height: 50
                                text: qsTr("Good Night")
                                color: "#2F2F37"
                                active: false
                                source: active ? "assets/icons/white-moon.svg" : "assets/icons/grey-moon.svg"

                                onClicked: {
                                    active = !active
                                }
                            }
                        }
                    }

                }
            }

            // frequently used
            Rectangle {
                id: frequentlyUsed
                width: parent.width
                height: 188
                color: "#18171C"

                ColumnLayout {
                    width: parent.width
                    spacing: 22

                    Text {
                        id: frequentlyUsedText
                        font.pixelSize: 14
                        font.bold: true
                        text: qsTr("Frequently Used")
                        color: "#FFFFFF"
                        Layout.alignment: Qt.AlignLeft
                    }

                    ScrollView {
                        id: frequentlyUsedScrollView
                        width: parent.width
                        height: parent.height
                        clip: true
                        contentHeight: frequentlyUsedButtons.height + 18

                        ScrollBar.horizontal: ScrollBar {
                            id: frequentlyUsedScrollBar
                            active: true
                            orientation: Qt.Horizontal
                            interactive: true

                            anchors {
                                left: parent.left
                                right: parent.right
                                bottom: parent.bottom
                            }

                            contentItem: Rectangle {
                                implicitWidth: 100
                                implicitHeight: 2
                                radius: 1
                                color: "#FFFFFF"
                            }

                            background: Rectangle {
                                color: "transparent"
                            }
                        }

                        RowLayout {
                            id: frequentlyUsedButtons
                            width: parent.width
                            spacing: 22

                            FrequentlyUsedButton {
                                active: true
                                room: "Living room"
                                device: "Lights"
                                deviceStatus: "On"
                            }

                            FrequentlyUsedButton {
                                active: false
                                room: "Kitchen"
                                device: "Lights"
                                deviceStatus: "Off"
                            }

                            FrequentlyUsedButton {
                                active: true
                                room: "Bedroom"
                                device: "Lights"
                                deviceStatus: "On"
                            }

                            FrequentlyUsedButton {
                                active: false
                                room: "Garage"
                                device: "Lights"
                                deviceStatus: "Off"
                            }

                            FrequentlyUsedButton {
                                active: true
                                room: "Living room"
                                device: "Sound system"
                                deviceStatus: "On"
                            }

                            FrequentlyUsedButton {
                                active: false
                                room: "Kitchen"
                                device: "Toaster"
                                deviceStatus: "Off"
                            }

                            FrequentlyUsedButton {
                                active: false
                                room: "Bedroom"
                                device: "Fan"
                                deviceStatus: "Off"
                            }

                            FrequentlyUsedButton {
                                active: true
                                room: "Living room"
                                device: "Fan"
                                deviceStatus: "On"
                            }
                        }
                    }
                }
            }

            // Favourite rooms
            ColumnLayout {
                width: parent.width
                height: parent.height
                spacing: 22

                Text {
                    id: roomsText
                    font.pixelSize: 14
                    font.bold: true
                    text: qsTr("Favourite Rooms")
                    color: "#FFFFFF"
                    Layout.alignment: Qt.AlignLeft
                }

                Rectangle {
                    id: rooms
                    color: "#18171C"
                    width: parent.width
                    height: 300

                    ScrollView {
                        id: roomsScrollView
                        width: parent.width
                        height: parent.height
                        clip: true
                        anchors.fill: parent

                        GridView {
                            id: roomsGrid
                            width: parent.width
                            height: parent.height
                            clip: true
                            cellWidth: 150
                            cellHeight: 150
                            flow: GridView.FlowTopToBottom
                            cacheBuffer: 4096

                            ScrollBar.vertical: ScrollBar {
                                id: roomsScrollBar
                                active: true
                                orientation: Qt.Vertical
                                interactive: true

                                anchors {
                                    right: parent.right
                                    top: parent.top
                                    bottom: parent.bottom
                                }

                                contentItem: Rectangle {
                                    implicitWidth: 2
                                    implicitHeight: 100
                                    radius: 1
                                    color: "#FFFFFF"
                                }

                                background: Rectangle {
                                    color: "transparent"
                                }
                            }

                            model: 1

                            delegate: GridLayout {
                                id: roomsGridLayout
                                width: parent.width
                                height: parent.height
                                columns: 2
                                rowSpacing: 22
                                columnSpacing: 22

                                FavouriteRoomsButton {
                                    room: "Living room"
                                    deviceCount: 8
                                    source: "assets/images/living-room.jpeg"
                                }

                                FavouriteRoomsButton {
                                    room: "Living room"
                                    deviceCount: 8
                                    source: "assets/images/living-room.jpeg"
                                }

                                FavouriteRoomsButton {
                                    room: "Living room"
                                    deviceCount: 8
                                    source: "assets/images/living-room.jpeg"
                                }

                                FavouriteRoomsButton {
                                    room: "Living room"
                                    deviceCount: 8
                                    source: "assets/images/living-room.jpeg"
                                }

                                FavouriteRoomsButton {
                                    room: "Living room"
                                    deviceCount: 8
                                    source: "assets/images/living-room.jpeg"
                                }

                                FavouriteRoomsButton {
                                    room: "Living room"
                                    deviceCount: 8
                                    source: "assets/images/living-room.jpeg"
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}
