# AGENTS.md — Instructions for zapret2control development

These instructions are mandatory for every agent, assistant, contributor, code change, review, and project-planning task in this repository.

## 1. Product mission

Build **zapret2control**, a Linux desktop application that exposes the documented and supported user-facing capabilities of upstream **zapret2** through a coherent KDE Plasma 6 application.

The application is a graphical control and integration layer around upstream zapret2. It is not a replacement implementation of zapret2, its packet-processing engine, its strategy-testing engine, its installer, or its configuration semantics.

The central product requirement is:

> A user must be able to discover, understand, configure, execute, monitor, and recover the applicable штатные (native/upstream) zapret2 functions from zapret2control, without losing upstream capabilities or being forced to maintain a parallel set of hand-written shell commands for ordinary supported workflows.

Do not reduce the application to a service start/stop panel plus an installer. Do not implement only the functions that are easiest to expose.

## 2. Approved scope and precedence

- Linux only.
- KDE Plasma 6, Qt 6, current supported Kirigami, C++, QML, CMake.
- Initial package/CI targets: Debian 13 DEB and Fedora RPM.
- systemd is the initial supported service manager.
- Windows support, Windows backends, Windows packaging, Windows CI, and Windows feature-parity work are out of scope.

The approved Linux-only product decision supersedes conflicting cross-platform and Windows requirements in project-kit v1.1.19. Keep generally applicable Qt/KDE, security, quality, repository, and release requirements. Do not implement or test Windows features.

Normative references:
- `docs/PROJECT_SCOPE_DECISION.md`
- `docs/REWRITE_PLAN.md`
- `docs/UPSTREAM_SCRIPT_AUDIT.md`
- project-kit v1.1.19: `instructions/PROJECT_RULES.md`, `skills/zapret2-integration.md`, `skills/qt-ui-design-review.md`, development, code-review, CI, testing, and release workflows.

If this file and another project document conflict, follow the approved Linux-only scope and the most specific current project decision. Record and correct the stale document; do not silently revive Windows requirements.

## 3. Mandatory upstream-first development

The upstream zapret2 project owns its engine, supported configuration format, runtime behavior, installation/removal behavior, diagnostics, strategy algorithms, and other native functions.

Canonical upstream repository:
- https://github.com/bol-van/zapret2

Documentation precedence:
1. `docs/manual.md` — primary manual.
2. `docs/manual.en.md` — supplementary manual.
3. Actual source implementation for behavior that is missing, ambiguous, or contradictory in the manuals.

For every feature or integration change:

1. Identify the exact upstream compatibility baseline.
2. Read the relevant sections of the Russian primary manual.
3. Check the corresponding English manual section.
4. Inspect the implementation at the same exact upstream commit when needed.
5. Record discrepancies, limitations, and conclusions.
6. Verify actual command names, arguments, environment variables, config semantics, exit codes, privilege needs, filesystem effects, service effects, cleanup, and platform requirements.
7. Add or update tests against that exact baseline.

Never rely on memory, a previous assistant response, an old local manual copy, a mutable branch name, or assumptions about how zapret2 works. Reopen the actual source when details matter.

### 3.1 Upstream source trust

Use only `https://github.com/bol-van/zapret2`. Fetch over HTTPS with normal TLS and hostname validation. Do not silently use mirrors, forks, alternate hosts, HTTP, or disabled certificate checks.

Resolve branches/tags to a full commit SHA. Verify checked-out HEAD equals the expected SHA and Git reports no object-integrity errors. Record:
- canonical source URL;
- full commit SHA and upstream release/version if present;
- retrieval/verification date;
- both manual revisions;
- signature status (`verified`, `unavailable`, or `invalid`) and independently established trust-key identity where applicable;
- residual trust limitations.

An exact SHA identifies content but does not by itself authenticate its publisher. Do not claim publisher authentication without independently verified signing identity. Block on TLS/source identity failure, commit mismatch, Git integrity failure, ambiguous reference, or invalid signature when verification is attempted.

The master branch is a tracking reference, not a compatibility pin. Do not silently change the pinned baseline as part of unrelated work.

## 4. Full native-function coverage is a release requirement

Before implementing more UI or backend abstractions, build and maintain a **versioned Upstream Capability Matrix** from the manuals and source of the selected baseline.

The matrix must enumerate all applicable, documented, user-facing zapret2 capabilities. Do not limit the inventory to the functions already present in the old application or to the two scripts previously audited.

At minimum, investigate and map these capability areas where they exist in the pinned upstream revision:

- installation, reinstallation, replacement, and removal;
- service lifecycle, startup, shutdown, restart, status, and autostart;
- upstream version/revision discovery and supported update/reinstall workflow;
- configuration files, parameters, defaults, validation, includes, and migration;
- runtime modes and supported filtering/processing options;
- IPv4 and IPv6 controls;
- firewall backend and related integration;
- list management, list sources, downloads, refresh, scheduling, and update timer/service;
- custom scripts and user-managed files;
- diagnostics, logs, and runtime status;
- `blockcheck2.sh`, its suites, test dimensions, supported input variables, output, cancellation, and cleanup;
- supported strategy/configuration selection and application workflow;
- Lua modules/APIs or other upstream extension points intended for user operation;
- binary/component compatibility and prerequisites;
- platform/init-system restrictions and unsupported modes;
- recovery, cleanup, and known failure cases.

This list is an investigation checklist, not a claim that every item exists in every upstream revision. Confirm each item in the pinned manuals/source. Add newly discovered capabilities; remove or mark inapplicable items only with evidence.

Each matrix row must include:
- capability ID and user-facing name;
- manual section and source file/function;
- upstream revision;
- upstream support/availability conditions;
- native upstream mechanism (script, CLI, config, service, file, API, etc.);
- input parameters and valid values;
- privilege requirements;
- side effects and files/services/firewall affected;
- expected output/status/error behavior;
- zapret2control exposure (main UI, advanced UI, guided workflow, terminal passthrough, or intentionally not exposed);
- implementation status: Planned / Implemented / Tested / Supported / Limited / Unsupported / Blocked;
- test evidence and known limitations.

Do not mark a capability Supported because a button exists or the code compiles. It is Supported only when behavior, failure paths, permissions, side effects, cleanup, and required integration tests are verified on the declared Linux targets.

No applicable native capability may disappear merely because it is inconvenient to expose. If a capability cannot safely be exposed in the GUI, explain why and provide a transparent, documented way to invoke the upstream workflow where appropriate. Do not pretend it is supported.

### 4.1 User interface must expose the upstream model, not replace it

Design pages around user tasks and native zapret2 concepts. Use understandable labels and help text, while preserving the actual upstream option names in advanced details/logs where useful.

- Common workflows should be guided and discoverable.
- Advanced users must not lose access to supported upstream parameters.
- Do not invent GUI-only settings that have no defined effect.
- Do not map several upstream options to one ambiguous control.
- Do not silently omit upstream defaults, modes, or supported values.
- When an upstream option is unsupported by the application, label it clearly; never display a fake working control.
- Preserve the ability to inspect the effective configuration and the exact command/environment used, with secrets redacted.
- Do not automatically apply a diagnostic recommendation or rewrite a user's config without explicit confirmation.

## 5. Native integration policy: wrap, do not clone

Use upstream mechanisms as the source of truth.

Examples:
- use official `install_easy.sh` for upstream installation;
- use official `uninstall_easy.sh` for upstream-managed removal;
- use `blockcheck2.sh` for strategy diagnostics;
- use upstream service units/scripts and systemd for service lifecycle;
- use upstream configuration and list formats;
- use the upstream-supported update/reinstall procedure for the selected revision.

Do not recreate upstream installation, firewall setup, strategy testing, list scheduling, or service orchestration in C++ when upstream already owns that behavior.

Do not patch upstream scripts or rely on undocumented prompt order/environment behavior as if it were a stable API.

Where an upstream tool is interactive and lacks a documented machine interface, preserve its real interaction in a terminal/PTY. Do not guess answers, pipe a fabricated answer stream, or silently replace the official workflow with a custom installer.

The application may add a guided front end only if it invokes a documented/stable upstream interface or a thoroughly tested, explicitly version-bound adapter. Preserve an escape hatch to the original upstream workflow.

For every external process, use a program path plus argument vector, never shell-concatenated command text. Control and document the environment. Capture stdout and stderr separately, show progress, support cancellation where safe, report the real exit status, and verify resulting system state independently.

## 6. Specific requirements for audited scripts

### 6.1 install_easy.sh

The audited snapshot showed that the installer is interactive, may replace the target installation after confirmation, may preserve selected user files, may install prerequisites, and on systemd enables/starts zapret2 plus its list-update timer.

These observations are specific to the audited revision. Re-verify them against the selected integration baseline before relying on them.

Requirements:
- stage a complete, integrity-verified upstream tree before invoking the script;
- explain replacement and preservation consequences before starting;
- run in a real terminal/PTY with visible prompts and output;
- never assume a clean system already has `/opt/zapret2/install_easy.sh`;
- never run a custom parallel installation engine;
- after completion, inspect service, timer, installation tree, and config state rather than trusting process exit alone;
- clearly disclose package-manager changes and privilege escalation.

### 6.2 uninstall_easy.sh

The audited snapshot showed that the systemd uninstaller removes upstream-managed service/timer/firewall/list state but leaves the installation directory.

Re-verify this against the selected baseline.

Requirements:
- invoke the official uninstaller;
- distinguish upstream-managed removal from optional deletion of residual files;
- preserve user config/lists unless the user explicitly confirms deletion;
- report what remains and why;
- verify service, timer, firewall and filesystem state after completion.

### 6.3 blockcheck2.sh

The audited snapshot showed that blockcheck2 is a privileged diagnostic workflow that temporarily modifies firewall rules, runs test `nfqws2` processes, invokes curl probes, performs DNS/IP diagnostics, and emits human-readable output rather than a documented JSON API.

Re-verify every detail against the selected baseline.

Requirements:
- use the upstream script and its matching `blockcheck2.d`, `common`, Lua, binaries and required assets from the same pinned revision;
- expose all applicable documented test dimensions and supported settings through a clear basic/advanced UI;
- do not invent environment variables, modes, suites or accepted values;
- show the safety preconditions before starting (VPN/other bypass tools/current zapret state as documented);
- make privilege escalation explicit;
- stream raw stdout/stderr and preserve a complete log;
- support safe cancellation and report whether cleanup completed;
- never claim cleanup succeeded without evidence;
- do not stop the live zapret service or other bypass tools without explicit user consent and a documented restore plan;
- do not treat curl success as universal browser/network success;
- never parse output with shell evaluation or execute strategy strings;
- any parser must be optional, fixture-tested, tied to a specific upstream revision, and must preserve raw output;
- never apply a suggested strategy or alter live configuration automatically.

## 7. Configuration and user data ownership

Treat upstream configuration as upstream-defined syntax, not as an arbitrary generic key/value file.

Before editing:
- inspect the exact parser and configuration loading behavior;
- identify default, user-managed, generated and included files;
- preserve unknown options and comments wherever feasible;
- validate changes against the pinned upstream parser or a verified compatible validator;
- write atomically with safe permissions;
- create recoverable backups;
- never source/evaluate a config file in the GUI process;
- never replace an invalid or unreadable config with defaults without explicit consent;
- show the effective config and distinguish application-managed values from user-managed values.

Routine updates must not overwrite user configuration, custom scripts, lists, or other user-owned data.

Maintain an ownership map for upstream binaries, scripts, Lua modules, config, lists, generated state, logs, temporary files and staging directories.

## 8. Security and privilege boundary

The GUI must run unprivileged.

Use polkit/pkexec or another KDE/Linux-standard authorization mechanism for narrowly scoped privileged operations. Do not elevate the entire application.

Security requirements:
- least privilege and explicit user consent;
- strict validation of every argument and path;
- no shell `eval`, no command-string execution, no command injection;
- safe temporary files and atomic writes;
- canonical path checks and symlink protections for privileged file operations;
- HTTPS/TLS validation and integrity checks for downloaded content;
- no execution of downloaded code before source/artifact checks;
- no secrets in logs;
- safe cancellation, timeout, child-process cleanup and crash recovery;
- audit logs for privileged operations;
- clear error reporting when authorization is denied or an operation partially completes.

A checksum downloaded from the same untrusted location is not publisher authentication. Follow the project-kit trust policy and record residual risk.

## 9. Minimal architecture; no abstraction for its own sake

Do not port the old architecture by default. Treat the old code as reference material only.

Start with the smallest architecture that can implement the verified user journeys:
- Kirigami application shell and page navigation;
- read-only upstream installation/system state inspection;
- one robust supervised-process component;
- narrowly scoped privilege integration;
- focused adapters for systemd and upstream scripts;
- configuration access with safe ownership/validation;
- blockcheck execution and result presentation;
- KDE desktop/tray/autostart integration.

Add an interface, service, controller, manager, generic framework, plugin system, command bus, or abstraction only when a concrete requirement and test justify it.

Do not scatter shell commands in QML. QML must not construct privileged commands or access upstream runtime files directly. Keep QML as presentation and user interaction; put process/system operations in testable C++ components.

Prefer direct Qt/KDE/Linux APIs over custom protocols. Prefer maintained KDE/Qt facilities over hand-written StatusNotifierItem/D-Bus menu implementations.

Every component must have one clear responsibility. Avoid one class per trivial operation and avoid multiple layers that only forward calls.

## 10. UI and KDE integration requirements

The interface must be redesigned fully, not cosmetically patched from the old UI.

- Show exactly one active primary page in the content area.
- No duplicated application title/logo in both global navigation and page header.
- Compact navigation organized around user tasks, not internal class names.
- Each page has one clear purpose and one title.
- Settings are grouped logically and avoid duplicate headings/controls.
- Service status and primary actions must be immediately understandable.
- Separate zapret2 controls from application preferences.
- Separate tray icon visibility, close-to-tray, start-minimized, and application autostart; they are not synonyms.
- The tray icon must be explicitly configurable and its registration/failure state visible and recoverable.
- Use current Kirigami components and KDE HIG.
- QML imports have no explicit version suffix.
- Use semantic Kirigami theme colors; no hardcoded HEX/RGB for ordinary UI.
- Use `Kirigami.Units` for spacing and layout.
- Support light/dark desktop themes, keyboard navigation, accessibility, and responsive layouts.
- Run Qt 6 qmllint; do not suppress real issues.

Do not declare the redesign complete from a screenshot or mockup. Verify the running application in a real KDE Plasma 6 session at multiple window sizes and both themes.

## 11. Build, tests, CI, and packaging

- Build with Qt 6/CMake using the documented dependency baseline.
- Keep QML lint clean and include all QML files in lint coverage.
- Unit-test process supervision, config handling, state detection, error paths and any parsers.
- Integration-test actual upstream interactions against a pinned revision.
- Use disposable VMs/containers for installer, uninstaller, systemd, firewall and blockcheck tests; do not run destructive integration tests on a developer's host.
- Test cancellation, timeout, process crash, partial install/remove, permission denial and recovery.
- Validate Debian 13 DEB and Fedora RPM packages, including installation and removal smoke tests.
- Linux CI, Debian 13 DEB and Fedora RPM checks must all target the exact same commit SHA. Report the SHA and actual workflow/job/test results; never invent counts or success.
- Keep old and new implementation separate until the rewrite meets the approved capability matrix and acceptance criteria.
- Do not merge a PR merely because it builds; review behavior, security, UI, tests and packaging.

## 12. Repository and workflow discipline

Before any significant task:
1. Read this file and relevant project-kit instructions/skills/workflows.
2. Inspect the current repository, branch, commit and open PRs.
3. Inspect actual current source and tests; do not rely on old summaries alone.
4. State the concrete goal and acceptance criteria.
5. Make a small, reviewable change on the rewrite branch.
6. Run relevant checks and inspect their real output.
7. Report exact files, commit SHA, checks, failures and remaining work.

Use feature branches and pull requests. Do not push directly to main. Do not merge without the required review and passing checks.

Do not claim that a file was changed, a test was run, a CI workflow passed, a script behaved a certain way, or a feature works unless verified from actual tool output or a reproducible test.

## 13. Required development order

Follow this order unless a documented dependency requires otherwise:

1. Pin the actual upstream release/commit for the first supported integration.
2. Produce the complete Upstream Capability Matrix from both manuals and source.
3. Produce user journeys and map each journey to one or more native upstream capabilities.
4. Approve the feature scope and identify unsupported/limited capabilities.
5. Design the information architecture and UI around those user journeys.
6. Build the minimal Qt 6/Kirigami application shell and design system.
7. Implement read-only detection and state reporting.
8. Implement supervised upstream process execution and privilege boundary.
9. Integrate service management and native installer/uninstaller workflows.
10. Integrate blockcheck2 fully, including its applicable options, output, cancellation and cleanup.
11. Implement configuration, lists, profiles/strategy workflows and other mapped native functions.
12. Implement update/recovery flows.
13. Implement tray, autostart, notifications and application settings.
14. Complete integration tests, visual review, packaging and release gates.

Do not start a later step while a prerequisite remains unverified. Do not build the full application around an incomplete or guessed capability inventory.

## 14. Definition of done for a feature

A feature is complete only when all applicable items are true:
- mapped to an upstream capability or explicitly identified as application-owned;
- verified against the pinned upstream baseline;
- implemented with a clear user-facing workflow;
- preserves native semantics and supported options;
- handles permission denial, invalid input, failure, cancellation and partial completion;
- protects user data and system state;
- has automated tests and integration evidence appropriate to its risk;
- is accessible and consistent with KDE HIG;
- has documentation and known limitations;
- passes relevant lint/build/CI/package checks.

A visible control without a verified working behavior is a defect, not a partially complete feature.

## 15. Reporting format

For each completed work unit, report:
- objective and scope;
- upstream baseline used;
- files changed;
- user-visible behavior;
- tests executed and exact results;
- CI status on the exact commit SHA;
- security/system side effects;
- known limitations and next dependency.

Be explicit about what has only been read, what has been implemented, what has been tested, and what is actually supported.
