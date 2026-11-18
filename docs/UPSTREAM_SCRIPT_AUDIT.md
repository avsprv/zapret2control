# Upstream Script Audit: `install_easy.sh` and `blockcheck2.sh`

## 1. Scope and source baseline

This audit is based on the upstream primary Russian manual and source files at one immutable revision.

| Item | Value |
|---|---|
| Upstream repository | https://github.com/bol-van/zapret2 |
| Upstream commit | `3bae3d26d741a8f6e0bc4ea651656b4e1a438375` |
| Commit date | 2026-11-14 |
| Verification date | 2026-11-19 |
| Primary manual | [docs/manual.md](https://github.com/bol-van/zapret2/blob/3bae3d26d741a8f6e0bc4ea651656b4e1a438375/docs/manual.md), blob `f0f21ecd4b9f5fa7039561a056cbd3e9a768fe19` |
| Supplementary manual | [docs/manual.en.md](https://github.com/bol-van/zapret2/blob/3bae3d26d741a8f6e0bc4ea651656b4e1a438375/docs/manual.en.md) |
| `install_easy.sh` | blob `a6b28f187410c076c5f41de67540c50b1fdbccb5`, 834 lines |
| `blockcheck2.sh` | blob `38905b593c53d2613ef8c7d4a1a8b592631bd9dc`, 2107 lines |
| `uninstall_easy.sh` (checked to understand the matching lifecycle) | blob available in the upstream tree at the same commit |

The project integration skill establishes that the Russian manual is primary, the English manual supplementary, and the implementation authoritative where documentation is incomplete. No material Russian/English discrepancy was found in the areas summarized below. The upstream baseline must be re-verified before implementation or release; `master` is not an immutable dependency reference.

## 2. `install_easy.sh`

Primary source: [install_easy.sh at audited commit](https://github.com/bol-van/zapret2/blob/3bae3d26d741a8f6e0bc4ea651656b4e1a438375/install_easy.sh).

### 2.1 Invocation and execution model

- POSIX shell script; the documented command is `sudo /opt/zapret2/install_easy.sh`.
- Determines its own directory and uses it as the source tree.
- Default target is `/opt/zapret2` (`ZAPRET_TARGET` can override it).
- Loads `config` (creates it from `config.default` if missing) and sources upstream `common/*) libraries.
- Detects the init system. The script explicitly supports Linux and identifies systemd, OpenRC, OpenWrt, or generic Linux. On generic/unsupported Linux it cannot configure autostart and tells the operator that manual setup is required.
- Requires root. The upstream elevation helper re-executes through `sudo -E` or `su` when needed.
- Checks required source files for the detected init system.
- Checks binaries; if compatible prebuilt binaries are absent, it can attempt a local build when the Makefile and build tools are available.
- This is an interactive installer. It asks questions about replacing an existing installation, retaining user data, firewall type, IPv6, offloading, mode/filter, options, interfaces, and lists. The inspected script does not expose a documented stable non-interactive CLI/API for a GUI to call.

### 2.2 Installation location and replacement

- Easy installation is normalized to `/opt/zapret2`.
- If launched from another directory, the script asks permission to copy itself to the target and relaunch there.
- If the target already exists and is not the same directory, it asks permission to replace the target directory.
- Replacing the target removes the existing tree. It offers to preserve selected state: `config`, custom scripts, and user list files. The implementation backs up/restores a defined list of paths, not an arbitrary complete installation snapshot.
- It creates default user list files/directories when absent and fixes ownership, directory/file permissions, and executable permissions for scripts and binaries.

**Implication:** an application must never silently invoke this operation against an existing tree. The replace/preserve decision is destructive and must remain explicit. Do not implement a second replacement/backup engine in zapret2control.

### 2.3 systemd path and side effects

For the systemd branch, the observed order is:

1. Check binaries.
2. Require root and check whether the systemd unit directory is writable.
3. Normalize/copy the installation location.
4. Check DNS and virtualization.
5. Stop and disable the existing `zapret2` service.
6. Select firewall type and check/install prerequisites.
7. Install/select binaries and IPv6/offloading/configuration options.
8. Install the `zapret2.service` unit and enable it.
9. Download configured lists when enabled.
10. Remove legacy cron entries for list updates.
11. Install, enable, and start `zapret2-list-update.timer`.
12. Start the `zapret2` service.

The systemd unit runs `/opt/zapret2/init.d/sysv/zapret2 start` and the corresponding stop command. The list-update timer triggers `zapret2-list-update.service`, which runs `/opt/zapret2/ipset/get_config.sh`.

**Important correction to earlier assumptions:** successful systemd installation does not leave zapret2 stopped or disabled. The upstream script enables and starts the service, and enables and starts the list-update timer.

The installer can also install OS prerequisites using the detected package manager. On Debian-family systems the inspected helper may run `apt-get update` and install required packages; other package managers have their own branches. These package-manager changes must be disclosed to the user.

### 2.4 Installer integration decision

- Treat `install_easy.sh` as the authority for upstream installation behavior.
- Do not duplicate its service, firewall, dependency, config, list, or permission setup.
- Because it is interactive and has no documented stable GUI API, the first implementation should launch it in a real terminal session with the documented command and preserve its prompts/output. Do not pipe guessed answers into it.
- Before launch, verify that the complete upstream release tree is staged at `/opt/zapret2`, with source revision and integrity recorded. Do not assume that a clean system already has this directory or script.
- Present the replacement warning and state-preservation implications before launching when an existing installation is detected.
- After completion, independently inspect systemd service/timer state and installation status; do not infer success from process exit alone.
- The app may provide a guided front end later only if it can pass validated answers through a supported upstream interface. Do not patch upstream shell internals or rely on undocumented prompt ordering.

## 3. Matching uninstall behavior

Although not one of the two requested scripts, the matching upstream `uninstall_easy.sh` was inspected because install/remove must be designed as one lifecycle.

- Requires root and dispatches by detected init system.
- On systemd it clears managed ipset state, stops/removes the service unit, removes the list-update timer/service, deletes zapret nftables table state, and removes matching cron entries.
- It does **not** delete `/opt/zapret2` itself.
- The manual explicitly says the installation directory must be deleted manually for complete removal.
- On generic Linux, the script warns that uninstall is incomplete and autostart must be removed manually.
- On OpenWrt it has additional firewall/hotplug/dependency cleanup; this is outside the Linux desktop target unless explicitly added later.

**Implication:** UI must distinguish “run upstream uninstaller” from “delete remaining installation directory/data”. Do not silently remove user configuration or lists. Report residual files and let the user explicitly choose whether to delete them.

## 4. `blockcheck2.sh`

Primary source: [blockcheck2.sh at audited commit](https://github.com/bol-van/zapret2/blob/3bae3d26d741a8f6e0bc4ea651656b4e1a438375/blockcheck2.sh). Primary manual section: [manual.md — blockcheck2](https://github.com/bol-van/zapret2/blob/3bae3d26d741a8f6e0bc4ea651656b4e1a438375/docs/manual.md#blockcheck2).

### 4.1 Purpose and test model

Upstream defines blockcheck2 as a POSIX shell tool for automating DPI strategy tests. It is modular: test suites live in subdirectories of `blockcheck2.d`; the selected suite is composed of scripts that are sourced and executed by the runner.

The default suite is `standard`. The current tree includes HTTP/basic, miscellaneous, OOB, multi, sequence overlap, synthetic data, fake/faked/hostfake families, combinations, and QUIC test scripts. A `custom` suite tests strategy lists from files.

The tool is a diagnostic/research tool, not an automatic strategy oracle. The manual explicitly says results require interpretation and do not guarantee that a strategy will work in a browser or across all resources.

### 4.2 Inputs and configuration

- Loads the zapret2 `config` if present; otherwise copies `config.default`.
- Uses the upstream `common` shell libraries and binaries from the same source tree.
- Default target is `rutracker.org`; default test suite is `standard`.
- Interactive mode asks for the test suite, domains/URIs, IP protocol version(s), HTTP/TLS 1.2/TLS 1.3/HTTP3 test selection, repeat count, parallel mode, and scan level.
- Batch mode is selected by `BATCH=1`; relevant settings are passed as environment variables. The manual example is `BATCH=1 DOMAINS=bbc.com CURL_CMD=1 SKIP_DNSCHECK=1 /opt/zapret2/blockcheck2.sh`.
- Protocol tests are performed with curl: HTTP, HTTPS/TLS 1.2, HTTPS/TLS 1.3, and HTTP/3 (QUIC). TLS 1.3 and HTTP/3 are enabled only if the selected curl build supports them.
- curl must support `--connect-to` (manual says at least curl 7.49). A custom curl binary can be selected through `CURL`.
- Main tuning inputs include `TEST`, `DOMAINS`, `IPVS`, `ENABLE_HTTP`, `ENABLE_HTTPS_TLS12`, `ENABLE_HTTPS_TLS13`, `ENABLE_HTTP3`, `REPEATS`, `PARALLEL`, `SCANLEVEL`, `SKIP_DNSCHECK`, `SKIP_IPBLOCK`, `SKIP_PKTWS`, `SECURE_DNS`, curl options, and standard-suite-specific variables. Do not expose every upstream shell variable as a UI control; begin with a small, documented subset and keep advanced controls separate.

### 4.3 Privileges, preconditions, and system effects

- On Linux, strategy tests use `nfqws2` and the selected firewall backend (`iptables` or `nftables`).
- Root is required unless `SKIP_PKTWS=1`; the normal strategy-testing path therefore requires elevated privileges.
- It checks for already-running DPI-bypass processes and warns that they must be stopped. It does not automatically stop them and does not make the warning a hard precondition.
- The manual says to run blockcheck with zapret and other bypass software disabled and without VPN.
- For each test it resolves target IPs, creates temporary packet-redirection/firewall rules, starts a temporary `nfqws2` process with a test queue/mark and Lua libraries, runs curl probes, terminates the test process, and removes temporary rules.
- Linux firewall integration creates temporary iptables chains/rules or a temporary nftables table named with the current process ID. The script also sweeps stale artifacts left by terminated/crashed runs.
- Signal handlers perform cleanup on normal interruption/termination paths. SIGKILL or a crash can bypass traps; stale-state sweeping is a recovery measure, not a guarantee that every possible external firewall mutation is restored.
- DNS checks can compare system resolver results with public DNS, detect likely spoofing/interception, and automatically switch to DoH. The selected DoH path and external DNS checks are part of the diagnostic behavior.
- The script clears `ALL_PROXY` from its environment so proxy configuration does not silently change the tests.
- IP/port block diagnostics are explicitly for manual interpretation. The script checks connectivity using suitable netcat/ncat where available, but does not automatically decide every IP-block case.

**Safety implication:** blockcheck2 is not a harmless read-only scan. It temporarily changes packet-filter rules and starts packet-processing processes with elevated privileges. The application must clearly explain this, check whether the normal zapret service or other bypass tools are active, require explicit confirmation, stream logs, and always surface interruption/cleanup failures. Do not run it invisibly in the background.

### 4.4 Output and result handling

- The script emits human-readable progress and a textual summary to stdout/stderr.
- Successful strategies are reported as text; for multiple domains it also prints COMMON intersection and COVERAGE summaries.
- The script does not expose a documented JSON or other stable machine-readable result protocol.
- Strategy lines are shell-oriented command-line fragments; the manual notes that output parameters are not escaped.
- A GUI must preserve the complete raw log. Any parser for summary/strategy lines must be treated as version-specific, tested against fixtures, and non-authoritative. Never evaluate output as shell code, and never silently apply a discovered strategy to the live config.

### 4.5 blockcheck integration decision

- Use the upstream script and its complete matching `blockcheck2.d`, `common`, Lua, binary, and configuration assets; do not reimplement its test families in C++.
- Run it as a supervised external process, capture stdout/stderr separately, support cancellation, and show its actual exit status.
- Use a PTY/terminal-compatible execution mode for upstream interactive mode. Alternatively use `BATCH=1` only when the app explicitly supplies and validates all required environment inputs.
- Do not build an elaborate domain-specific parser before proving that the raw upstream workflow is usable.
- The initial GUI should support a safe basic workflow (target(s), IP version, protocols, scan level, repeats) and an advanced section for supported upstream variables. Do not invent flags or environment variables.
- Do not assume the script's cleanup succeeded merely because the child process exited. Verify that the process ended and inspect temporary firewall artifacts where possible; present cleanup failures clearly.
- Do not automatically stop a running zapret service without the user's explicit consent and a defined restore policy.
- Do not treat a successful curl probe as proof of browser-wide or permanent bypass.

## 5. Product architecture consequences

The new application should remain a thin, understandable Linux desktop control surface around upstream zapret2 and Linux facilities.

### Installation lifecycle
- Stage and verify an exact upstream release.
- Invoke upstream `install_easy.sh` in an interactive terminal.
- Verify resulting service, timer, files, and configuration.
- Invoke upstream `uninstall_easy.sh` for upstream-managed removal.
- Handle residual installation-directory/data removal as a separate, explicit action.

### Diagnostics lifecycle
- Launch upstream `blockcheck2.sh` with a controlled environment.
- Use the upstream test suite assets from the same pinned revision.
- Show live output and cancellation.
- Keep results as diagnostics; require explicit user action before changing active configuration.

### Non-goals
- No custom replacement installer/uninstaller.
- No second strategy-testing engine.
- No shell `eval` of GUI-provided or script-produced strings.
- No hidden firewall modifications.
- No auto-application of strategy recommendations.
- No Windows backend, Windows packaging, or Windows CI gates.

## 6. Follow-up verification required before implementation

1. Select and record the actual upstream release/tag and exact commit to integrate. The audited `master` commit is a documentation/source review snapshot, not automatically the product's selected release.
2. Verify release archive layout, checksums/signatures (if available), and staging behavior for a clean installation where `/opt/zapret2` does not yet exist.
3. Run the installer in disposable Debian 13 and Fedora environments and record exact filesystem, systemd, package-manager, firewall, timer, and service side effects.
4. Run the uninstaller in the same disposable environments; verify what remains and whether user config/lists are preserved.
5. Run blockcheck2 in a disposable VM with nftables and iptables backends, test normal exit/cancel/forced termination, and verify firewall cleanup.
6. Compare both manuals again when pinning the actual release. Record any differences against the Russian primary manual.
7. Add integration tests around the observed external-process contracts, not a duplicate implementation of upstream internals.
