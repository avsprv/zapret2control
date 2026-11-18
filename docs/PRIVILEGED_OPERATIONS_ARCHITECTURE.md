# Privileged Operations Architecture

## Status

- Decision: proposed shared application/platform contract.
- Scope: replace direct `Helper::start(QProcess *, QStringList)` calls with typed domain operations.
- Linux implementation: existing allowlisted helper and Polkit boundary, adapted behind typed interfaces.
- Windows implementation: not implemented by this document. The Windows supervisor remains a separate, narrowly scoped engine-lifecycle boundary and must follow `docs/WINDOWS_PACKAGING_ARCHITECTURE.md`.
- Security rule: no shared API may accept an executable path, shell command, arbitrary argument vector, environment override, or arbitrary filesystem path for privileged execution.

This document records the interface design before implementation. It does not claim Windows support or approve the Windows supervisor prototype.

## 1. Audit of the current call sites

The current Linux implementation starts the fixed `zapret2control-helper` through `pkexec`. The helper has a command allowlist and validates command-specific arguments. Eight production source files call the helper directly:

| Caller | Existing helper operation | Data / behavior |
|---|---|---|
| `ConfigController` | `config-read` | Read the installed upstream configuration. |
| `ConfigController` | `config-write EXPECTED_SHA256` | Write configuration bytes on stdin; helper verifies the previous-file digest, validates shell syntax, creates a backup and atomically replaces the file. |
| `ConfigController` | `config-rollback` | Restore the latest managed backup. |
| `ProfileController` | `config-read`, `config-write EXPECTED_SHA256` | Shares the same configuration store; profile editing itself is unprivileged domain logic. |
| `ListsController` | `lists-read NAME` | Read one allowlisted list identifier. |
| `ListsController` | `lists-write NAME` | Write validated bytes on stdin to one allowlisted source list; helper owns backup and atomic-write behavior. |
| `LogsController` | `logs-read LINE_COUNT` | Read the system journal; line count is restricted to 50, 100, 200 or 500. |
| `SystemdBackend` | `unit ACTION` | Privileged start, stop, enable or disable of the fixed `zapret2.service` unit. State queries use unprivileged `systemctl`. |
| `BlockcheckRunner` | `blockcheck OPTIONS...` | Run the legacy foreground diagnostic with validated domains, IP versions, protocol flags, repeat count and scan level; stream output. |
| `BlockcheckRunner` | `blockcheck-cancel` | Request cancellation of the currently owned legacy diagnostic. |
| `BlockcheckJobController` | `blockcheck-submit OPTIONS...` | Submit a validated diagnostic job; returns an operation UUID. |
| `BlockcheckJobController` | `blockcheck-recovery-probe` | Read recovery state. |
| `BlockcheckJobController` | `blockcheck-job-status UUID` | Read status for the exact operation UUID. |
| `BlockcheckJobController` | `blockcheck-job-logs COUNT START_EPOCH` | Read bounded journal output for the specified time window. |
| `BlockcheckJobController` | `blockcheck-job-cancel UUID` | Cancel only the operation identified by the UUID. |
| `BlockcheckJobController` | `blockcheck-recover` | Retry the guarded recovery operation. |
| `ZapretUpdater` | `install VERSION`, `update VERSION` | Run the separately governed upstream installation/update operation; stream bounded progress output. |

The helper itself remains the authoritative Linux allowlist. The new C++ API must not merely move its string command names into another generic wrapper.

## 2. Required domain interfaces

Use explicit interfaces in `src/platform/` (or a dedicated platform-operations subdirectory). The composition root creates the platform implementations and injects them into consumers. Shared controllers and domain services depend on interfaces only.

### 2.1 Configuration operations

A `ConfigurationOperations` interface exposes only:

- `readConfiguration()`
- `writeConfiguration(bytes, expectedSha256)`
- `rollbackConfiguration()`

The write request carries the complete bounded content and the digest of the version the user edited. The Linux adapter sends content through stdin and preserves helper-side syntax validation, digest comparison, backup and atomic replacement. The Windows adapter must use the approved per-user configuration location and safe file replacement; it must not elevate the whole GUI.

Both ConfigController and ProfileController use this same interface. There must not be two competing configuration implementations.

### 2.2 Host-list operations

A `HostListOperations` interface exposes:

- `readList(HostListId)`
- `writeList(HostListId, bytes)`

`HostListId` is a closed enum representing the exact supported list names. Do not accept a free-form path or filename at the platform boundary. The adapter maps each enum value to its platform-owned storage. Linux retains the helper's source-list allowlist and write restrictions.

### 2.3 System-log operations

A `SystemLogOperations` interface exposes `readRecentLogs(LogLineCount)`. `LogLineCount` is a validated closed set (50, 100, 200, 500), not an arbitrary integer. Platform-specific log sources and authorization are selected inside the adapter. The returned data is bounded and decoded as UTF-8 by the caller or a shared decoder.

### 2.4 Engine lifecycle operations

The existing backend lifecycle contract remains the application-facing abstraction for engine state, start, stop and boot policy. Linux's SystemdBackend may use a typed `ServiceControlOperations` adapter for the fixed zapret2 unit; it must not pass a caller-selected unit name or systemctl verb.

The Windows implementation must not emulate systemd or create a persistent Windows service by default. It must follow the documented per-session supervisor model. The elevated bootstrap is permitted to perform only the documented bootstrap transaction. The reduced-privilege worker may expose only the versioned `QueryStatus`, `Ping` and `Stop` protocol. A new start requires a new UAC bootstrap transaction. Boot-time behavior remains a separate capability and must be reported unavailable until a supported Windows design is implemented and verified.

### 2.5 Blockcheck operations

A `BlockcheckOperations` interface exposes named operations with typed payloads:

- `runDiagnostic(BlockcheckOptions)`
- `cancelDiagnostic()`
- `submitDiagnosticJob(BlockcheckOptions)`
- `queryDiagnosticJob(OperationUuid)`
- `readDiagnosticJobLogs(LogLineCount, StartEpoch)`
- `cancelDiagnosticJob(OperationUuid)`
- `probeDiagnosticRecovery()`
- `retryDiagnosticRecovery()`

`BlockcheckOptions` contains structured fields (domains, IP-version selection, protocol booleans, repeat count and scan level). Validation is shared where platform-independent and repeated at the privileged boundary where security requires it. UUIDs are parsed into a dedicated value type. The API must not expose the helper's CLI flags.

The legacy foreground runner and the systemd-managed job flow are distinct workflows; do not collapse them into one loosely typed method. Preserve operation correlation, stale-UUID protections, recovery markers, output limits and cancellation semantics.

### 2.6 Upstream maintenance operations

An `UpstreamMaintenanceOperations` interface exposes only:

- `installUpstreamVersion(ValidatedVersion)`
- `updateUpstreamVersion(ValidatedVersion)`

The version type is validated before dispatch and validated again by the platform boundary. Progress output is streamed through the common operation result channel. Application self-update remains a separate subsystem and must not be routed through this interface.

## 3. Asynchronous operation result contract

Platform methods return an owned asynchronous operation handle (QObject-based) with a deliberately small transport-neutral contract:

- `standardOutputReceived(QByteArray)` for bounded streaming output;
- `standardErrorReceived(QByteArray)` for bounded diagnostics;
- `finished(OperationResult)` exactly once.

`OperationResult` carries a stable status enum, an optional native/exit code and a bounded diagnostic code. Expected statuses include succeeded, failed, authorization denied/cancelled, timed out, unavailable and protocol error. Localized presentation strings are produced by the application layer from stable codes; adapters must not expose unbounded remote or helper-controlled strings as trusted UI text.

The handle does not expose `start(program, arguments)`, arbitrary stdin after dispatch, a generic `terminateProcess()`, or a raw process pointer. Requests that require stdin (configuration/list writes) include the bounded byte payload in their typed request. Cancellation is a domain operation with its own semantics, not a generic kill switch.

Every request has one terminal result. Implementations must define timeout, object lifetime, output limits and behavior when the caller is destroyed. Signals must not be emitted from a worker thread into UI objects without the appropriate Qt thread-affinity handling.

## 4. Security invariants

1. No API accepts arbitrary executable paths, shell strings, argument vectors, environment variables, unit names, or filesystem paths for privileged execution.
2. Identifiers are enums or validated value types with closed sets.
3. All privileged operations are explicit methods with operation-specific payloads.
4. Linux keeps the existing fixed helper path, Polkit authorization, command allowlist and helper-side validation.
5. Windows keeps the GUI unelevated. Do not introduce a general-purpose elevated command broker or a persistent high-integrity worker.
6. The Windows supervisor IPC remains limited to the operation set and fixed frame contract in `docs/WINDOWS_PACKAGING_ARCHITECTURE.md`; do not add configuration, update, filesystem or arbitrary process commands to that worker.
7. File writes remain bounded, validated, backup-aware where applicable, and atomic.
8. Operation UUID correlation and stale-request rejection remain mandatory for Blockcheck job control.
9. Authorization cancellation is represented distinctly from execution failure.
10. A platform capability is unavailable until its implementation and verification exist. Shared UI must not imply otherwise.

## 5. Migration sequence

1. Add shared request/value types and the asynchronous operation result contract.
2. Add typed interfaces and Linux adapters that preserve the existing helper behavior.
3. Inject interfaces through the platform composition root; remove direct `Helper::start` calls from QML controllers and backend domain services.
4. Add fake adapters and focused tests for each operation contract, including payload bounds, invalid identifiers, authorization cancellation, timeout, output truncation and exactly-once completion.
5. Verify Linux behavior against the existing helper integration and package smoke tests.
6. Implement Windows adapters only after the Windows runtime/upstream audit and supervisor prototype gates are satisfied. Do not use a generic privileged process runner as a shortcut.
7. Remove `Helper::start` from shared application code. Keep the Linux helper implementation private to the Linux adapter.

## 6. Acceptance criteria

- No production consumer outside the Linux platform adapter calls `Helper::start` or knows helper command names/flags.
- All operation methods are typed and closed over approved actions.
- Linux behavior, stdin payload handling, streaming output, authorization-denied behavior and recovery semantics remain unchanged.
- Unit tests cover each typed operation and invalid-input boundary.
- Main CI, Debian 13 DEB, Fedora RPM and Windows CI report on the same exact commit SHA before the integrated change is eligible for merge.
- Windows supervisor remains marked proposed/not implemented until its dedicated clean-VM prototype and security acceptance tests pass.
