# Linux-specific application integration.
#
# Keep systemd, Polkit, D-Bus and privileged helper details out of the
# top-level application target configuration. This module is included only
# after the production executable has been declared.

find_package(Qt6 ${QZAPRET_QT_MIN} REQUIRED COMPONENTS DBus)

set(QZAPRET_HELPER "${CMAKE_INSTALL_FULL_LIBEXECDIR}/zapret2control/zapret2control-helper")

target_sources(zapret2control PRIVATE
    src/platform/platformintegration_linux.cpp
    src/platform/linuxautostartintegration.cpp
    src/platform/linuxautostartintegration.h
    src/platform/platformintegration.h
    src/platform/autostartintegration.h
    src/platform/trayintegration.h
    src/qml/traycontroller.cpp
    src/qml/traycontroller.h
    src/qml/dbusmenu.cpp
    src/qml/dbusmenu.h
    src/backend/systemdbackend.cpp
    src/backend/systemdbackend.h
)

target_link_libraries(zapret2control PRIVATE Qt6::DBus)

target_compile_definitions(zapret2control PRIVATE
    QZAPRET_HELPER_PATH="${QZAPRET_HELPER}"
)

configure_file(data/org.zapret2control.policy.in
    ${CMAKE_BINARY_DIR}/org.zapret2control.policy @ONLY)
configure_file(data/systemd/zapret2control-blockcheck.service.in
    ${CMAKE_BINARY_DIR}/zapret2control-blockcheck.service @ONLY)
configure_file(data/systemd/zapret2control-blockcheck-recovery.service.in
    ${CMAKE_BINARY_DIR}/zapret2control-blockcheck-recovery.service @ONLY)

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
