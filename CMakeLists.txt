cmake_minimum_required(VERSION 3.16)
project(SceneWall VERSION 0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)

# Path to an installed OBS SDK (headers + import libraries).
set(OBS_SDK_DIR "" CACHE PATH "Path to an installed OBS SDK (contains include/ and lib/)")
if(NOT OBS_SDK_DIR)
    message(FATAL_ERROR "OBS_SDK_DIR is not set. Pass -DOBS_SDK_DIR=<path> when configuring.")
endif()

# Optional path to a Qt6 CMake package directory (lib/cmake/Qt6).
set(OBS_QT_DIR "" CACHE PATH "Path to a Qt6 build (lib/cmake/Qt6)")

include_directories("${OBS_SDK_DIR}/include")
link_directories("${OBS_SDK_DIR}/lib")

if(OBS_QT_DIR)
    list(APPEND CMAKE_PREFIX_PATH "${OBS_QT_DIR}")
endif()

find_package(Qt6 REQUIRED COMPONENTS Widgets)

add_library(SceneWall MODULE
    src/plugin-main.cpp
    src/SceneWall.cpp
    src/SceneTileWidget.cpp
    src/SceneHeaderWidget.cpp
    src/ColoredTabBar.cpp
    src/ToggleSwitch.cpp
    src/ThumbnailManager.cpp
    src/FlowLayout.cpp
    src/SceneWallConfig.cpp
    src/SettingsDialog.cpp
    src/AboutDialog.h
)

target_link_libraries(SceneWall PRIVATE obs.lib obs-frontend-api.lib Qt6::Widgets)

target_compile_definitions(SceneWall PRIVATE SCENEWALL_VERSION="${PROJECT_VERSION}")

set_target_properties(SceneWall PROPERTIES PREFIX "")

install(TARGETS SceneWall LIBRARY DESTINATION "obs-plugins/64bit")
