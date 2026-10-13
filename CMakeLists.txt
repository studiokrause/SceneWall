cmake_minimum_required(VERSION 3.16)
project(SceneWall LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)

# Configure OBS SDK locations (no hardcoded paths)
set(OBS_SDK_DIR "" CACHE PATH "Path to the installed OBS SDK")
if(NOT OBS_SDK_DIR)
    message(FATAL_ERROR "Set OBS_SDK_DIR to the installed OBS SDK, e.g. -DOBS_SDK_DIR=<sdk>")
endif()

set(OBS_QT_CMAKE_DIR "" CACHE PATH "Optional path to the Qt6 CMake package (lib/cmake/Qt6)")
if(OBS_QT_CMAKE_DIR)
    list(APPEND CMAKE_PREFIX_PATH "${OBS_QT_CMAKE_DIR}")
endif()

include_directories("${OBS_SDK_DIR}/include")
link_directories("${OBS_SDK_DIR}/lib")

find_package(Qt6 REQUIRED COMPONENTS Widgets)

add_library(SceneWall MODULE
    src/plugin-main.cpp
    src/SceneWall.cpp
    src/SettingsDialog.cpp
    src/AboutDialog.h
    src/FlowLayout.h
    src/Config.h
    src/SceneContainer.h
    src/SceneTabBar.h
    src/ToggleSwitch.h
)

target_link_libraries(SceneWall PRIVATE obs.lib obs-frontend-api.lib Qt6::Widgets)

set_target_properties(SceneWall PROPERTIES PREFIX "")

include(CTest)
if(BUILD_TESTING)
    add_executable(ConfigTest tests/ConfigTest.cpp)
    target_link_libraries(ConfigTest PRIVATE Qt6::Gui)
    add_test(NAME ConfigTest COMMAND ConfigTest)
    add_executable(SchedulerTest tests/SchedulerTest.cpp)
    target_include_directories(SchedulerTest PRIVATE src "${OBS_SDK_DIR}/include")
    target_link_libraries(SchedulerTest PRIVATE Qt6::Widgets)
    add_test(NAME SchedulerTest COMMAND SchedulerTest)
endif()

install(TARGETS SceneWall
    RUNTIME DESTINATION "plugins/SceneWall"
    LIBRARY DESTINATION "plugins/SceneWall")
install(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/data/locale"
    DESTINATION "plugins/SceneWall/data")

