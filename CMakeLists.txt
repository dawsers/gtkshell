cmake_minimum_required(VERSION 3.12)
project("gtkshell"
    LANGUAGES CXX
    DESCRIPTION "Wayland bar")

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

find_package(PkgConfig REQUIRED)
pkg_check_modules(gtk4-layer-shell REQUIRED IMPORTED_TARGET gtk4-layer-shell-0)
pkg_check_modules(GTK4 REQUIRED IMPORTED_TARGET gtk4)
pkg_check_modules(gtkmm REQUIRED IMPORTED_TARGET gtkmm-4.0)
pkg_check_modules(LIBNM REQUIRED IMPORTED_TARGET libnm)

find_package(nlohmann_json REQUIRED)
pkg_check_modules(wireplumber REQUIRED IMPORTED_TARGET wireplumber-0.5)

find_package(CURL REQUIRED)

# Compile flags
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED true)

add_executable(gtkshell
    shellwindow.cpp
    hyprland.cpp
    mpris.cpp
    bind.cpp
    utils.cpp
    clock.cpp
    widgets.cpp
    graph.cpp
    wireplumber.cpp
    notifications.cpp
    systemtray.cpp
    weather.cpp
    scroll.cpp
    main.cpp)

target_link_libraries(gtkshell PRIVATE
    PkgConfig::gtk4-layer-shell
    PkgConfig::GTK4
    PkgConfig::gtkmm
    PkgConfig::LIBNM
    nlohmann_json::nlohmann_json
    PkgConfig::wireplumber
    curl
)
