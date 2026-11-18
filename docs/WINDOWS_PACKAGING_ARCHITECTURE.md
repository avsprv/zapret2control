# Windows Packaging and Runtime Architecture

**Status:** Architecture baseline; implementation specification  
**Target:** Windows 11 x64  
**Installation scope:** Per-machine only  
**Runtime staging:** KDE Craft  
**Installer:** NSIS  
**Implementation status:** Not implemented. Windows Packaging Gate remains **Blocked**.

This document consolidates the approved Windows design constraints for repository implementation. It is not evidence that a Windows build, backend, supervisor, installer, signing pipeline, or WinDivert runtime exists. Every upstream behavior and dependency must be revalidated against the exact immutable upstream revision and binaries selected for the product.

## 1. Product and build principles

The Windows package must preserve the declared application feature scope. Platform-specific implementation may differ, but a package must not silently omit features or substitute no-op implementations.

The build and distribution pipeline has three independent layers:

1. **KDE Craft:** build the Qt 6/Kirigami application and resolve/stage Qt, QML, Qt Quick Controls, KDE Frameworks, icon/theme, and third-party runtime dependencies.
2. **NSIS:** consume a deterministic, validated staging directory and produce the final installer EXE. The NSIS script, compiler version, options, resources, and manifest are version-controlled inputs.
3. **Distribution:** publish immutable versioned artifacts and, when separately authorized, publish WinGet manifests for the exact tested NSIS artifact.

A Craft deployment directory is not the final installer. `windeployqt` may supplement deployment but does not replace Craft's KDE dependency closure or third-party runtime verification.

Before NSIS runs, validate the executable, QML modules, Qt/platform plugins, Kirigami, QQC2 desktop style, KDE Frameworks, icons/themes, third-party runtime dependencies, architecture, build configuration, and absence of build-machine-only files. Generate a deterministic staging manifest with SHA-256 hashes. NSIS must fail on missing or unexpected required inputs.

## 2. Runtime and privilege boundary

### 2.1 Unelevated GUI

The Kirigami GUI runs as the interactive user (`asInvoker`). It owns UI, ordinary settings, diagnostics presentation, update policy, and requests to platform adapters. It must never be elevated merely because network interception requires administrator privileges.

### 2.2 Windows engine model

The inspected upstream `zapret2` Windows implementation uses `winws2` as a user-mode process and WinDivert for packet interception. WinDivert is opened by an elevated process. The upstream Windows implementation does not establish a requirement for a persistent zapret2 Windows service.

The first target is therefore a per-interactive-session, narrowly scoped elevated bootstrap and reduced-privilege supervisor worker, not a permanent Windows service. Consider Task Scheduler only if unattended startup becomes an explicit product requirement. Introduce a Windows service only if a concrete requirement cannot be met safely by the process/task model.

The privileged boundary must expose only an allowlisted operation set. It must validate every request, profile, path, executable identity, and argument; use restrictive ACLs; avoid shell construction; and never expose generic process launch, arbitrary filesystem access, command execution, or environment mutation.

### 2.3 Upstream behavior reference

The reviewed reference is upstream `bol-van/zapret2` v1.0.7, commit `038c97e5819df10f8e8c22a2f1fffe57f2f7379b` (reviewed 2026-11-17). This is a research reference, not an approved application pin. Before implementation, record the exact upstream commit actually integrated and repeat the compatibility review against that immutable revision.

At the reviewed revision:

- `winws2` uses WinDivert; the driver is installed on demand when `WinDivertOpen()` is called by an elevated process.
- Standalone execution requires the matching `cygwin1.dll` beside the executable. When launched inside a Cygwin environment, that DLL must not be beside the executable.
- Cygwin signal delivery is limited to processes using the same `cygwin1.dll` instance. Do not assume a signal sent by an unrelated Cygwin installation will stop the engine.
- After WinDivert initialization, upstream lowers the engine's integrity level, removes all token privileges except `SeChangeNotifyPrivilege`, and applies its own Job Object restrictions. This does not make the process an ordinary unelevated process and does not replace the application's separate privilege boundary.
- The engine closes its WinDivert handle during normal cleanup.

Do not infer service, driver-installation, signal, or readiness semantics beyond the exact pinned source and tested binaries.

## 3. Proposed supervisor lifecycle

The supervisor design is a prototype candidate, not an implementation approval.

### 3.1 Start

1. The GUI validates the selected profile and resolves all inputs inside approved roots.
2. The GUI creates a one-instance local Named Pipe endpoint using a random per-launch suffix and `FILE_FLAG_FIRST_PIPE_INSTANCE`; failure is fatal.
3. The GUI launches only the fixed, machine-installed bootstrap executable using the UAC `runas` verb. The only command-line argument is the generated pipe endpoint identifier.
4. The bootstrap authenticates the GUI peer, verifies its own signature/location, verifies the installed engine/runtime manifest, and validates the profile UUID, revision, digest, schema, and referenced files.
5. A per-user/per-session singleton prevents duplicate application-controlled engines. It must not block unrelated WinDivert consumers.
6. The bootstrap creates an outer containment Job Object, starts the exact machine-installed `winws2.exe` suspended with a controlled environment and working directory, assigns it to the job, and resumes it. Do not inherit unrelated privileged handles.
7. The engine initializes WinDivert and applies its upstream sandbox. The supervisor reports `Running` only after a pinned, tested readiness handshake succeeds. PID existence or a stale log is not readiness. Do not parse human-readable logs as a stable API without pinning and testing the parser against the exact upstream revision.
8. The bootstrap creates a reduced-privilege worker, transfers only the required pipe/process/job handles, waits for worker readiness, closes its own handle copies, and exits.

UAC cancellation must return safely to `Stopped` without leaving persistent state or partial processes.

### 3.2 Stop and crash behavior

The GUI sends `Stop` over authenticated IPC. The worker requests graceful shutdown through a mechanism proven against the exact Cygwin/`winws2` build (for example, a dedicated console control event or a reviewed upstream stop-event integration). Do not assume an undocumented POSIX signal mapping.

Wait a bounded interval and observe the engine exit. If graceful shutdown fails, a separately reviewed emergency termination path may use retained process/job handles and must be reported as forced termination. Report `Stopped` only after process exit and WinDivert handle release are established, or after forced termination and exit confirmation.

The initial policy is to stop the engine when the controlling GUI/session exits. Background/logon operation is a separate feature and needs explicit user policy and tests.

The proposed outer Job Object uses kill-on-close so worker failure cannot leave an unmanaged engine. Upstream creates its own inner Job Object. Nested Job Object support, assignment order, UI restrictions, process limits, and kill-on-close behavior must be proven on every supported Windows build. The outer job must not apply UI restrictions if that prevents nesting. If this design conflicts with upstream's inner job, redesign crash containment; do not silently remove it.

### 3.3 Lifecycle states

The Windows adapter must distinguish at least:

- `Stopped`: no controlled engine process is running;
- `ElevationPending`: UAC is outstanding;
- `Starting`: bootstrap/worker/engine launch is in progress;
- `Initializing`: engine exists but WinDivert readiness is unconfirmed;
- `Running`: pinned readiness handshake succeeded and process is alive;
- `Stopping`: graceful stop or bounded forced-stop fallback is in progress;
- `Failed`: startup/runtime failed with diagnostic details;
- `Unknown`: supervisor/IPC unavailable or state cannot be verified.

Transitions are event-driven and timeout-bounded. A lost pipe or worker is `Unknown`, never `Running`.

## 4. Named Pipe IPC security contract (prototype required)

Use a local Windows Named Pipe only. No TCP listener, remote pipe, generic RPC endpoint, or command-shell interface.

The GUI creates the server endpoint before elevation, with `PIPE_REJECT_REMOTE_CLIENTS`, an explicit security descriptor, a one-instance pipe, and a DACL restricted to the initiating user's SID, logon SID, and the required Administrators access. Do not rely on the default pipe DACL (which may grant access to Everyone and Anonymous). Independently verify peer SID and session ID; ACLs alone are insufficient.

Authentication is directional:

- The elevated bootstrap is the pipe client. It validates the GUI pipe server using `GetNamedPipeServerProcessId` and `GetNamedPipeServerSessionId`, then inspects the server process/token/image. The bootstrap must not impersonate the GUI server with `ImpersonateNamedPipeClient`.
- The GUI is the pipe server. It validates the bootstrap client using `GetNamedPipeClientProcessId` and `GetNamedPipeClientSessionId`, then inspects process/token/image. If client impersonation is needed for an access check, always call `RevertToSelf` on every path.
- After worker handoff, do not assume the pipe APIs report the worker PID merely because a handle was inherited. Establish the actual reported PID. The bootstrap supplies worker PID and process creation time; the GUI independently opens and validates that process, creation time, image path/signature, token policy, session, and lifetime. Account for PID reuse and over-the-shoulder UAC credentials; do not require worker SID equality with the GUI SID when a different administrator account was used.

### 4.1 Version 1 frame

Use a fixed binary frame, not JSON, a shell string, or loosely parsed command line:

| Field | Size | Rule |
|---|---:|---|
| Magic | 4 bytes | ASCII `Z2CP` |
| Major | 2 bytes | Little-endian; v1 = 1 |
| Minor | 2 bytes | Little-endian; v1 = 0 |
| Message type | 2 bytes | Fixed enumerated value |
| Flags | 2 bytes | Must be zero |
| Payload length | 4 bytes | Little-endian; maximum 4096 |
| Request ID | 8 bytes | Nonzero; echoed in response |

Header size is 24 bytes. Read complete headers/payloads; do not assume one pipe read equals one frame. Reject truncation, oversized payloads before allocation, unknown message types, nonzero reserved flags, zero/replayed request IDs, and unsupported major versions. The shared protocol header must define all response payload layouts and stable result/state code registries before either endpoint is implemented.

Fixed v1 message types:

| Type | Value | Payload / meaning |
|---|---:|---|
| `BootstrapStart` | `0x0001` | Profile UUID (16 bytes, RFC 4122 order), config revision (u64 LE), SHA-256 digest (32 raw bytes) |
| `BootstrapStartResponse` | `0x8001` | Stable result/state plus worker PID and process creation time |
| `QueryStatus` | `0x0002` | Empty |
| `QueryStatusResponse` | `0x8002` | Fixed status/result payload |
| `Stop` | `0x0003` | Empty |
| `StopResponse` | `0x8003` | Fixed stop result/status payload |
| `Ping` | `0x0004` | Empty |
| `PingResponse` | `0x8004` | Fixed responsiveness/state payload |

The worker accepts only `QueryStatus`, `Ping`, and `Stop`; it cannot start processes or mutate configuration. `Stop` is terminal for that worker. A new activation requires a new UAC bootstrap transaction and a new pipe. Process requests serially; reject duplicate IDs and duplicate starts deterministically. A repeated Stop with a new ID is idempotent. The singleton must prevent concurrent bootstrap races.

`BootstrapStart` must never carry an executable path, raw command line, working directory, environment variable, DLL path, Lua source, or arbitrary filesystem path. The bootstrap resolves the UUID only within the authenticated user's approved data root, validates files and configuration, rejects reparse-point/path escapes, and builds an argument vector from an explicit allowlist. Treat all profile content as untrusted input.

### 4.2 Bootstrap/worker handoff

The bootstrap is the only process that temporarily holds the full elevated token. It creates the outer Job Object without UI restrictions, launches the engine suspended, assigns it to the job, then resumes it. It creates a restricted worker token, removes unnecessary privileges, marks the Administrators SID deny-only where supported, and selects a tested integrity level. Transfer only the authenticated pipe, engine process handle with minimum query/synchronize/termination rights, and outer Job handle with minimum query/terminate rights. Do not inherit token, installer, or unrelated file handles.

The worker confirms readiness, becomes the sole owner of the outer Job handle, and exposes only the narrow IPC methods. Its token, handle rights, pipe continuity, session identity, and behavior after bootstrap exit must be experimentally verified. Changing integrity level alone does not remove administrator groups or privileges.

## 5. WinDivert ownership and driver safety

Ship only a verified, pinned WinDivert DLL/driver pair as immutable installer-owned payload under Program Files. Let the exact pinned WinDivert library perform documented on-demand driver installation when `WinDivertOpen()` is called. Do not pre-register a second project-owned driver service unless the exact upstream implementation requires it and a reviewed design proves it safe.

Never run generic `sc stop`, `sc delete`, or equivalent cleanup against a WinDivert service during normal stop, upgrade, repair, or uninstall. Other applications may use it. Do not remove a driver image or service registration until the exact service image path and all consumers are known. If safe ownership cannot be established, preserve the referenced file and report residual machine state rather than breaking another application.

Preserve exact Win32 error codes for diagnostics, including signature/policy failures, access denied, missing runtime, blocked/incompatible driver, and conflicting software. Never disable Secure Boot, enable Test Signing, replace system drivers, disable Defender, add Defender exclusions, or ask users to restore quarantined files as a workaround.

**WinDivert security hold:** WinDivert 2.2.2 is not approved for bundling. A community report dated 2026-10-17 described Microsoft Defender quarantine with detection `Misleading:Win32/Malgent!AMTB`; no false-positive conclusion is established. Before reconsidering, retrieve exact official binary/source assets, calculate independent SHA-256 hashes, verify Authenticode chains on a clean Windows image, record Defender platform/intelligence versions and scan results, and use Microsoft's official security review process if detection reproduces. A valid signature alone does not prove safety.

## 6. Windows support and runtime baseline

Initial target: Windows 11 x64 only. Windows 10, earlier Windows, 32-bit Windows, ARM64, and Windows Server are not in the initial support commitment. Do not advertise ARM64 while its driver path requires Test Signing. At release, name tested Windows 11 editions and exact servicing/build baseline; refresh Microsoft's servicing information and do not support editions/builds outside their servicing window.

The reviewed upstream build path uses x64 Cygwin and LuaJIT. Treat `cygwin1.dll`, `winws2.exe`, `windivert.dll`, the architecture-specific driver, Lua/runtime resources, and every transitive dependency as explicit artifacts. Pin versions and hashes, inspect PE architecture/imports, verify signatures, and record tool versions. Do not bundle the whole Cygwin installation or build tools unless a concrete runtime dependency requires them. The Cygwin repository metadata/archive hashes were not locked in the reviewed research; the toolchain is not production-approved.

The reviewed Lua modules contained no FFI usage at that upstream revision. Keep the upstream Windows build's `LUAJIT_DISABLE_FFI` candidate until a complete Lua/runtime test proves compatibility. Future FFI use triggers a separate review.

Licensing/provenance requirements include: Cygwin runtime LGPL-3.0-or-later with linking exception and exact corresponding-source obligations; per-package license audit for any shipped Cygwin runtime files; LuaJIT MIT plus Lua-origin and dlmalloc notices; WinDivert's selected LGPL-3.0 or GPL-2.0 terms; and inspection of GCC runtime components/static link output. Generate a file-level SBOM/manifest, SPDX identifiers, full notices, source URLs/hashes, modification records, PE import/link reports, driver signature evidence, Defender scan evidence, and a reviewed source-availability plan. This is not legal advice; unresolved redistribution obligations block release.

## 7. Installation, data, update, and removal

Only **per-machine** installation is supported, under `%ProgramFiles%\zapret2control\`. Per-user and portable modes are out of scope.

| Data | Location | Ownership |
|---|---|---|
| Immutable binaries/runtime | `%ProgramFiles%\zapret2control\` | Installer-owned; standard users cannot modify privileged files |
| Per-user settings | `%APPDATA%\zapret2control\` | Current user |
| Per-user logs/transient diagnostics | `%LOCALAPPDATA%\zapret2control\` | Current user; documented retention |
| Machine-wide state, only if justified | `%ProgramData%\zapret2control\` | Restrictive ACL; minimum access |

Do not store mutable user data in Program Files. Do not create machine-wide state without a concrete need and documented ACLs.

NSIS must support fresh install, in-place upgrade, repair, silent install/uninstall, UAC decline, interrupted operations, and safe recovery. It must coordinate only application-owned processes and never terminate unrelated processes. Elevation is required for machine-wide install/update/removal; the GUI remains unelevated. Declined UAC must not leave a partial install.

Upgrade is transactional: identify installation/version/owner; validate signature and payload; check prerequisites and space; stage new files; stop only owned components; activate recoverably; migrate versioned configuration without data loss; verify the new runtime; commit only after success; and preserve rollback/recovery state. Test forced interruption and reboot/power-loss-like scenarios.

Uninstall removes only owned files/registration. Preserve user data by default. Never remove shared dependencies or WinDivert resources still referenced by another consumer. If the driver image remains referenced, preserve the file and containing directory and report incomplete removal; later generic cleanup must not delete it.

Support two update ownership modes: standalone (project updater may manage it if implemented) and WinGet-managed (WinGet is update authority; application updater must not replace package-managed files). Detect ownership with tamper-resistant evidence, not a user-writable marker alone. If ambiguous, block destructive updates. WinGet presence alone does not mean this installation is WinGet-managed.

Sign public stable installers and applicable executables/DLLs. Verify Authenticode after signing, calculate SHA-256 from the final artifact, publish immutable versioned URLs, and retain provenance linking source SHA, upstream revision, toolchain, Craft blueprint/cache package revisions, staging manifest, and NSIS inputs. WinGet manifests must use the currently validated schema, exact tested silent switches, correct architecture/scope, immutable URL, hash, upgrade and uninstall behavior. WinGet publication requires separate authorization.

## 8. Required prototype and release verification

Use a disposable, restorable Windows 11 x64 VM, not a daily-use workstation, for driver, crash, forced-termination, and security experiments. Start with a synthetic child process; integrate pinned `winws2` only after the pipe, identity, token, and Job containment layers pass.

Mandatory supervisor prototype coverage:

- UAC approval/cancellation and standard-user launch with over-the-shoulder administrator credentials;
- Named Pipe first-instance behavior, DACL/logon-SID isolation, remote rejection, directional PID/SID/session/token/image checks, and pipe-squatting rejection;
- malformed, truncated, oversized, unknown, replayed, version-mismatched frames; duplicate/concurrent starts; idempotent stop;
- worker handoff identity, PID reuse/race handling, inherited-handle list, restricted-token properties, and worker operation after bootstrap exits;
- profile UUID/revision/digest validation, hostile config, reparse-point/path traversal rejection, and proof no request can choose an executable or inject arbitrary arguments/environment;
- failure injection at every bootstrap/worker/job/process/handle-transfer stage with no unmanaged engine or stale singleton;
- nested Job Object behavior and kill-on-close containment on every supported Windows build;
- exact pinned engine/runtime launch, WinDivert on-demand load, readiness handshake, engine sandbox, graceful stop, forced-stop reporting, and handle release;
- GUI/worker crash, logoff, second user session, upgrade, repair, uninstall, and preservation of unrelated WinDivert consumers;
- missing DLL/driver, invalid signature, driver load failure, antivirus conflict, and network-interface changes with actionable diagnostics.

Retain machine-readable results and human-readable evidence: test ID, expected/actual result, Windows build, account/session topology, source commit, binary SHA-256, driver version/signature/image path, relevant API return and GetLastError values, process IDs/creation times, token groups/privileges/integrity levels, Job membership/limits, pipe peer identity, exit codes, and sanitized logs. Never store credentials, access tokens, signing secrets, or user configuration secrets. A failed or inconclusive security test blocks the next stage.

Installer matrix must additionally cover clean install, upgrade, interrupted upgrade, repair, silent install/uninstall with exact switches, UAC declined, ownership ambiguity, user-data preservation, clean environment without development tools, signatures, and checksums. CI packaging success does not replace clean-VM evidence.

## 9. Release blockers and open decisions

The following remain unresolved and must not be silently assumed:

- exact Windows 11 editions and minimum serviced build/UBR; 25H2 and 26H2 are proposed test candidates, not validated support claims;
- exact upstream zapret2 commit and verified Windows engine behavior;
- supervisor/IPC implementation, over-the-shoulder UAC identity, restricted-token policy, Job nesting, and graceful-stop mechanism;
- exact ACLs and any justified ProgramData state;
- atomic NSIS upgrade/rollback strategy and ownership detection;
- code-signing certificate, timestamping, and secret custody;
- tested NSIS silent switches and WinGet manifest identity/schema;
- Windows CI and clean-VM infrastructure;
- WinDivert version, provenance, hashes, Authenticode, Defender investigation, and safe shared-driver cleanup;
- exact Cygwin/LuaJIT/WinDivert runtime manifest and all license/source obligations.

Windows CI must eventually run on a real Windows 11 x64 runner using the actual shared Qt 6/Kirigami target, Windows adapter, QML lint, shared/Windows tests, Craft staging, manifest validation, and NSIS packaging. It must not compile a stub or mark Linux-only tests as passed. Main CI, Debian 13 DEB, Fedora RPM, and Windows CI must all verify the same exact source SHA. Linux CI, systemd E2E, DEB/RPM and install/remove smoke tests remain mandatory throughout.

Until implementation, security prototype, dependency/licensing review, CI, and clean-VM acceptance are complete, Windows Packaging Gate remains **Blocked**. No Windows support or installer availability may be claimed.

## 10. Normative references

- KDE Craft for Windows: https://develop.kde.org/docs/getting-started/building/craft/craft-windows/
- KDE Windows packaging: https://develop.kde.org/docs/packaging/windows/packaging/
- Upstream primary manual at reviewed reference commit: https://github.com/bol-van/zapret2/blob/038c97e5819df10f8e8c22a2f1fffe57f2f7379b/docs/manual.md
- Upstream supplementary English manual at the same reference commit: https://github.com/bol-van/zapret2/blob/038c97e5819df10f8e8c22a2f1fffe57f2f7379b/docs/manual.en.md
- Upstream Windows build instructions (moving branch; re-pin before use): https://github.com/bol-van/zapret2/blob/master/docs/compile/build_howto_windows.txt
- WinDivert documentation: https://github.com/basil00/WinDivert/wiki/WinDivert-Documentation
- Microsoft Windows servicing: https://learn.microsoft.com/en-us/windows/release-health/
- Microsoft distribution paths: https://learn.microsoft.com/en-us/windows/apps/package-and-deploy/choose-distribution-path
- WinGet manifest documentation: https://github.com/microsoft/winget-pkgs/tree/master/doc/manifest
