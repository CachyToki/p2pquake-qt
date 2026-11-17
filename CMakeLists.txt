cmake_minimum_required(VERSION 3.21)

project(p2pquake-qt VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release)
endif()

find_package(Qt6 6.5 REQUIRED COMPONENTS Widgets Network WebSockets)
qt_standard_project_setup()

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(-Wall -Wextra -Wpedantic)
endif()

# アプリ本体はライブラリにまとめ、実行ファイルとテストの両方から使う。
qt_add_library(p2pquake STATIC
    src/core/Types.h
    src/core/Parser.h           src/core/Parser.cpp
    src/core/Labels.h           src/core/Labels.cpp
    src/core/AreaTable.h        src/core/AreaTable.cpp
    src/core/Client.h           src/core/Client.cpp
    src/ui/Presenter.h          src/ui/Presenter.cpp
    src/ui/EventListModel.h     src/ui/EventListModel.cpp
    src/ui/EventDelegate.h      src/ui/EventDelegate.cpp
    src/ui/MainWindow.h         src/ui/MainWindow.cpp
)
target_include_directories(p2pquake PUBLIC src)
target_compile_definitions(p2pquake PUBLIC
    P2PQUAKE_QT_VERSION="${PROJECT_VERSION}"
    QT_NO_CAST_FROM_ASCII
    QT_NO_CAST_TO_ASCII
)
target_link_libraries(p2pquake PUBLIC Qt6::Widgets Qt6::Network Qt6::WebSockets)

qt_add_resources(p2pquake "assets"
    PREFIX "/"
    BASE resources
    FILES
        resources/epsp-area.csv
        resources/p2pquake-qt.svg
)

qt_add_executable(p2pquake-qt src/main.cpp)
target_link_libraries(p2pquake-qt PRIVATE p2pquake)

include(GNUInstallDirs)
install(TARGETS p2pquake-qt RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install(FILES packaging/p2pquake-qt.desktop
    DESTINATION ${CMAKE_INSTALL_DATADIR}/applications)
install(FILES LICENSE THIRD_PARTY_NOTICES.md
    DESTINATION ${CMAKE_INSTALL_DOCDIR})
install(FILES resources/p2pquake-qt.svg
    DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/scalable/apps)

option(P2PQUAKE_BUILD_TESTS "テストをビルドする" ON)
if(P2PQUAKE_BUILD_TESTS)
    find_package(Qt6 REQUIRED COMPONENTS Test)
    enable_testing()
    add_subdirectory(tests)
endif()
