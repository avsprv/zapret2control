# Upstream zapret2 Capability Matrix

**Status:** inventory not yet complete  
**Purpose:** enumerate every applicable, documented user-facing native zapret2 capability and prove how zapret2control exposes it.  
**Rule:** do not mark a capability Supported until implementation and verification evidence exist.

## 1. Compatibility baseline

| Field | Value |
|---|---|
| Canonical repository | https://github.com/bol-van/zapret2 |
| Selected upstream release/tag | Not selected |
| Full upstream commit SHA | Not selected |
| Russian manual revision | Not recorded |
| English manual revision | Not recorded |
| Verification date | Not recorded |
| Source/TLS/Git integrity checks | Not recorded |
| Signature status and trusted identity | Not recorded |
| Target OS/distributions/architectures | Linux; Debian 13 and Fedora initial targets |
| Notes | The previously audited commit is a review snapshot, not an approved production baseline. |

## 2. Status vocabulary

| Status | Meaning |
|---|---|
| Planned | Verified capability; integration not implemented. |
| Implemented | Code exists; required verification is incomplete. |
| Tested | Required tests passed in the stated environment. |
| Supported | Behavior, security, failure paths, cleanup, and required tests are complete. |
| Limited | Available with explicit documented restrictions. |
| Unsupported | Not available in the selected upstream baseline or intentionally outside approved product scope. |
| Blocked | An unresolved issue prevents implementation or verification. |
| Not assessed | Not yet investigated; must not be presented as supported. |

## 3. Capability inventory

The table below is a work queue, not a claim that all listed functions exist in every upstream version. Verify each row in both manuals and source at the selected baseline. Split rows into smaller capability IDs whenever one row hides multiple independently configurable or executable functions. Add rows for every additional upstream capability discovered.

| ID | Capability area | What must be investigated | Manual section / source | Native mechanism | zapret2control access path | Status |
|---|---|---|---|---|---|---|
| INST-01 | Installation | Clean installation, prerequisites, prompts, target paths, side effects | TBD | TBD | TBD | Not assessed |
| INST-02 | Reinstallation/replacement | Existing tree detection, overwrite and preservation behavior | TBD | TBD | TBD | Not assessed |
| INST-03 | Removal | Upstream-managed cleanup and residual files | TBD | TBD | TBD | Not assessed |
| SVC-01 | Service lifecycle | Start, stop, restart, status, enable/disable, failure state | TBD | TBD | TBD | Not assessed |
| SVC-02 | Scheduled list updates | Timer/service, schedule, state, manual trigger | TBD | TBD | TBD | Not assessed |
| UPD-01 | Upstream version discovery | Releases, tags, revisions and compatibility | TBD | TBD | TBD | Not assessed |
| UPD-02 | Upstream update/reinstall | Official update procedure, preservation and recovery | TBD | TBD | TBD | Not assessed |
| CFG-01 | Configuration model | Syntax, defaults, includes, validation, comments, unknown keys | TBD | TBD | TBD | Not assessed |
| CFG-02 | Runtime modes and options | All documented modes, parameters, values and interactions | TBD | TBD | TBD | Not assessed |
| CFG-03 | Network protocol/IP options | IPv4, IPv6, TCP, UDP, QUIC and other documented controls | TBD | TBD | TBD | Not assessed |
| FW-01 | Firewall integration | Supported backends, rules, prerequisites, cleanup | TBD | TBD | TBD | Not assessed |
| LIST-01 | User lists | List types, paths, formats, editing, import/export | TBD | TBD | TBD | Not assessed |
| LIST-02 | List sources and refresh | Sources, download, refresh, update schedule, errors | TBD | TBD | TBD | Not assessed |
| STRAT-01 | Strategy configuration | Native strategy parameters, profiles, selection/application | TBD | TBD | TBD | Not assessed |
| DIAG-01 | Runtime diagnostics | Native status, diagnostic commands, health checks | TBD | TBD | TBD | Not assessed |
| LOG-01 | Logs | Log locations, formats, rotation and troubleshooting | TBD | TBD | TBD | Not assessed |
| BC-01 | blockcheck suites | All suites and test modules in the pinned source tree | TBD | `blockcheck2.sh` | TBD | Not assessed |
| BC-02 | blockcheck inputs | Domains/URIs, IP versions, protocols, repeats, scan modes, parallelism, advanced variables | TBD | `blockcheck2.sh` | TBD | Not assessed |
| BC-03 | blockcheck execution | Privileges, preconditions, temporary firewall/process behavior, cancellation | TBD | `blockcheck2.sh` | TBD | Not assessed |
| BC-04 | blockcheck results | Raw output, common/coverage results, exit codes and interpretation | TBD | `blockcheck2.sh` | TBD | Not assessed |
| CUSTOM-01 | Custom scripts/extensions | Supported extension points, lifecycle, security and ownership | TBD | TBD | TBD | Not assessed |
| BIN-01 | Binary components | Required binaries, compatibility checks, build/download behavior | TBD | TBD | TBD | Not assessed |
| REC-01 | Recovery and cleanup | Stale state, interrupted operations, firewall/service recovery | TBD | TBD | TBD | Not assessed |

## 4. Required evidence per capability

For each capability ID, add a detailed subsection containing:

1. **User task:** what the user wants to accomplish.
2. **Upstream definition:** exact Russian manual heading and supplementary English heading.
3. **Source evidence:** exact source file, function/script, and line range at the pinned commit.
4. **Availability:** Linux distributions, init systems, architectures, prerequisites, and version constraints.
5. **Native interface:** script/command/config/service/file/API; exact arguments, accepted values, environment variables, and defaults.
6. **Privilege model:** whether root/polkit is required and why.
7. **Side effects:** files, directories, services, timers, firewall rules, network state, package-manager changes, and user data affected.
8. **Output contract:** stdout, stderr, exit codes, status transitions, errors, cancellation, cleanup.
9. **UI mapping:** normal UI, advanced UI, guided terminal/PTY, or documented non-exposure with reason.
10. **Tests:** unit, integration, VM, and package tests with exact environment and result.
11. **Limitations:** unsupported configurations and unresolved questions.
12. **Status:** use the vocabulary above; cite evidence for Tested/Supported.

## 5. UI coverage rule

Every applicable capability must have an explicit access path:
- **Guided UI** for common workflows.
- **Advanced UI** for supported advanced parameters and modes.
- **Native terminal/PTY workflow** when upstream only provides an interactive interface or when exposing a GUI would require an undocumented/unsafe contract.

A terminal path is not an excuse to omit a common workflow from the GUI. Conversely, a GUI must not pretend to support an operation if it cannot preserve upstream semantics.

Show exact native values and complete logs in advanced details. Do not evaluate user input or script output as shell code. Do not automatically modify active configuration from diagnostic recommendations.

## 6. Coverage gate

Implementation beyond the minimal application shell must not proceed until:
- the selected upstream baseline is pinned and trust checks are recorded;
- both manuals have been reviewed;
- the source tree has been inventoried;
- every applicable documented user-facing capability has a matrix row;
- material manual/source discrepancies are recorded;
- each capability has a proposed access path and risk assessment;
- the first-release scope and any Limited/Unsupported entries are explicitly approved.

A release cannot claim full native-function integration while any in-scope capability remains Not assessed, Planned, Implemented, Tested-only, Blocked, or silently omitted.
