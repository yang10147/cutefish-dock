cmake_minimum_required(VERSION 3.16)
project(cutefish-dock VERSION 0.8 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

# ---------- Qt6 ----------
find_package(Qt6 REQUIRED COMPONENTS
    Core
    Widgets
    DBus
    Qml
    Quick
    QuickControls2
    WaylandClient
)

# ---------- KDE Frameworks 6 ----------
find_package(ECM 6.0 REQUIRED NO_MODULE)
set(CMAKE_MODULE_PATH ${ECM_MODULE_PATH})

include(KDEInstallDirs)

find_package(KF6 REQUIRED COMPONENTS
    WindowSystem
    DBusAddons
    CoreAddons
)

# ---------- LayerShellQt ----------
find_package(LayerShellQt REQUIRED)

# ---------- 源文件 ----------
set(SOURCES
    src/main.cpp
    src/mainwindow.cpp
    src/dockadaptor.cpp
    src/xwindowinterface.cpp
    src/docksettings.cpp
    src/applicationmodel.cpp
    src/systemappmonitor.cpp
    src/systemappitem.cpp
    src/processprovider.cpp
    src/trashmanager.cpp
    src/activity.cpp
    src/fakewindow.cpp
    src/utils.cpp
    src/iconthemeimageprovider.cpp
)

qt6_add_resources(QML_RESOURCES qml.qrc)

add_executable(cutefish-dock
    ${SOURCES}
    ${QML_RESOURCES}
)

target_include_directories(cutefish-dock PRIVATE src)

target_link_libraries(cutefish-dock PRIVATE
    Qt6::Core
    Qt6::Widgets
    Qt6::DBus
    Qt6::Qml
    Qt6::Quick
    Qt6::QuickControls2
    Qt6::WaylandClient
    KF6::WindowSystem
    KF6::DBusAddons
    KF6::CoreAddons
    LayerShellQt::Interface
)

install(TARGETS cutefish-dock DESTINATION ${KDE_INSTALL_BINDIR})
