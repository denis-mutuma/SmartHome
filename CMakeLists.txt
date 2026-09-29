cmake_minimum_required(VERSION 3.22)

project(SmartHome VERSION 0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(Qt6 6.11 REQUIRED COMPONENTS Quick QuickControls2 Network Test)

qt_standard_project_setup(REQUIRES 6.11)
set(QT_USE_TARGET_ANDROID_BUILD_DIR TRUE CACHE BOOL "Separate Android build directory for each executable")

if(EXISTS "${CMAKE_SOURCE_DIR}/config.local.cmake")
    include("${CMAKE_SOURCE_DIR}/config.local.cmake")
endif()

qt_add_library(smarthome_core STATIC
    src/core/home_rules.cpp
    src/core/home_json.cpp
    src/app/token_store.cpp
)
target_include_directories(smarthome_core PUBLIC src/core src/app)
if(WIN32)
    target_link_libraries(smarthome_core PUBLIC crypt32)
endif()
target_link_libraries(smarthome_core PUBLIC Qt6::Core)
if(MSVC)
    target_compile_options(smarthome_core PRIVATE /W4)
else()
    target_compile_options(smarthome_core PRIVATE -Wall -Wextra -Wpedantic)
endif()

enable_testing()
if(WIN32)
    cmake_path(GET Qt6_DIR PARENT_PATH _qt_cmake_dir)
    cmake_path(GET _qt_cmake_dir PARENT_PATH _qt_lib_dir)
    cmake_path(GET _qt_lib_dir PARENT_PATH _qt_prefix)
    cmake_path(GET CMAKE_CXX_COMPILER PARENT_PATH _cxx_bin)
endif()

function(smarthome_add_test target source)
    qt_add_executable(${target} ${source})
    set_target_properties(${target} PROPERTIES WIN32_EXECUTABLE FALSE)
    target_link_libraries(${target} PRIVATE smarthome_core Qt6::Test)
    add_test(NAME ${target} COMMAND ${target})
    if(WIN32)
        set_property(TEST ${target} PROPERTY ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:${_qt_prefix}/bin"
            "PATH=path_list_prepend:${_cxx_bin}")
    endif()
endfunction()

smarthome_add_test(tst_home_rules tests/tst_home_rules.cpp)
smarthome_add_test(tst_home_json tests/tst_home_json.cpp)
smarthome_add_test(tst_token_store tests/tst_token_store.cpp)

if(NOT DEFINED SMARTHOME_SUPABASE_URL)
    set(SMARTHOME_SUPABASE_URL "")
endif()
if(NOT DEFINED SMARTHOME_SUPABASE_ANON_KEY)
    set(SMARTHOME_SUPABASE_ANON_KEY "")
endif()

qt_add_executable(appSmartHome
    main.cpp
)

set_source_files_properties(qml/Theme.qml PROPERTIES QT_QML_SINGLETON_TYPE TRUE)

qt_add_qml_module(appSmartHome
    URI SmartHome
    VERSION 1.0
    SOURCES
        src/app/api_client.h
        src/app/api_client.cpp
        src/app/session_controller.h
        src/app/session_controller.cpp
    QML_FILES
        qml/Main.qml
        qml/Theme.qml
        qml/LoginPage.qml
        qml/RegisterPage.qml
        qml/HomePage.qml
        qml/RoomPage.qml
        qml/DeviceEditPage.qml
        qml/SettingsPage.qml
        qml/ActionButton.qml
    RESOURCES
        assets/icons/sun-cloud.svg
        assets/icons/light-on.svg
        assets/icons/light-off.svg
        assets/icons/toggle-on.svg
        assets/icons/toggle-off.svg
        assets/icons/white-moon.svg
        assets/icons/yellow-sun.svg
        assets/icons/plug.svg
        assets/icons/thermometer.svg
        assets/images/mutuma.jpg
)

set_target_properties(appSmartHome PROPERTIES
    QT_ANDROID_PACKAGE_NAME org.mutuma.smarthome
    MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
    MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}
    MACOSX_BUNDLE TRUE
    WIN32_EXECUTABLE TRUE
)

target_include_directories(appSmartHome PRIVATE src/core src/app)
target_compile_definitions(appSmartHome PRIVATE
    "SMARTHOME_SUPABASE_URL=\"${SMARTHOME_SUPABASE_URL}\""
    "SMARTHOME_SUPABASE_ANON_KEY=\"${SMARTHOME_SUPABASE_ANON_KEY}\""
)
if(MSVC)
    target_compile_options(appSmartHome PRIVATE /W4)
else()
    target_compile_options(appSmartHome PRIVATE -Wall -Wextra -Wpedantic)
endif()

target_link_libraries(appSmartHome
    PRIVATE smarthome_core Qt6::Quick Qt6::QuickControls2 Qt6::Network
)
