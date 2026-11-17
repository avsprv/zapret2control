# Backend Architecture: Current-State Audit and Target Boundaries

**Status:** Design proposal based on the current `main` source tree.  
**Scope:** Application/backend separation for Linux continuity and planned Windows 11 x64 support.  
**Implementation status:** No backend refactor is included in this document-only change.

## 1. Current implementation observed on main

The current backend abstraction is declared in `src/backend/zapretbackend.h`.

It exposes:

- `refresh()`, `verifyState()`, `start()`, and `stop()`;
- request-correlated overloads for lifecycle operations;
- `setBootEnabled(bool)`;
- service/unit-oriented completion signals;
- a state enum with Unknown, Active, Inactive and Failed.

The abstraction is not platform-neutral yet. In particular:

1. `setBootEnabled(bool)` models a boot-enabled system service, not a generic application capability.
2. `unitActionFinished` and `unitActionFinishedForRequest` expose systemd unit terminology.
3. `ApplicationController` directly constructs `SystemdBackend` in `src/qml/applicationcontroller.cpp`.
4. The controller exposes `bootEnabled` and `setBootEnabled()` as its public QML-facing contract.
5. `SystemdBackend` owns systemctl process execution, systemd unit parsing, and privileged helper invocation.

This is an accurate representation of the current Linux implementation. It must not be treated as a reason to weaken or replace the existing Linux behavior.

## 2. Current CMake/platform coupling

The current root `CMakeLists.txt` unconditionally:

- discovers KDE ECM and Kirigami;
- discovers Qt DBus;
- compiles `systemdbackend.cpp`;
- compiles the Linux helper-path implementation;
- configures a Polkit policy;
- configures systemd service units;
- installs the privileged helper, Polkit policy and systemd units;
- links the production application to Qt DBus.

Some of these dependencies may be shared by other existing features, so they must not be removed merely because Windows does not use them. First classify each dependency and source file by actual consumer. Make only the platform-specific pieces conditional after that inventory is complete.

## 3. Required architectural boundary

Keep the shared controller responsible for application intent and UI state. It must not know which OS command, service manager, privilege broker, IPC transport or driver mechanism performs an operation.

The backend boundary should represent engine lifecycle and observable results:

- query current engine state;
- request engine start;
- request engine stop;
- validate/apply a selected configuration where supported;
- obtain bounded diagnostics;
- report supported capabilities;
- report authorization requirements, progress, cancellation, partial failure and final outcome.

The API should use engine-oriented terminology rather than systemd unit terminology.

A generic state model should not overclaim what a platform can know. Keep at least Unknown, Starting, Active, Stopping, Inactive and Failed where the implementation can distinguish them. If readiness cannot be established, report Unknown or a distinct NotReady state rather than treating process existence as Active.

All asynchronous operations need correlation IDs or an equivalent typed operation handle. A stale response must not overwrite newer application state. Preserve the current Linux backend's request-correlation and stale-result protections during migration.

## 4. Capabilities are not lifecycle operations

Do not put every platform-specific feature into the common lifecycle interface.

Examples:

- Enabling a system service at boot is a Linux service-manager capability.
- Starting the GUI at user login is an application autostart capability.
- Windows UAC authorization is an authorization capability/operation.
- WinDivert driver readiness is a Windows runtime prerequisite.
- Reading the systemd journal is a Linux diagnostics capability.

Expose capabilities explicitly, with a typed availability state and explanatory reason. Do not map unrelated mechanisms onto a shared boolean merely to simplify QML.

The existing `bootEnabled` property should remain available to the Linux UI until a deliberate UI/product decision changes it. For the cross-platform UI, model service boot enablement and application login autostart as distinct settings. A platform that does not support one must report it as unavailable, not silently return false.

## 5. Composition and platform selection

Remove direct construction of `SystemdBackend` from `ApplicationController` only when a safe composition path is ready.

Preferred direction:

- construct the platform backend in a composition/factory layer;
- inject the backend through the controller constructor or a narrowly scoped factory;
- select the implementation at compile time using the platform-specific target sources;
- keep OS selection out of QML;
- avoid runtime platform branching scattered through controllers.

The Linux application must continue to construct the existing SystemdBackend in its production composition path until a Windows implementation is complete. Tests should inject a fake backend without invoking systemctl or pkexec.

Do not create a Windows backend that merely returns Unsupported for every operation and then call Windows support implemented. A temporary stub may be used only to validate compilation and must be clearly marked unavailable.

## 6. Migration sequence

1. Add backend contract tests using a fake implementation.
2. Introduce typed, platform-neutral lifecycle results and capability reporting.
3. Adapt SystemdBackend to the new contract without changing its systemctl/helper behavior.
4. Inject the backend into ApplicationController; retain Linux production composition.
5. Migrate the UI-facing service/autostart properties without changing Linux user-visible behavior.
6. Add Linux regression tests for refresh, stale results, action serialization, timeouts, authorization cancellation and destruction with active processes.
7. Only after the Linux refactor passes full CI, DEB and RPM smoke tests, add Windows-specific source files and dependencies.
8. Implement Windows lifecycle and privilege handling as a separate backend; do not emulate systemd semantics.

Each step must be a separate reviewable change with CI results recorded against one exact commit SHA.

## 7. Explicit non-goals for the first refactor

- Do not rewrite SystemdBackend internals without a demonstrated need.
- Do not change the upstream Linux engine CLI or systemd unit.
- Do not change Polkit policy semantics.
- Do not move Windows APIs into shared QML or domain code.
- Do not implement Windows runtime/process supervision as part of the interface-only change.
- Do not rename unrelated classes or reorganize the whole source tree.
- Do not remove Qt DBus until every existing consumer has been identified.

## 8. Acceptance criteria

The backend-boundary refactor is acceptable only when:

- Linux production composition still selects SystemdBackend;
- all current Linux lifecycle behavior remains intact;
- no systemctl, pkexec, Polkit or systemd unit details leak into the common backend contract;
- controller tests can inject a fake backend;
- async results are correlated and stale results cannot overwrite newer state;
- capability absence is represented explicitly;
- QML does not select a platform backend;
- QML lint, unit tests, Linux systemd E2E, Debian 13 DEB and Fedora RPM checks pass on the same commit SHA;
- package install/remove smoke tests remain successful.

Windows is not considered implemented by passing these Linux-focused criteria. It requires its own backend and Windows 11 x64 runtime acceptance tests.
