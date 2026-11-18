# Windows Build Readiness Audit

**Audit date:** 2026-11-18  
**Audited source revision:** `07cefbfb0fcc5b2f043218cf71af1f2b7d3e808e`  
**Target:** Windows 11 x64  
**Status:** Windows build graph is not ready; Windows CI implementation is blocked on platform separation.

This document records the repository-side build blockers found before introducing a Windows workflow. It does not claim Windows support, a working Windows build, or a completed Windows packaging pipeline.

## 1. Current CI topology

The repository currently contains two workflow files:

| Workflow | Current scope | Windows coverage |
|---|---|---|
| `.github/workflows/ci.yml` | Fedora-based Linux configure/build, translation check, systemd unit verification, QML lint, CTest; separate systemd E2E job | None |
| `.github/workflows/packages.yml` | Debian 13 DEB and Fedora RPM builds, package inspection, clean install/remove smoke tests, artifact upload | None |

There is no Windows workflow, Windows runner, Craft environment, NSIS packaging job, or Windows deployment-manifest validation.

The required integrated CI set is Main CI, Debian 13 DEB, Fedora RPM, and Windows CI. All four must verify the same exact commit SHA. A future Windows workflow must not replace or weaken Linux checks.

## 2. Build-graph blockers

The current root `CMakeLists.txt` is a Linux build graph, not a cross-platform graph.

### 2.1 Unconditional Linux/KDE dependencies

The root configuration unconditionally:

- requires ECM and KF6 Kirigami;
- discovers Qt DBus as a required Qt component;
- compiles `src/backend/systemdbackend.cpp`;
- compiles Linux desktop integration and helper-dependent controllers into the production executable;
- configures and installs Polkit policy and systemd unit files;
- installs the Linux privileged helper and its Python support modules;
- requires `dbus-run-session` when `BUILD_TESTING` is enabled.

A Windows configure job would therefore fail at dependency discovery or later build/install stages before it could verify the shared application.

### 2.2 Linux-specific runtime integration

The following coupling is confirmed in the inspected source:

| Component | Current Linux dependency | Required direction |
|---|---|---|
| `SystemdBackend` | systemctl, systemd unit lifecycle, privileged helper | Keep as Linux adapter; add a separate Windows adapter |
| `helperpath.h` | pkexec launch and Linux helper path | Keep Linux helper launcher out of shared code |
| `ApplicationController` | Default construction of SystemdBackend | Compose a platform backend outside controller logic |
| `main.cpp` | Constructs Linux-oriented controllers and tray integration | Keep shared composition separate from platform adapters |
| `BlockcheckRunner` | Launches privileged helper through Helper API | Introduce a platform execution boundary before Windows support |
| Config, lists and logs controllers | Helper-mediated operations; Linux-specific helper commands | Separate domain/UI behavior from platform storage and diagnostics |
| Tray and autostart integration | Linux desktop/session integration | Define platform capability adapters; do not assume DBus semantics on Windows |

This is an initial source audit, not a claim that every transitive dependency has been exhaustively classified. The remaining controllers, helper modules, tests, QML imports and runtime dependencies require a file-by-file pass before Windows source selection is finalized.

## 3. Existing test assumptions

The current test graph is also Linux-oriented:

- the main CI installs systemd and DBus packages;
- systemd unit validation runs after installing the application into the runner;
- the E2E job requires systemd as PID 1;
- the CMake test configuration requires `dbus-run-session`;
- helper-oriented tests use Linux shell scripts and Linux helper paths.

These checks must remain mandatory on Linux. Windows CI must select Windows-compatible tests explicitly without marking Linux-only tests as passed or silently skipping required tests.

Shared tests should remain platform-neutral. Linux adapter and systemd integration tests should be conditionally registered only for Linux. Windows adapter and supervisor tests must be added with their own explicit prerequisites and result reporting.

## 4. Required CMake separation

Before enabling a Windows runner, refactor the build graph into explicit source/dependency groups:

1. **Shared application:** Qt 6, shared QML/Kirigami module, platform-neutral controllers and domain logic.
2. **Linux adapter:** SystemdBackend, Polkit/helper integration, DBus-specific components where required, Linux desktop integration.
3. **Windows adapter:** Windows backend, authorization/supervisor IPC and Windows-specific runtime integration.
4. **Shared tests:** platform-neutral contract, controller and domain tests.
5. **Linux tests:** systemd, DBus, helper, shell and package integration tests.
6. **Windows tests:** backend contract, process supervision, IPC/security and Windows runtime tests.

Use CMake platform checks for target sources, dependencies, compile definitions, test registration and installation rules. Do not create a second Qt Widgets application or fork the shared QML UI. Do not introduce an all-Unsupported Windows backend as a substitute for implementation.

The Linux production composition must continue to select SystemdBackend until the Windows implementation is complete. Linux behavior and package layouts must remain unchanged through this refactor.

## 5. Windows CI introduction sequence

A Windows workflow should be introduced in stages, with each stage backed by a real target:

### Stage A — Windows configure and dependency resolution

- Windows 11 x64 runner.
- Approved MSVC/Qt 6/KDE Craft toolchain.
- Fresh build directory.
- Configure the actual shared application target.
- Confirm no systemd, Polkit, pkexec or Linux-only DBus dependency is discovered.

### Stage B — Windows build and static validation

- Build the shared QML/Kirigami application and Windows adapter.
- Run the complete applicable Qt 6 QML lint target.
- Validate QML import/type metadata and runtime dependency collection.
- Run shared and Windows-specific automated tests.
- Report exact passed, failed, skipped and not-run counts where available.

### Stage C — deployment and installer validation

- Produce a deployment staging directory using KDE Craft.
- Validate QML modules, Qt plugins, QQC2 desktop style, Kirigami, KDE Frameworks, icon/theme resources and third-party runtime dependencies.
- Generate a staging manifest and file hashes.
- Generate the final NSIS EXE from the validated staging directory.
- Inspect installer contents and hashes; upload artifacts with source SHA provenance.

A successful Windows compilation does not establish WinDivert operation, UAC correctness, supervisor security, install/upgrade/uninstall correctness, or Windows feature parity. Those require clean Windows 11 VM acceptance tests.

## 6. Packaging and security constraints

The approved direction is KDE Craft for runtime staging and NSIS for the final per-machine installer. The GUI must run unelevated; privileged operations must use a narrowly scoped Windows-specific boundary. Do not introduce a permanent Windows service solely to mirror systemd.

Do not bundle an upstream Windows runtime until the exact upstream release and asset are pinned, hashes are independently calculated, PE imports and Authenticode signatures are inspected, license obligations are reviewed, and the runtime is tested on a clean Windows 11 x64 VM. WinDivert artifacts remain subject to the security and provenance hold recorded in the Windows packaging architecture specification.

## 7. Exit criteria for enabling Windows CI

Windows CI may become a required green gate only after:

- Linux-only sources, dependencies, resources, install rules and tests are isolated;
- the shared Qt 6/Kirigami target configures and builds on Windows;
- a real Windows backend exists for the declared test scope;
- QML lint covers the complete shared QML module;
- shared and Windows-specific tests run and report results;
- Linux CI, DEB, RPM and systemd E2E continue to pass;
- all four primary workflows report the same exact source commit SHA.

Until these conditions are met, Windows CI status is **Blocked / Not implemented**, not Passed.

## 8. Source evidence

The audit inspected the repository at the source revision listed above, including:

- `.github/workflows/ci.yml`;
- `.github/workflows/packages.yml`;
- `CMakeLists.txt`;
- `src/qml/main.cpp`;
- `src/qml/applicationcontroller.cpp` and `.h`;
- `src/backend/systemdbackend.cpp` and `.h`;
- `src/backend/helperpath.h`;
- `src/backend/blockcheckrunner.cpp`;
- helper-dependent configuration and list controller code;
- `docs/PLATFORM_SUPPORT.md`;
- `docs/BACKEND_ARCHITECTURE.md`.

The detailed Windows supervisor, IPC, runtime, licensing, installer and release design remains specified in the project kit's `reference/WINDOWS_PACKAGING_ARCHITECTURE.md`. It must be reviewed and incorporated into the repository's authoritative `docs/` set before implementation of the privileged Windows supervisor begins.
