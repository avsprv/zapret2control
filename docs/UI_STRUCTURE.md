# zapret2control — complete UI structure

**Status:** proposed UI specification; no implementation is implied.  
**Upstream baseline:** zapret2 v1.0.7, commit `038c97e5819df10f8e8c22a2f1fffe57f2f7379b`.  
**Platform:** KDE Plasma 6, Qt 6, Kirigami; Debian 13 and Fedora; systemd.  
**Requirements source:** `docs/UPSTREAM_CAPABILITY_MATRIX.md`.

## 1. Core design rules

- The application is a control surface for native zapret2, not a replacement for its engine, scripts, firewall logic, list generation or diagnostics.
- Show exactly one selected primary section in the content area.
- Do not repeat the application icon/name inside the page. Do not add a second branding header or duplicate page title.
- Use compact, conventional Kirigami navigation. No nested hamburger navigation and no permanent oversized sidebar header.
- Keep application preferences separate from zapret2 configuration and system service state.
- Common tasks are guided; all applicable advanced upstream capabilities remain accessible through advanced controls, raw configuration or a transparent upstream terminal workflow.
- No fake controls, inferred option semantics, optimistic success, hidden shell execution or automatic application of diagnostic recommendations.
- The GUI runs unprivileged. Elevate only the specific privileged operation.
- Use Kirigami/Breeze components, semantic theme colors and `Kirigami.Units`; no hardcoded colors or custom visual framework.

## 2. Main window and navigation

One Kirigami application window with a compact left navigation on desktop and standard collapsed drawer on narrow widths. The selected page occupies the content area. Page-specific actions belong to that page. No persistent cards or navigation widgets that duplicate the active page's information.

Primary navigation, in order:

1. **Обзор**
2. **Конфигурация**
3. **Списки**
4. **Диагностика**
5. **Журнал**
6. **Управление zapret2**
7. **Настройки приложения**
8. **О программе**

Do not create a top-level item for each individual upstream option or Lua helper. Internal page subsections are selected one at a time; do not stack every form vertically.

Responsive requirements: readable desktop default size (not a narrow phone-like aspect ratio), one-column forms on narrow windows, adaptive tables, usable process output while resizing, keyboard navigation, visible focus and accessible labels. Verify light/dark themes and multiple window sizes.

## 3. Page structure

## 3.1 Обзор

**Purpose:** immediately show whether zapret2 is installed and running, and the primary action available.

Content order:
1. Installation: installed/not installed/incomplete/unknown; detected upstream version/revision; path; explanation of uncertain detection.
2. Service: actual systemd unit state; runtime/process evidence where available; detected firewall backend (nftables/iptables/unknown); IPv4/IPv6 indicators.
3. Primary action: Start or Stop according to state; Restart as a distinct secondary action. Explain disabled actions.
4. Related state: service autostart; list-update timer; latest operation/error.
5. Shortcuts to Configuration, Lists, Diagnostics and Journal.

Do not report healthy state solely because a systemctl command returned zero. Do not duplicate full service controls elsewhere. Update availability links to Управление zapret2.

## 3.2 Конфигурация

**Purpose:** expose native config semantics without losing shell syntax, profiles, filters, CLI options or Lua extensions.

Internal sections (one selected at a time):

### Основные параметры
Guided controls for `NFQWS2_ENABLE`, TCP/UDP ports and packet windows, IPv4/IPv6, `FWTYPE` (automatic/nftables/iptables), `MODE_FILTER`, `INIT_APPLY_FW`, packet marks and applicable flow-offload policy. Every control shows exact upstream key, meaning, accepted values, default where documented and applicability. Never infer meaning from variable names.

### Профили и стратегии
Show profiles in native order, names, enabled/skip state, filters and Lua desync calls. Support upstream templates/imports. Editing must preserve ordering, unknown options, comments and valid `--lua-desync` calls. Include read-only effective command preview and raw view. Do not reduce the strategy system to a small incomplete preset list.

### Фильтры и протоколы
Guided and advanced controls for L3/L4/L7, packet marks, SSID filters where applicable, hostlist/IP-set include/exclude, payload and packet-range filters, auto-hostlist thresholds/diagnostics. Provide searchable exact protocol/payload values from the matrix, including special values. Preserve arbitrary valid upstream combinations.

### Хостлисты и IP-фильтры
Show which list files are referenced and their ownership/type. Actual list editing belongs in Списки; do not duplicate list editors here.

### Расширенные параметры nfqws2
Searchable reference/editor grouped by process/debug, Lua initialization/blobs/writable directory, conntrack/reassembly, IP/hostname cache, server/listener, Linux queue/user/UID/bind-fix/fwmark, and every other applicable Linux option. Show exact name, grammar, values, default if documented, applicability and warning. Unknown future options survive edits.

### Lua и пользовательские расширения
Browse installed six upstream Lua modules, user Lua files, custom strategy files and fake blobs. Show exact module/function names and references. Optional source viewer/editor with backup, validation and explicit save. Lua is not executed in the GUI. Do not turn each Lua helper into a generic toggle.

### Исходный config
Lossless raw editor for upstream shell config. Preserve comments, unknown variables, expressions and ordering where possible. Explicit save, diff, backup, validation result and restore. Never source/evaluate config in the GUI. Explain that static parsing cannot fully evaluate arbitrary shell syntax.

Actions: validate only through a verified upstream mechanism (explain limits); preview diff; backup; apply with explicit confirmation; restore; import/export. Never silently replace an invalid config with defaults.

## 3.3 Списки

**Purpose:** manage upstream list files and scheduled refresh without implementing a parallel downloader.

Internal sections:

- **Обзор списков:** user/generated/auto hostlists; user/generated IPv4 and IPv6 lists; exclusions; ipban; compressed variants. Show path, ownership, size, modification time, source and last result. Counts only when safely measurable.
- **Пользовательские списки:** edit/import/export user-owned files; validate domain, IP and CIDR entries; protect generated lists from accidental editing.
- **Источники:** upstream provider scripts, role, configured GETLIST selection, source/provider, last result/error. Do not imply all upstream sources are current or equally reliable.
- **Обновление:** invoke upstream `ipset/get_config.sh` or the appropriate documented script; stream output; preserve logs; use upstream clear operation only with confirmation. No custom downloader or list-generation engine.
- **Планировщик:** systemd `zapret2-list-update.timer` and associated service state, last/next trigger when available, enable/disable/start/stop with impact confirmation. This timer is independent of zapret2 service autostart.
- **Наборы ядра:** inspect `zapret`, `zapret6`, `nozapret`, `nozapret6`, `ipban`, `ipban6` when supported. Explain ipban does not itself implement routing/proxy behavior. Do not mutate sets outside upstream operations.

## 3.4 Диагностика

**Purpose:** transparent UI for original `blockcheck2.sh`.

### Перед запуском
Show exact upstream version/tree, service state, firewall backend, known prerequisites, privilege requirement and warning that blockcheck temporarily changes firewall state and launches test processes. Never stop VPNs, bypass tools or active zapret service automatically. Ask separately before any required state change.

### Быстрый запуск
Documented inputs: domains/URIs; IPv4/IPv6/both; HTTP, HTTPS TLS 1.2, HTTPS TLS 1.3, HTTP/3 QUIC; repeats; parallelism; pace; scan levels `veryquick`, `quick`, `standard`, `force`; common DNS/DoH settings.

### Расширенные параметры
Expose all applicable documented variables in groups: targets; protocol switches; curl binary/options/timeouts/padding; DNS/DoH; IP-block checks; scan/repeat; extra nfqws2 args before/after; TTL/autottl; fake repeats/fooling; fake/pattern files; per-test `NOTEST_*`; custom suite. Use exact names and source-confirmed values. Include reset-to-upstream-default.

### Наборы тестов
Show all 13 standard modules and their actual run/skip state. Show custom suite files: `list_http.txt`, `list_https_tls12.txt`, `list_https_tls13.txt`, `list_quic.txt`. Missing dependencies or skipped tests must be visible.

### Выполнение и результаты
Run upstream script with matching assets in a real PTY when prompts are needed. Stream stdout/stderr, preserve full raw output and exact run settings, show process state and elapsed time. Cancellation must report process termination and cleanup separately. Parsed results are supplemental; raw output is authoritative. Explain curl/IP-block limitations. Recommendations never change live config automatically; applying/copying a suggestion requires explicit action and diff preview.

## 3.5 Журнал

Separate sources:
- **Операции приложения:** install/reinstall/remove, service actions, list refresh, config validation/apply, blockcheck, authorization/cancellation.
- **Журнал zapret2:** systemd journal for service, list timer/service, upstream process output.
- **Фильтры:** time, severity, source, operation, text.
- **Детали:** timestamp, executable, argument vector (secrets redacted), environment summary, exit code, stdout/stderr, verified state and cleanup result.

Allow copy/export/open system journal. Clearing application-owned logs must not delete upstream logs through an ambiguous “clear all”.

## 3.6 Управление zapret2

**Purpose:** upstream installation lifecycle, release and removal. Runtime service controls remain on Обзор.

- **Установка:** detect existing tree; choose published release; download complete archive; verify SHA-256 and disclose trust limitations; stage safely; check architecture/prerequisites; launch official `install_easy.sh` in visible PTY; leave real upstream prompts to user; verify installation, config, service and timer after exit.
- **Версия и обновление:** show installed and available release, notes and digest. v1.0.7 manual does not define a separate non-interactive update API. Use staged official release and official installer with visible preservation prompts; never invent an `update` subcommand.
- **Переустановка:** release selection, backup/preservation preview, official installer in PTY, postcondition checks.
- **Удаление:** explain official `uninstall_easy.sh` effects; run in visible PTY; verify service/timer/firewall/list state. State clearly that upstream uninstaller leaves the installation directory. Offer residual-file deletion as a separate destructive action with preview and confirmation. Preserve user config/lists/scripts unless separately confirmed for deletion.
- **Компоненты и зависимости:** architecture, selected upstream binary, required packages/tools and missing prerequisites. No arbitrary unofficial binary replacement.

## 3.7 Настройки приложения

Only zapret2control preferences. No engine config, firewall or zapret2 service settings.

- **Системный трей:** independent controls for (1) show tray icon, (2) close/minimize to tray, (3) start minimized to tray. Tray visibility must not depend on close-to-tray. Show registration failure and retry/diagnostic action. Use standard KDE StatusNotifierItem integration, not custom D-Bus protocol implementation.
- **Запуск приложения:** launch on login, start minimized, restore window state.
- **Уведомления:** service changes, operation result, list refresh, update availability, blockcheck completion.
- **Внешний вид:** follow system theme; optional app theme only if KDE-consistent; Breeze/KDE icon semantics.
- **Дополнительно:** language if translated, application log retention, diagnostics bundle privacy preview, reset preferences.

## 3.8 О программе

Application version/purpose, upstream compatibility baseline, project and upstream links, documentation, licenses/third-party components, diagnostics export with preview/redaction. No duplicate app identity in page body.

## 4. Shared operation lifecycle

Every long-running operation follows: Ready (preconditions/current state) → Review (exact action, files/services, privilege and effects) → Authorize → Running (real progress/output/cancellation) → Verifying (independent postconditions) → Result (success/partial/failure/cancelled and recovery). Exit code alone is not proof of success. If verification is impossible, state so.

## 5. Capability-to-page mapping

| Matrix IDs | Primary page | Access |
|---|---|---|
| ENG-01..02 | Обзор | Конфигурация, Журнал |
| ENG-03..06 | Конфигурация | Advanced/raw config |
| ENG-07 | Конфигурация → Фильтры и протоколы | Searchable reference |
| ENG-08 | Конфигурация → Расширенные параметры | Журнал |
| LUA-01..06 | Конфигурация → Lua и расширения | Source/reference |
| CFG-01..03 | Конфигурация | Исходный config |
| LIST-01..06 | Списки | Обзор summary |
| SVC-01 | Обзор | Журнал |
| SVC-02 | Обзор / Списки | App autostart only in app settings |
| SVC-03 | Not supported in initial scope | Explain limitation |
| SVC-04 | Конфигурация → expert workflow | Native docs/terminal |
| FW-01..03 | Конфигурация | Read-only backend status on Обзор |
| INST-01..03, INST-04, UPD-01, AUX-03 | Управление zapret2 | Журнал |
| BC-01..09 | Диагностика | Журнал |
| AUX-01..02 | Списки | Internal helper details |

A visible page/control does not mean a capability is supported. Each matrix row must link to implementation and test evidence and use statuses Planned/Implemented/Tested/Supported/Limited/Unsupported/Blocked.

## 6. Explicitly forbidden UI patterns

- Duplicate app icon/name or duplicate page title.
- Multiple primary pages displayed simultaneously.
- Nested hamburger menus or oversized permanent branding header.
- One top-level navigation item per individual option.
- Duplicate settings in several sections.
- Mixing app preferences with zapret2 configuration.
- Fake progress/health and optimistic success.
- Controls without verified upstream effect.
- Custom installer, downloader, firewall engine or blockcheck implementation.
- Automatic application of recommendations or automatic stopping of VPN/bypass tools/service.

## 7. Visual acceptance checklist

- [ ] One active page at a time.
- [ ] No duplicate app identity in page content; one page title.
- [ ] Compact KDE-native navigation.
- [ ] Each page has one responsibility.
- [ ] Overview makes installation and service state immediately clear.
- [ ] Config preserves native syntax and all applicable advanced options.
- [ ] Lists distinguish user/generated/auto/kernel data.
- [ ] Diagnostics invokes matching upstream blockcheck assets.
- [ ] Journal distinguishes application and upstream logs.
- [ ] Lifecycle uses official scripts and visible PTY.
- [ ] App settings are separate; tray controls independent.
- [ ] Light/dark and multiple window sizes tested.
- [ ] Keyboard navigation/accessibility tested.
- [ ] Kirigami semantic colors and Units only.
- [ ] QML imports have no explicit versions; Qt 6 qmllint passes.

## 8. Implementation sequence

1. Build the shell and page placeholders only; visually review navigation and hierarchy in Plasma 6 before feature work.
2. Implement read-only Overview/system state.
3. Implement upstream lifecycle through official scripts.
4. Implement systemd service/timer state and actions.
5. Implement Lists.
6. Implement Configuration: lossless raw config first, then guided editors.
7. Implement Diagnostics and PTY blockcheck.
8. Implement Journal and operation history.
9. Implement app preferences and standard KDE tray integration.
10. Complete accessibility, visual verification and packaging.

Do not begin full feature implementation until the shell/navigation has been visually reviewed in KDE Plasma 6.
