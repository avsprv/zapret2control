# zapret2control — clean rewrite plan

## Product scope

- Linux desktop application only.
- KDE Plasma 6, Qt 6, current Kirigami, C++ and QML.
- Debian 13 and Fedora are the initial package/CI targets.
- Windows support, Windows abstractions, Windows packaging, and Windows CI are out of scope.
- The application is a graphical control surface for upstream zapret2; it must not become a second implementation of zapret2.

## Rewrite principles

1. Start from user tasks, not from the old class hierarchy.
2. Use upstream scripts and documented interfaces for upstream lifecycle operations.
3. Prefer direct Qt/KDE/Linux APIs for desktop integration and service status.
4. Add an abstraction only when it removes a real dependency or makes a concrete behavior testable.
5. Keep privileged operations narrow, explicit, auditable, and asynchronous.
6. Never run shell command strings through a shell when a direct executable plus argument list is possible.
7. Never silently alter firewall rules, service state, user configuration, or installation files.
8. Do not parse undocumented upstream output as a stable API; preserve raw logs and version any optional parser.
9. Use Kirigami components and KDE HIG patterns rather than a custom visual framework.
10. Every feature must have a user-visible success, failure, cancellation, and recovery path.

## First-release user journeys

1. Detect whether zapret2 is installed and identify its upstream version/revision.
2. Install zapret2 through the official `install_easy.sh` workflow.
3. Start, stop, restart, and inspect the upstream systemd service.
4. Run blockcheck2 with explicit test settings, visible live output, cancellation, and cleanup status.
5. Review diagnostic results without automatically applying them.
6. Open and safely edit supported zapret2 configuration files, with validation and backup.
7. Run the official `uninstall_easy.sh` workflow and separately offer deletion of residual installation files.
8. Configure application behavior, tray visibility, and autostart.
9. View application/upstream information and logs.

## Initial architecture (intentionally small)

- **Application shell (Kirigami):** one compact navigation model and one active page at a time.
- **System state reader:** reads installation metadata, systemd service/timer state, and known paths. Read-only by default.
- **Process runner:** one small component for supervised external processes (argument vector, environment, stdout/stderr, cancellation, exit code).
- **Privilege boundary:** use polkit/pkexec for narrowly scoped privileged actions; do not make the whole GUI run as root.
- **Upstream lifecycle adapter:** stages a pinned upstream release and launches the official installer/uninstaller in an interactive terminal. It does not duplicate their logic.
- **Blockcheck runner:** launches the upstream script and streams raw output; optional parser isolated and version-bound.
- **Configuration access:** explicit file model, safe writes, validation, and recoverable backups; no shell evaluation.
- **KDE integration:** standard Kirigami navigation, standard StatusNotifierItem integration, desktop autostart, notifications, and semantic icon/theme assets.

Avoid introducing a generic cross-platform backend, a plugin framework, a custom D-Bus menu implementation, or a multi-layer command bus unless a demonstrated requirement emerges.

## UI direction

- One selected section is visible in the content area.
- No duplicate application logo/name in the sidebar and page header.
- Compact navigation; the content page owns its single title.
- Main page prioritizes service state and primary service actions.
- Configuration, diagnostics, logs, updates, and settings have clear separate responsibilities.
- Settings are grouped by task; no duplicate nested headers or parallel copies of the same setting.
- Tray icon visibility is independent from close-to-tray behavior and autostart.
- Follow KDE HIG, Breeze semantics, Kirigami Units, accessibility, keyboard navigation, and responsive layout.
- Design and verify both light and dark themes and small/large window sizes before declaring UI complete.

## Proposed implementation sequence

1. **Baseline and requirements:** pin upstream release; reconcile project kit's former cross-platform rules with the approved Linux-only scope; inventory user journeys and acceptance criteria.
2. **Minimal application shell:** clean CMake/Qt 6/Kirigami app, single-page navigation, metadata, icons, translations, basic tests.
3. **Read-only detection:** installed state, version, systemd service/timer state, diagnostics without modifying the system.
4. **Service control:** direct systemd integration with explicit authorization and error handling.
5. **Installer/uninstaller integration:** verified release staging and official interactive scripts; clean-install and existing-install tests.
6. **Blockcheck integration:** process supervision, controlled inputs, logs, cancellation, cleanup reporting, result presentation.
7. **Configuration and profiles:** only after source/manual mapping and safe validation rules.
8. **Tray/autostart/settings:** standard KDE integration and independent settings.
9. **Packaging and release:** Debian 13 DEB, Fedora RPM, install/remove smoke tests, CI on one SHA, release verification.

## Rewrite migration policy

- The old implementation is reference material only. Do not port its architecture or controllers by default.
- Keep the old code/branches intact until the new implementation reaches feature parity for the approved first-release journeys.
- Develop the rewrite on a separate branch and use small reviewable commits.
- Do not merge the old UI PRs into the rewrite branch.
- Remove obsolete files only after the new implementation and migration checklist are verified.
- Every milestone requires tests and a short evidence-based review before moving to the next one.

## Acceptance criteria

- A fresh Debian 13/Fedora installation can be detected and managed without undocumented assumptions.
- Official upstream installer/uninstaller behavior is preserved and clearly disclosed.
- Service actions report actual systemd state, not merely successful process launch.
- Blockcheck can be started, observed, cancelled, and audited; cleanup failures are visible.
- No shell evaluation of user or upstream output.
- Tray icon can be explicitly shown/hidden; close-to-tray and autostart are independent.
- No duplicated page title/brand and no simultaneous primary pages.
- QML imports have no explicit version numbers; Kirigami semantic theme colors and Units are used; qmllint passes.
- CI, DEB and RPM workflows pass on the same commit SHA, including package installation/removal smoke tests.
- Visual checks are completed in KDE Plasma 6 for light/dark themes and multiple window sizes.
