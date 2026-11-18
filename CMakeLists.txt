cmake_minimum_required(VERSION 3.21)
project(zapret2control VERSION 0.5.0 LANGUAGES CXX)

# Minimum supported Qt release. Keep the API compatible with the Qt 6.8 LTS baseline.
set(QZAPRET_QT_MIN 6.8 CACHE STRING "Minimum supported Qt version (LTS)")

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include(GNUInstallDirs)
include(CTest)

# KDE Frameworks / Kirigami dependencies for the production QML application.
find_package(ECM 6.0.0 REQUIRED NO_MODULE)
set(CMAKE_MODULE_PATH ${ECM_MODULE_PATH})
include(ECMFindQmlModule)
find_package(KF6 REQUIRED COMPONENTS Kirigami)
ecm_find_qmlmodule(org.kde.kirigami REQUIRED)

find_package(Qt6 ${QZAPRET_QT_MIN} REQUIRED COMPONENTS
    Network LinguistTools
    Core Gui Qml Quick QuickControls2 QuickDialogs2 DBus
)

qt_standard_project_setup(REQUIRES ${QZAPRET_QT_MIN})

set(QZAPRET_HELPER "${CMAKE_INSTALL_FULL_LIBEXECDIR}/zapret2control/zapret2control-helper")


qt_add_library(zapret2control_i18n STATIC src/i18n.cpp src/i18n.h)
target_include_directories(zapret2control_i18n PUBLIC src)
target_link_libraries(zapret2control_i18n PUBLIC Qt6::Core)
qt_add_translations(zapret2control_i18n
    TS_FILES i18n/zapret2control_en.ts
    SOURCES
        src/i18n.cpp
        src/qml/main.cpp
        src/qml/applicationcontroller.cpp
        src/qml/configcontroller.cpp
        src/qml/profilecontroller.cpp
        src/qml/settingsmanager.cpp
        src/qml/logscontroller.cpp
        src/qml/listscontroller.cpp
        src/qml/blockcheckjobcontroller.cpp
        src/qml/updatecontroller.cpp
        src/qml/autostartcontroller.cpp
        src/qml/traycontroller.cpp
        src/qml/dbusmenu.cpp
        src/backend/blockcheckrunner.cpp
        src/backend/systemdbackend.cpp
        src/backend/updatechecker.cpp
        src/backend/zapretupdater.cpp
        src/profileanalyzer.cpp
        src/qml/Main.qml
        src/qml/pages/SettingsPage.qml
        src/qml/pages/ConfigPage.qml
        src/qml/pages/ProfilesPage.qml
        src/qml/pages/AboutPage.qml
        src/qml/pages/LogsPage.qml
        src/qml/pages/ListsPage.qml
        src/qml/pages/BlockcheckPage.qml
        src/qml/pages/UpdatePage.qml
    INCLUDE_DIRECTORIES src
    LUPDATE_OPTIONS -no-obsolete
    LUPDATE_TARGET zapret2control_i18n_lupdate
)

# The Kirigami application is the sole production executable.
qt_add_executable(zapret2control
    src/qml/main.cpp
    src/qml/applicationcontroller.cpp
    src/qml/applicationcontroller.h
    src/qml/settingsmanager.cpp
    src/qml/settingsmanager.h
    src/qml/autostartcontroller.cpp
    src/qml/autostartcontroller.h
    src/qml/traycontroller.cpp
    src/qml/traycontroller.h
    src/qml/dbusmenu.cpp
    src/qml/dbusmenu.h
    src/qml/configcontroller.cpp
    src/qml/configcontroller.h
    src/qml/profilecontroller.cpp
    src/qml/profilecontroller.h
    src/qml/logscontroller.cpp
    src/qml/logscontroller.h
    src/qml/listscontroller.cpp
    src/qml/listscontroller.h
    src/qml/blockcheckjobcontroller.cpp
    src/qml/blockcheckjobcontroller.h
    src/qml/updatecontroller.cpp
    src/qml/updatecontroller.h
    src/backend/updatechecker.cpp
    src/backend/updatechecker.h
    src/backend/zapretupdater.cpp
    src/backend/zapretupdater.h
    src/backend/zapretpaths.h
    src/backend/blockcheckrunner.cpp
    src/backend/blockcheckrunner.h
    src/profileanalyzer.cpp
    src/profileanalyzer.h
    src/backend/systemdbackend.cpp
    src/backend/systemdbackend.h
    src/backend/zapretbackend.h
    src/backend/backendtypes.h
    src/backend/helperpath.h
)
qt_add_qml_module(zapret2control
    URI org.avsprv.zapret2control
    VERSION 1.0
    QML_FILES
        src/qml/Main.qml
        src/qml/pages/SettingsPage.qml
        src/qml/pages/ConfigPage.qml
        src/qml/pages/ProfilesPage.qml
        src/qml/pages/AboutPage.qml
        src/qml/pages/LogsPage.qml
        src/qml/pages/ListsPage.qml
        src/qml/pages/BlockcheckPage.qml
        src/qml/pages/UpdatePage.qml
)
target_include_directories(zapret2control PRIVATE src src/qml)
target_link_libraries(zapret2control PRIVATE
    Qt6::Core Qt6::Gui Qt6::Network Qt6::Qml Qt6::Quick Qt6::QuickControls2 Qt6::QuickDialogs2 Qt6::DBus
    zapret2control_i18n
)
target_compile_definitions(zapret2control PRIVATE
    QZAPRET_VERSION="${PROJECT_VERSION}"
    QZAPRET_HELPER_PATH="${QZAPRET_HELPER}")
target_compile_options(zapret2control PRIVATE -Wall -Wextra)
target_link_options(zapret2control PRIVATE -Wl,-z,now)

# The production executable must never acquire a Qt Widgets dependency.
get_target_property(QZAPRET_PRODUCTION_LINK_LIBRARIES zapret2control LINK_LIBRARIES)
if("Qt6::Widgets" IN_LIST QZAPRET_PRODUCTION_LINK_LIBRARIES)
    message(FATAL_ERROR "The production QML target must not link Qt6::Widgets")
endif()

configure_file(data/org.zapret2control.policy.in ${CMAKE_BINARY_DIR}/org.zapret2control.policy @ONLY)

configure_file(data/systemd/zapret2control-blockcheck.service.in
    ${CMAKE_BINARY_DIR}/zapret2control-blockcheck.service @ONLY)
configure_file(data/systemd/zapret2control-blockcheck-recovery.service.in
    ${CMAKE_BINARY_DIR}/zapret2control-blockcheck-recovery.service @ONLY)

install(TARGETS zapret2control RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install(PROGRAMS packaging/helper/zapret2control-helper
        DESTINATION ${CMAKE_INSTALL_LIBEXECDIR}/zapret2control)
install(FILES
        packaging/helper/blockcheck_manager.py
        packaging/helper/blockcheck_state.py
        packaging/helper/blockcheck_staging.py
        packaging/helper/blockcheck_worker.py
        packaging/helper/blockcheck_recovery.py
        packaging/helper/blockcheck_submit.py
        packaging/helper/blockcheck_status.py
        packaging/helper/blockcheck_probe.py
        packaging/helper/blockcheck_admission.py
        packaging/helper/blockcheck_cancel.py
        packaging/helper/updater_transaction.py
        packaging/helper/updater_staging.py
        packaging/helper/updater_activation.py
        packaging/helper/updater_service.py
        packaging/helper/updater_cli.py
        DESTINATION ${CMAKE_INSTALL_DATADIR}/zapret2control)
install(FILES ${CMAKE_BINARY_DIR}/org.zapret2control.policy
        DESTINATION ${CMAKE_INSTALL_DATADIR}/polkit-1/actions)
install(FILES
        ${CMAKE_BINARY_DIR}/zapret2control-blockcheck.service
        ${CMAKE_BINARY_DIR}/zapret2control-blockcheck-recovery.service
        DESTINATION lib/systemd/system)
install(FILES data/zapret2control.desktop DESTINATION ${CMAKE_INSTALL_DATADIR}/applications)
install(FILES data/zapret2control.1 DESTINATION ${CMAKE_INSTALL_MANDIR}/man1)
install(FILES data/org.avsprv.zapret2control.metainfo.xml
        DESTINATION ${CMAKE_INSTALL_DATADIR}/metainfo)
install(FILES data/icons/hicolor/scalable/apps/zapret2control.svg
        DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/scalable/apps)
install(FILES data/icons/hicolor/symbolic/apps/zapret2control-tray-symbolic.svg
        DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/symbolic/apps)

include(cmake/Testing.cmake)
