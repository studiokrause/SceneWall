cmake_minimum_required(VERSION 3.16)
project(SceneWall LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)

# Configure OBS SDK locations
set(OBS_SDK_DIR "D:/obs-hider/.deps/obs-sdk-installed")

include_directories("${OBS_SDK_DIR}/include")
link_directories("${OBS_SDK_DIR}/lib")

# Add Qt6 location
list(APPEND CMAKE_PREFIX_PATH "D:/obs-hider/.deps/obs-studio-32.2.2/.deps/obs-deps-qt6-2026-07-15-x64/lib/cmake/Qt6")

find_package(Qt6 REQUIRED COMPONENTS Widgets)

add_library(SceneWall MODULE
    src/plugin-main.cpp
    src/SceneWall.cpp
    src/SettingsDialog.cpp
    src/AboutDialog.h
)

target_link_libraries(SceneWall PRIVATE obs.lib obs-frontend-api.lib Qt6::Widgets)

set_target_properties(SceneWall PROPERTIES PREFIX "")

install(TARGETS SceneWall LIBRARY DESTINATION "obs-plugins/64bit")

