cmake_minimum_required(VERSION 3.16)

project(SmartHome VERSION 0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 6.4 REQUIRED COMPONENTS Quick)

qt_standard_project_setup()

qt_add_executable(appSmartHome
    main.cpp
)

qt_add_qml_module(appSmartHome
    URI SmartHome
    VERSION 1.0
    QML_FILES Main.qml
    QML_FILES ClickableImage.qml
    QML_FILES QuickActionButton.qml
    QML_FILES FrequentlyUsedButton.qml
    QML_FILES FavouriteRoomsButton.qml
    QML_FILES BottomBarButton.qml

    README.md

    RESOURCES
    "assets/icons/settings.svg"
    "assets/icons/sun-cloud.svg"
    "assets/icons/light-on.svg"
    "assets/icons/light-off.svg"
    "assets/icons/toggle-on.svg"
    "assets/icons/toggle-off.svg"
    "assets/icons/white-moon.svg"
    "assets/icons/grey-moon.svg"
    "assets/icons/yellow-sun.svg"
    "assets/icons/white-sun.svg"
    "assets/icons/green-home.svg"
    "assets/icons/white-home.svg"

    "assets/images/bedroom.jpeg"
    "assets/images/kitchen.jpeg"
    "assets/images/garage.jpg"
    "assets/images/living-room.jpeg"
    "assets/images/mutuma.jpg"

)

# Qt for iOS sets MACOSX_BUNDLE_GUI_IDENTIFIER automatically since Qt 6.1.
# If you are developing for iOS or macOS you should consider setting an
# explicit, fixed bundle identifier manually though.
set_target_properties(appSmartHome PROPERTIES
#    MACOSX_BUNDLE_GUI_IDENTIFIER com.example.appSmartHome
    MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
    MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}
    MACOSX_BUNDLE TRUE
    WIN32_EXECUTABLE TRUE
)

target_link_libraries(appSmartHome
    PRIVATE Qt6::Quick
)

include(GNUInstallDirs)
install(TARGETS appSmartHome
    BUNDLE DESTINATION .
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
