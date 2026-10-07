cmake_minimum_required(VERSION 3.22)

project(SmartHome VERSION 0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(Qt6 6.11 REQUIRED COMPONENTS Quick Gui Network Test)

qt_standard_project_setup(REQUIRES 6.11)
set(QT_USE_TARGET_ANDROID_BUILD_DIR TRUE CACHE BOOL "Separate Android build directory for each executable")

if(EXISTS "${CMAKE_SOURCE_DIR}/config.local.cmake")
    include("${CMAKE_SOURCE_DIR}/config.local.cmake")
endif()
if(NOT DEFINED SMARTHOME_SUPABASE_URL)
    set(SMARTHOME_SUPABASE_URL "")
endif()
if(NOT DEFINED SMARTHOME_SUPABASE_ANON_KEY)
    set(SMARTHOME_SUPABASE_ANON_KEY "")
endif()

qt_add_library(smarthome_core STATIC
    src/core/home_rules.cpp
    src/core/home_json.cpp
    src/app/api_client.cpp
    src/app/token_store.cpp
)
target_include_directories(smarthome_core PUBLIC src/core src/app)
if(WIN32)
    target_link_libraries(smarthome_core PUBLIC crypt32)
endif()
target_link_libraries(smarthome_core PUBLIC Qt6::Core Qt6::Network)
if(MSVC)
    target_compile_options(smarthome_core PRIVATE /W4)
else()
    target_compile_options(smarthome_core PRIVATE -Wall -Wextra -Wpedantic)
endif()

enable_testing()
qt_add_executable(tst_home_rules tests/tst_home_rules.cpp)
target_link_libraries(tst_home_rules PRIVATE smarthome_core Qt6::Test)
add_test(NAME home_rules COMMAND tst_home_rules)
qt_add_executable(tst_home_json tests/tst_home_json.cpp)
target_link_libraries(tst_home_json PRIVATE smarthome_core Qt6::Test)
add_test(NAME home_json COMMAND tst_home_json)
qt_add_executable(tst_token_store tests/tst_token_store.cpp)
target_link_libraries(tst_token_store PRIVATE smarthome_core Qt6::Test)
add_test(NAME token_store COMMAND tst_token_store)
qt_add_executable(tst_api_client tests/tst_api_client.cpp)
target_link_libraries(tst_api_client PRIVATE smarthome_core Qt6::Test)
add_test(NAME api_client COMMAND tst_api_client)
qt_add_executable(tst_session_controller
    tests/tst_session_controller.cpp
    src/app/session_controller.cpp
    src/app/session_controller.h
)
target_link_libraries(tst_session_controller PRIVATE smarthome_core Qt6::Test Qt6::Gui)
target_compile_definitions(tst_session_controller PRIVATE
    "SMARTHOME_SUPABASE_URL=\"${SMARTHOME_SUPABASE_URL}\""
    "SMARTHOME_SUPABASE_ANON_KEY=\"${SMARTHOME_SUPABASE_ANON_KEY}\"")
add_test(NAME session_controller COMMAND tst_session_controller)
if(ANDROID)
    foreach(android_test_target IN ITEMS tst_home_rules tst_home_json tst_token_store tst_api_client)
        target_link_libraries(${android_test_target} PRIVATE Qt6::Gui)
    endforeach()
    set_property(TARGET tst_token_store PROPERTY QT_ANDROID_PACKAGE_SOURCE_DIR
        "${CMAKE_SOURCE_DIR}/android")
endif()
if(WIN32)
    cmake_path(GET Qt6_DIR PARENT_PATH _qt_cmake_dir)
    cmake_path(GET _qt_cmake_dir PARENT_PATH _qt_lib_dir)
    cmake_path(GET _qt_lib_dir PARENT_PATH _qt_prefix)
    cmake_path(GET CMAKE_CXX_COMPILER PARENT_PATH _cxx_bin)
    set_property(TEST home_rules PROPERTY ENVIRONMENT_MODIFICATION
        "PATH=path_list_prepend:${_qt_prefix}/bin"
        "PATH=path_list_prepend:${_cxx_bin}")
    set_property(TEST home_json PROPERTY ENVIRONMENT_MODIFICATION
        "PATH=path_list_prepend:${_qt_prefix}/bin"
        "PATH=path_list_prepend:${_cxx_bin}")
    set_property(TEST token_store PROPERTY ENVIRONMENT_MODIFICATION
        "PATH=path_list_prepend:${_qt_prefix}/bin"
        "PATH=path_list_prepend:${_cxx_bin}")
    set_property(TEST api_client PROPERTY ENVIRONMENT_MODIFICATION
        "PATH=path_list_prepend:${_qt_prefix}/bin"
        "PATH=path_list_prepend:${_cxx_bin}")
    set_property(TEST session_controller PROPERTY ENVIRONMENT_MODIFICATION
        "PATH=path_list_prepend:${_qt_prefix}/bin"
        "PATH=path_list_prepend:${_cxx_bin}")
endif()

qt_add_executable(appSmartHome
    main.cpp
)

set_source_files_properties(AccountMain.qml PROPERTIES QT_RESOURCE_ALIAS Main.qml)
qt_add_qml_module(appSmartHome
    URI SmartHome
    VERSION 1.0
    SOURCES
        src/app/session_controller.cpp
        src/app/session_controller.h
    QML_FILES AccountMain.qml

    RESOURCES
    README.md
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
if(ANDROID)
    set_property(TARGET appSmartHome PROPERTY QT_ANDROID_PACKAGE_NAME
        "org.mutuma.smarthome")
    set_property(TARGET appSmartHome PROPERTY QT_ANDROID_PACKAGE_SOURCE_DIR
        "${CMAKE_SOURCE_DIR}/android")
endif()

target_link_libraries(appSmartHome
    PRIVATE Qt6::Quick smarthome_core
)
target_compile_definitions(appSmartHome PRIVATE
    "SMARTHOME_SUPABASE_URL=\"${SMARTHOME_SUPABASE_URL}\""
    "SMARTHOME_SUPABASE_ANON_KEY=\"${SMARTHOME_SUPABASE_ANON_KEY}\"")

include(GNUInstallDirs)
install(TARGETS appSmartHome
    BUNDLE DESTINATION .
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
