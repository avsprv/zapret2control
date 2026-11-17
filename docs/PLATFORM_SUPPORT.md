# Platform Support Strategy

**Status:** Architecture and roadmap baseline; Windows implementation has not started.  
**Current release platform:** Linux.  
**Windows target:** Windows 11 x64 only.  
**Shared UI stack:** Qt 6 and KDE Kirigami 6.

This document distinguishes the platform currently implemented in the repository from the intended cross-platform product. It is not a claim that Windows is already supported.

## 1. Product direction

zapret2control is a cross-platform desktop application with one shared Qt Quick/Kirigami user interface and platform-specific system integrations.

Linux support must remain a first-class product requirement. Adding Windows must not replace, regress, or reduce Linux functionality, packaging, tests, or release quality.

The target is one product with two platform backends:

- **Linux:** the existing systemd/Polkit integration and Linux packaging.
- **Windows:** a dedicated Windows backend, an unelevated GUI, a narrowly scoped elevated supervisor for the upstream Windows engine, and an NSIS installer.

Do not describe Windows as supported, stable, or release-ready until the Windows implementation and release gates in this document have passed.

## 2. Support status

| Area | Linux | Windows |
|---|---|---|
| Shared Qt 6/Kirigami UI | Implemented; validation continues | Target architecture; not implemented |
| Service/engine lifecycle | systemd backend | Windows supervisor design; not implemented |
| Privilege boundary | Polkit helper / pkexec | UAC bootstrap and restricted worker design; not implemented |
| Configuration and profiles | Existing Linux implementation | Shared domain model; Windows persistence and apply path not implemented |
| Lists | Existing Linux implementation | Windows path and update semantics not implemented |
| Logs | systemd journal | Windows diagnostic source not selected |
| Blockcheck2 | Existing Linux orchestration | Windows compatibility not established |
| Upstream runtime | Installed/updated from upstream Linux release | Existing upstream Windows x64 runtime is the preferred integration input; exact asset not pinned |
| Application packaging | Debian DEB and Fedora RPM | NSIS installer; not implemented |
| CI | Linux CI, DEB, RPM | Windows build and package validation; not implemented |
| Release status | Current supported platform, subject to release gates | Planned; unsupported until all gates pass |

The current README files and release metadata must continue to describe the actual released state. A roadmap target is not a support guarantee.

## 3. Shared application architecture

Keep the following platform-neutral:

- QML/Kirigami presentation, navigation, accessibility and localization;
- domain models for profiles, configuration, lists, diagnostics and update metadata;
- input validation that does not depend on operating-system behavior;
- update/release metadata parsing where protocol semantics are identical;
- typed controller contracts and user-visible operation/result states.

Keep operating-system integration behind platform-specific interfaces:

- engine/service lifecycle and status;
- privilege elevation and authorization;
- filesystem locations, permissions and atomic replacement;
- startup-at-login or startup-at-boot;
- logs and diagnostics;
- process supervision and cancellation;
- firewall/runtime prerequisites;
- installation, upgrade, repair and removal.

Do not put Linux commands, systemd unit names, Polkit actions, /opt paths, or POSIX shell assumptions into shared QML or platform-neutral domain classes. Do not put Windows APIs, registry paths, Named Pipe names, UAC logic, or WinDivert lifecycle code into shared QML.

### 3.1 Backend contracts

The existing ZapretBackend abstraction is a useful starting point, but its current API includes Linux-shaped concepts such as enabling a system service at boot. Before adding a second implementation, review the contract and split generic engine operations from platform capabilities.

The common contract should express user-visible intent, not prescribe a platform mechanism. At minimum, define typed operations and results for:

- query state and verify state;
- start and stop the engine;
- apply or validate a configuration/profile;
- retrieve bounded diagnostics;
- query supported capabilities;
- report progress, cancellation, authorization-required, partial failure and final result.

Represent platform-dependent features as explicit capabilities or separate interfaces. For example, service boot enablement, application login autostart, journal access and Windows driver readiness are not interchangeable concepts.

Do not add a Windows implementation by making SystemdBackend conditional throughout the UI. Use separate implementations selected at composition time. Keep platform selection out of QML wherever possible.

### 3.2 CMake and dependency boundaries

The current CMake project unconditionally discovers KDE ECM/Kirigami and Qt DBus and compiles Linux-specific backend sources and install resources. This is valid for the current Linux build but is not yet a Windows-ready build graph.

Before enabling Windows builds:

1. identify every Linux-only source, dependency, compile definition, resource and install rule;
2. separate common sources from Linux and Windows sources;
3. make platform dependencies and install rules conditional using CMake platform checks;
4. preserve the same Qt 6/Kirigami QML module and shared application target;
5. ensure the Windows target does not discover or link systemd, Polkit, pkexec or Linux-only DBus integration;
6. ensure Linux DEB/RPM targets remain unchanged in behavior and continue to pass their existing smoke tests;
7. validate QML module deployment and all Qt/KDE runtime imports on a clean Windows 11 x64 VM.

Do not introduce a parallel Qt Widgets application. Do not fork the QML interface by platform unless a documented native integration requires a small platform-specific component.

## 4. Linux requirements

Linux remains fully supported and must retain its existing product behavior, including the applicable features:

- systemd service state, start, stop and boot enablement;
- Polkit-protected helper operations;
- configuration and profile management with validation, backups and safe writes;
- hostlist management;
- journal monitoring;
- blockcheck2 orchestration and recovery behavior;
- upstream installation and update;
- tray, desktop integration, translations and settings.

Distribution baseline remains Debian 13 DEB and Fedora RPM unless an explicit project decision changes it. Package contents, dependencies, installation, upgrade, removal and runtime behavior must continue to be tested.

Every cross-platform change must include a Linux regression impact assessment. Linux CI, DEB and RPM checks remain mandatory and must refer to the same commit as the Windows CI checks for an integrated verification result.

## 5. Windows requirements

### 5.1 Supported target

The only planned Windows target is **Windows 11 x64**.

Windows 10 and earlier, ARM64, 32-bit Windows and Windows Server are outside the current support scope. The minimum Windows 11 build and supported feature releases must be established by actual clean-VM testing before release claims are made.

### 5.2 Runtime integration

Prefer the official prebuilt Windows x64 runtime published in an upstream zapret2 release over rebuilding unchanged upstream binaries.

The runtime archive previously inspected contains these candidate components:

- winws2.exe;
- cygwin1.dll;
- killall.exe;
- ip2net.exe;
- mdig.exe;
- WinDivert.dll;
- WinDivert64.sys.

This list is a candidate inventory, not an approved package manifest. Before redistribution, pin an exact upstream release and asset, preserve its original archive, calculate and verify SHA-256 for the archive and each included file, inspect PE imports and Authenticode signatures on Windows, review licenses/notices, and test the exact runtime on a clean Windows 11 x64 VM.

Do not rebuild these binaries merely to reproduce an existing upstream release. Keep a separate optional source-build track for cases where a binary must be patched, upstream does not publish a required artifact, or reproducibility research is explicitly required.

Do not claim that a published checksum authenticates the publisher unless it is cryptographically signed and the signing identity is independently trusted.

### 5.3 Process and privilege model

- The GUI always runs unelevated (asInvoker).
- Use machine-wide installation under %ProgramFiles%\zapret2control\.
- Use UAC only for the narrowly scoped operations that require elevation.
- The initial target is a per-interactive-session supervisor, not a permanent Windows service.
- Start the upstream engine using the verified upstream WinDivert lifecycle.
- Do not globally stop, delete or unregister WinDivert services during normal stop or uninstall; other applications may use the driver.
- Authenticate IPC peers and validate process identity, token, session and executable location.
- Do not equate process existence with engine readiness.
- Report forced termination accurately; do not claim graceful shutdown without a verified mechanism.
- UAC cancellation or failure must leave the system in a safe, clearly reported state.

The detailed Windows supervisor and installer design is currently maintained in the project documentation kit at reference/WINDOWS_PACKAGING_ARCHITECTURE.md. Before implementation begins, bring the reviewed design into this repository under docs/ and keep the repository copy authoritative. The design remains proposed until a Windows prototype passes its acceptance tests.

### 5.4 Installer and user data

- NSIS produces the final installer.
- KDE Craft stages the shared Qt 6/Kirigami application and its runtime dependencies.
- Install/uninstall is per-machine; no portable or per-user mode is currently planned.
- GUI execution remains unelevated even when the installer requires UAC.
- User data and logs belong in the appropriate per-user AppData locations.
- Machine-wide application files and any justified machine state must have explicit ownership and ACLs.
- Never overwrite an unrelated or unrecognized pre-existing installation silently.
- Uninstall must not remove shared WinDivert resources or user configuration unless the user explicitly chooses to remove that data.

## 6. Feature parity policy

Feature parity means that each platform provides the complete declared feature scope for a release. It does not mean that the underlying operating-system mechanisms must be identical.

For every user-visible feature, maintain a platform matrix with one of these states:

- **Implemented:** implemented and verified on that platform;
- **Limited:** available with documented limitations and tested fallback behavior;
- **Planned:** not implemented; must not be presented as available;
- **Unsupported:** intentionally outside the declared scope, with rationale and user-visible disclosure.

A Windows equivalent may use a different native mechanism. For example, Linux journal monitoring and Windows diagnostics may use different data sources. Such substitutions require their own security, privacy, bounded-output and test contracts.

Do not silently omit a feature on one platform. Do not mark a feature Implemented based only on a shared QML page or successful compilation.

## 7. CI and package validation

The required integrated verification set is:

1. Main Linux CI: configure, build, QML lint, unit tests and Linux integration tests;
2. Debian 13 DEB build and clean install/remove smoke test;
3. Fedora RPM build and clean install/remove smoke test;
4. Windows 11 x64 application CI: configure, build, QML lint, tests and deployment-manifest validation;
5. Windows package job: create NSIS installer and verify package contents and hashes.

All jobs in one integrated verification record must use the same exact commit SHA. Record passed, failed, skipped and not-run counts. Do not represent a Windows runner build as proof of WinDivert runtime operation or privileged lifecycle correctness.

Windows-specific acceptance must additionally run on a clean Windows 11 x64 VM and cover:

- first install, upgrade, repair and uninstall;
- UAC approval and UAC cancellation;
- standard-user GUI execution;
- engine start, readiness, status, stop, crash and recovery;
- WinDivert interaction and coexistence with another application using WinDivert;
- user data preservation and removal choices;
- network/firewall prerequisite failures;
- diagnostics and log redaction;
- restart and session-exit behavior.

Linux release gates remain mandatory. Windows CI must not replace Linux CI, DEB or RPM checks.

## 8. Implementation sequence

1. **Baseline audit:** inventory the current Linux implementation, source dependencies, build requirements, tests and user-visible features.
2. **Cross-platform boundary:** define the backend contracts and platform capability model without changing Linux behavior.
3. **Linux regression gate:** run full Linux CI and DEB/RPM packaging checks after the abstraction changes.
4. **Windows build graph:** isolate Linux-only dependencies and introduce a Windows build configuration while keeping the shared QML application.
5. **Windows backend prototype:** implement and test the supervisor, IPC, privilege boundary and runtime lifecycle.
6. **Runtime integration:** pin and verify the official upstream Windows x64 runtime; do not rebuild unchanged upstream binaries.
7. **Feature completion:** implement and test Windows equivalents for the declared feature scope; update the parity matrix.
8. **Packaging:** stage with KDE Craft, create NSIS installer, and validate install/upgrade/uninstall on clean VMs.
9. **Release readiness:** pass all Linux and Windows CI, package, security, license, crash-recovery and manual VM gates before declaring cross-platform support.

## 9. Current decision

- Linux remains supported and must not regress.
- Windows 11 x64 is an approved development target, not yet a supported release platform.
- The application should use one shared Qt 6/Kirigami UI with separate platform backends.
- Official prebuilt upstream Windows runtime binaries are the preferred first integration path.
- Rebuilding unchanged upstream binaries is not a prerequisite for the first Windows implementation.
- Linux and Windows CI/package checks are both mandatory before a cross-platform release.
