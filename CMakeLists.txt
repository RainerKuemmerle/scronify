cmake_minimum_required(VERSION 3.16)

set(SCRONIFY_VERSION "0.1.0")

project(scronify VERSION ${SCRONIFY_VERSION} DESCRIPTION "Notify in case the screen count changes")

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(SCRONIFY_QTVERSION "6" CACHE STRING "Which Qt version to use. 6|5")
find_package(Qt${SCRONIFY_QTVERSION} COMPONENTS Core Widgets Concurrent DBus REQUIRED)

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/config.h.in
 ${CMAKE_CURRENT_SOURCE_DIR}/include/scronify/config.h)

set(SCRONIFY_SRCS
    icons.qrc
    src/split_command.cpp
    src/screen_handler.cpp
    src/action_widget.cpp
    src/display_event.cpp
    src/lid_event.cpp
    src/main.cpp)

add_executable(scronify ${SCRONIFY_SRCS})
find_package(PkgConfig QUIET)

option(ENABLE_WAYLAND "Build with Wayland support" ON)
if(ENABLE_WAYLAND)
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
        # Add a small CLI utility to enumerate Wayland outputs and compute layout plans.
        add_executable(scronify-wayland-placer src/wayland_placer.cpp src/wayland_display.cpp)
        target_include_directories(scronify-wayland-placer PRIVATE ${WAYLAND_INCLUDE_DIRS} ${CMAKE_CURRENT_SOURCE_DIR}/include)
        target_link_libraries(scronify-wayland-placer PRIVATE ${WAYLAND_LIBRARIES})
        target_link_libraries(scronify-wayland-placer PRIVATE Qt${SCRONIFY_QTVERSION}::Core)
        # Optionally generate and include wlr-output-management protocol bindings
        find_program(WAYLAND_SCANNER wayland-scanner)
        if(WAYLAND_SCANNER)
            # Look for the exact protocol XML file in common locations or under a provided third_party tree.
            set(WLR_XML_NAME "wlr-output-management-unstable-v1.xml")
            set(WLR_XML_CANDIDATES
                /usr/share/wayland-protocols/unstable/wlr-output-management-unstable-v1/${WLR_XML_NAME}
                /usr/share/wayland-protocols/unstable/${WLR_XML_NAME}
                /usr/share/wayland-protocols/${WLR_XML_NAME}
                ${CMAKE_SOURCE_DIR}/third_party/wayland-protocols/${WLR_XML_NAME}
            )

            set(WLR_OUTPUT_MGMT_XML "")
            foreach(candidate IN LISTS WLR_XML_CANDIDATES)
                if(EXISTS ${candidate} AND NOT IS_DIRECTORY ${candidate})
                    set(WLR_OUTPUT_MGMT_XML ${candidate})
                    message(STATUS "Found wlr output-management protocol XML at ${candidate}")
                    break()
                endif()
            endforeach()

            if(WLR_OUTPUT_MGMT_XML)
                set(GEN_DIR ${CMAKE_BINARY_DIR}/generated)
                file(MAKE_DIRECTORY ${GEN_DIR})
                set(WLR_OUT_HDR ${GEN_DIR}/wlr-output-management-unstable-v1-client-protocol.h)
                set(WLR_OUT_C   ${GEN_DIR}/wlr-output-management-unstable-v1-client-protocol.c)
                add_custom_command(
                    OUTPUT ${WLR_OUT_HDR} ${WLR_OUT_C}
                    COMMAND ${WAYLAND_SCANNER} client-header ${WLR_OUTPUT_MGMT_XML} ${WLR_OUT_HDR}
                    COMMAND ${WAYLAND_SCANNER} code ${WLR_OUTPUT_MGMT_XML} ${WLR_OUT_C}
                    DEPENDS ${WLR_OUTPUT_MGMT_XML}
                    COMMENT "Generating wlr output-management client header and source"
                    VERBATIM
                )
                add_custom_target(generate_wlr_output_mgmt_hdr_dep DEPENDS ${WLR_OUT_HDR} ${WLR_OUT_C})
                add_dependencies(scronify-wayland-placer generate_wlr_output_mgmt_hdr_dep)
                target_include_directories(scronify-wayland-placer PRIVATE ${GEN_DIR})
                target_sources(scronify-wayland-placer PRIVATE ${WLR_OUT_C})
                target_compile_definitions(scronify-wayland-placer PRIVATE HAVE_OUTPUT_MGMT)
            else()
                message(STATUS "wlr output-management XML not found; --apply will be disabled")
            endif()
        else()
            message(STATUS "wayland-scanner not found; cannot check/generate wlr output-management bindings")
        endif()
    else()
        message(STATUS "Wayland client not found; building without Wayland support")
    endif()
endif()

option(ENABLE_X11 "Build with X11 support" ON)
if(ENABLE_X11)
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
    Qt${SCRONIFY_QTVERSION}::Widgets
    Qt${SCRONIFY_QTVERSION}::DBus)

include(GNUInstallDirs)
set(INSTALL_TARGETS scronify)
if(TARGET scronify-wayland-placer)
    list(APPEND INSTALL_TARGETS scronify-wayland-placer)
endif()
install(TARGETS ${INSTALL_TARGETS}
    EXPORT scronify
    INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
)
