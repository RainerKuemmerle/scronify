cmake_minimum_required(VERSION 3.16)

set(SCRONIFY_VERSION "0.1.0")

project(scronify VERSION ${SCRONIFY_VERSION} DESCRIPTION "Notify in case the screen count changes")

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(SCRONIFY_QTVERSION "6" CACHE STRING "Which Qt version to use. 6|5")
find_package(Qt${SCRONIFY_QTVERSION} COMPONENTS Core Widgets Concurrent REQUIRED)

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/config.h.in
 ${CMAKE_CURRENT_SOURCE_DIR}/include/scronify/config.h)

set(SCRONIFY_SRCS
    icons.qrc
    src/split_command.cpp
    src/screen_handler.cpp
    src/action_widget.cpp
    src/display_event.cpp
    src/main.cpp)

add_executable(scronify ${SCRONIFY_SRCS})

option(ENABLE_WAYLAND "Build with Wayland support" ON)
if(ENABLE_WAYLAND)
    find_package(PkgConfig)
    if(PkgConfig_FOUND)
        pkg_check_modules(WAYLAND QUIET wayland-client)
    else()
        message(STATUS "PkgConfig not found; cannot check for Wayland client library")
    endif()

    if(WAYLAND_LIBRARIES)
        target_sources(scronify PRIVATE src/wayland_event.cpp)
        target_include_directories(scronify PRIVATE ${WAYLAND_INCLUDE_DIRS})
        target_link_libraries(scronify PRIVATE ${WAYLAND_LIBRARIES})
        target_compile_definitions(scronify PRIVATE HAVE_WAYLAND)
    else()
        message(STATUS "Wayland client not found; building without Wayland support")
    endif()
endif()

option(ENABLE_X11 "Build with X11 support" ON)
if(ENABLE_X11)
    find_package(PkgConfig)
    if(PkgConfig_FOUND)
        pkg_check_modules(X11 QUIET x11)
        pkg_check_modules(XRANDR QUIET xrandr)
    else()
        message(STATUS "PkgConfig not found; cannot check for X11/xrandr")
    endif()

    if(X11_LIBRARIES AND XRANDR_LIBRARIES)
        target_sources(scronify PRIVATE src/x11_event.cpp)
        target_include_directories(scronify PRIVATE ${X11_INCLUDE_DIRS} ${XRANDR_INCLUDE_DIRS})
        target_link_libraries(scronify PRIVATE ${X11_LIBRARIES} ${XRANDR_LIBRARIES})
        target_compile_definitions(scronify PRIVATE HAVE_X11 HAVE_XRANDR)
    else()
        message(STATUS "X11/Xrandr not found; building without X11 support")
    endif()
endif()

target_compile_options(scronify PRIVATE -Wall -Wextra -pedantic)
target_include_directories(scronify PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(scronify PRIVATE Qt${SCRONIFY_QTVERSION}::Core
    Qt${SCRONIFY_QTVERSION}::Widgets)

include(GNUInstallDirs)
install(TARGETS scronify
    EXPORT scronify
    INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
)
