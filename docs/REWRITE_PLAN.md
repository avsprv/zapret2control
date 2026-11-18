# zapret2control — clean rewrite plan

## Product scope

- Linux desktop application only.
- KDE Plasma 6, Qt 6, current Kirigami, C++ and QML.
- Debian 13 and Fedora are the initial package/CI targets.
- Windows support, Windows abstractions, Windows packaging, and Windows CI are out of scope.
- The application is a complete graphical control surface for the applicable documented user-facing capabilities of upstream zapret2. It must preserve native zapret2 semantics and must not become a second implementation of zapret2. A full, versioned upstream capability matrix is a prerequisite for implementation; the first-release scope must not be reduced to installer + service controls + blockcheck.

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

The following are baseline journeys, not a complete capability list. Before implementation, derive the complete list from both upstream manuals and source at the pinned compatibility baseline and record it in `docs/UPSTREAM_CAPABILITY_MATRIX.md`.

1. Discover installation, upstream revision, runtime prerequisites, service and list-update state.
2. Install, reinstall/replace, and remove zapret2 using official upstream workflows, preserving user data according to upstream behavior.
3. Manage all applicable documented service lifecycle and startup behaviors.
4. Discover and use the upstream-supported version/update/reinstall workflow.
5. Inspect, edit, validate, and preserve all applicable upstream configuration semantics, including advanced options.
6. Manage all applicable upstream lists, list sources, downloads, refreshes, and scheduled updates.
7. Access all applicable documented runtime modes, filters, protocol/IP-family controls, and other user-facing configuration capabilities.
8. Run blockcheck2 with all applicable documented suites and test dimensions, visible logs, cancellation, and cleanup reporting.
9. Review diagnostic output and upstream-supported strategy workflows without silently applying results.
10. Access applicable upstream logs, diagnostics, custom scripts, and recovery/cleanup functions.
11. Configure application-only preferences, tray visibility, notifications, and autostart separately from zapret2 settings.

Every journey must map to one or more capability IDs in the matrix. No applicable native capability may be omitted solely because it is inconvenient to expose.

## Upstream capability coverage gate

`docs/UPSTREAM_CAPABILITY_MATRIX.md` is mandatory. The initial Linux inventory has been completed against the latest published release observed on 2026-11-19: zapret2 v1.0.7, commit `038c97e5819df10f8e8c22a2f1fffe57f2f7379b`. It covers the Linux engine, CLI options, profiles, protocol/payload recognition, Lua modules, shell configuration, lists/providers, firewall/service integration, install/remove/update behavior, blockcheck2, and helper programs. This is an inventory of upstream capabilities, not proof of zapret2control implementation or runtime support. Implementation must continue to use the matrix and update it when the baseline changes. It must record native mechanism, parameters, privilege requirements, side effects, output/errors, GUI exposure, implementation state, tests, and limitations for every applicable capability. Common controls belong in guided UI; advanced supported options must remain accessible in an advanced interface or through a transparent upstream terminal workflow. Do not fabricate controls or silently discard supported options.

## Initial architecture (intentionally small)

- **Application shell (Kirigami):** one compact navigation model and one active page at a time.
- **System state reader:** reads installation metadata, systemd service/timer state, and known paths. Read-only by default.
- **Process runner:** one small component for supervised external processes (argument vector, environment, stdout/stderr, cancellation, exit code).
- **Privilege boundary:** use polkit/pkexec for narrowly scoped privileged actions; do not make the whole GUI run as root.
- **Upstream lifecycle adapter:** stages a pinned upstream release and launches the official installer/uninstaller in an interactive terminal. It does not duplicate their logic.
- **Upstream process integration:** a shared, testable process supervisor launches native upstream scripts/commands with verified argument/environment contracts, terminal interaction where required, stdout/stderr capture, cancellation and exit reporting.
- **Blockcheck runner:** launches the upstream script and matching assets from the same pinned revision; exposes applicable documented inputs, streams raw output, and reports cleanup; any parser is optional, isolated, fixture-tested and version-bound.
- **Capability-driven UI:** exposes every applicable documented native capability through guided controls, advanced controls, or a clearly explained terminal passthrough; does not clone upstream algorithms.
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
6. **Native feature coverage:** implement the capability matrix in dependency order, including configuration, lists, modes, runtime options, updates, diagnostics, and other applicable upstream workflows.
7. **Blockcheck integration:** process supervision, all applicable documented suites/inputs, logs, cancellation, cleanup reporting, result presentation.
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
- Every applicable documented upstream user-facing capability is represented in the completed capability matrix and has a verified access path in the application (guided UI, advanced UI, or transparent native terminal workflow).
- No native option or workflow is silently lost, reinterpreted, or automatically applied.
- Blockcheck can be started with all applicable documented test settings, observed, cancelled, and audited; cleanup failures are visible.
- No shell evaluation of user or upstream output.
- Tray icon can be explicitly shown/hidden; close-to-tray and autostart are independent.
- No duplicated page title/brand and no simultaneous primary pages.
- QML imports have no explicit version numbers; Kirigami semantic theme colors and Units are used; qmllint passes.
- CI, DEB and RPM workflows pass on the same commit SHA, including package installation/removal smoke tests.
- Visual checks are completed in KDE Plasma 6 for light/dark themes and multiple window sizes.
