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

add_executable(scronify
icons.qrc
src/screen_handler.cpp
src/action_widget.cpp
src/main.cpp)

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
