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
            spacing: 11

            // topbar
            Rectangle {
                id: topBar
                width: parent.width
                height: 108
                color: "#2F2F37"

                RowLayout {
                    id: topBarLayout
                    anchors.fill: parent
                    spacing: 22
                    anchors.margins: 11

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

            // quick actions
            Rectangle {
                id: quickActions
                width: parent.width
                height: 92
                color: "#18171C"

                ColumnLayout {
                    width: parent.width
                    spacing: 11

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
                        contentHeight: quickActionsButtons.height + 11

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
                                implicitWidth: 50
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
                            spacing: 16
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
                                width: 98
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
                                width: 98
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
                height: 177
                color: "#18171C"

                ColumnLayout {
                    width: parent.width
                    spacing: 11

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
                            spacing: 16

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
                height: 300
                anchors.bottom: bottomBar.bottom

                RowLayout {
                    width: parent.width

                    Text {
                        id: favouriteRoomsText
                        font.pixelSize: 14
                        font.bold: true
                        text: qsTr("Favourite Rooms")
                        color: "#FFFFFF"
                        Layout.alignment: Qt.AlignLeft
                    }

                    Text {
                        id: favouriteRoomsEdit
                        font.pixelSize: 14
                        font.bold: true
                        text: qsTr("Edit")
                        color: "#FFFFFF"
                        Layout.alignment: Qt.AlignRight
                    }
                }

                Rectangle {
                    id: rooms
                    color: "#18171C"
                    width: parent.width
                    height: parent.height

                    ScrollView {
                        id: roomsScrollView
                        width: parent.width
                        height: parent.height

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
                                implicitHeight: 30
                                radius: 1
                                color: "#FFFFFF"
                            }

                            background: Rectangle {
                                color: "transparent"
                            }
                        }

                        GridLayout {
                            id: roomsGrid
                            width: parent.width
                            height: parent.height
                            columns: 2
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

            // bottom bar
            RowLayout {
                id: bottomBar
                width: parent.width
                height: 25
                anchors.bottom: parent.bottom
                spacing: 16

                BottomBarButton {
                    id: homeButton
                    width: 78
                    height: 40
                    text: "Home"
                    color: "red"
                }

                BottomBarButton {
                    id: roomsButton
                    width: 78
                    height: 40
                    text: "Devices"
                    color: "green"
                }

                BottomBarButton {
                    id: devicesButton
                    width: 78
                    height: 40
                    text: "Rooms"
                    color: "blue"
                }

                BottomBarButton {
                    id: statsButton
                    width: 78
                    height: 40
                    text: "Stats"
                    color: "orange"
                }
            }
        }
    }
}
