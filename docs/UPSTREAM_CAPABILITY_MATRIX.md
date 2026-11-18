# Upstream zapret2 Capability Matrix — Linux

**Inventory status:** source/manual inventory completed for the selected release snapshot; application exposure and runtime verification are not implemented yet.  
**Review date:** 2026-11-19  
**Product target:** Linux desktop, KDE Plasma 6. Initial supported distributions: Debian 13 and Fedora. Initial init integration: systemd.

> This document inventories native zapret2 capabilities. It does not claim that zapret2control already implements or supports them. Upstream features that are only intended for embedded routers, other operating systems, or advanced zapret2 developers are identified separately rather than silently presented as desktop features.

## 1. Upstream compatibility baseline

| Field | Value |
|---|---|
| Canonical repository | https://github.com/bol-van/zapret2 |
| Latest published release observed | `v1.0.7` |
| Release tag | `v1.0.7` |
| Tag target commit | `038c97e5819df10f8e8c22a2f1fffe57f2f7379b` |
| Commit message/date | `blockcheck2: higher prio value comes first`, 2026-11-05 |
| Russian manual | `docs/manual.md`, blob `04d696ec1b757a85891b3f5a75c86438304b3bc6` |
| English manual | `docs/manual.en.md`, blob `9a8a99862e5b5c031f186e448c25bab541beae98` |
| Source tree | 253 tracked files at the selected tag |
| Signature | GitHub reports unsigned (`verified=false`, reason `unsigned`). No publisher authentication is claimed. |
| Retrieval | GitHub canonical repository API over HTTPS, 2026-11-19 |
| Latest master tracking point | `3bae3d26d741a8f6e0bc4ea651656b4e1a438375`, 2026-11-14; later than the release tag and not selected as the product baseline |
| Baseline decision | Use the latest published release `v1.0.7` for initial integration. Track master separately and evaluate future changes explicitly. |

The v1.0.7 release asset metadata includes a SHA-256 digest for `zapret2-v1.0.7.tar.gz`: `547a5dce3f4820721a425680ea3f7f3971b7e9c772b8c795dd2d623148da0928`. This is release metadata, not independent publisher authentication. The tag commit is unsigned.

Both manuals were fetched at the exact tag. Their top-level and second-level chapter structure is aligned for the reviewed Linux subjects. Their documented engine option inventories contain the same 86 option names across all engine variants. The inventory below filters out WinDivert/Windows-only and BSD-only engine controls where they do not apply to the Linux desktop product. The Russian manual remains primary.

## 2. Scope classification

- **In scope:** Linux `nfqws2`, Linux traffic interception through nftables or iptables, systemd startup, upstream configuration, lists, helper utilities, Lua strategy libraries, installer/uninstaller, and blockcheck2 on Linux.
- **Advanced in scope:** custom Lua strategy development, custom init scripts, custom lists, custom firewall hooks, manual per-instance systemd units, and diagnostic shell variables. These must remain accessible without pretending every advanced operation belongs on the main screen.
- **Not a Linux desktop target:** OpenWrt-specific UCI/hotplug/firewall integration, embedded firmware packaging, Android build targets, BSD `dvtws2`, Windows `winws2`/WinDivert and Windows logical-network filters.
- **Not an application-owned engine:** packet parsing, conntrack, DPI strategy execution, Lua VM, packet generation, firewall rule semantics, list generation, and blockcheck algorithms remain upstream-owned.

## 3. Core engine and traffic interception

### ENG-01 — Linux packet interception

| Attribute | Inventory |
|---|---|
| Native mechanism | Linux kernel traffic interception using NFQUEUE, configured by iptables or nftables rules; packets are delivered to `nfqws2`. |
| Manual | `Перехват трафика из ядра ОС → Перехват трафика в ядре Linux → nftables / iptables`. |
| Native components | `nfqws2`, `common/linux_fw.sh`, `common/ipt.sh`, `common/nft.sh`, `common/fwtype.sh`, Linux init scripts. |
| Application surface | Firewall backend/status and safe diagnostics in the normal UI; detailed rule state and advanced controls in an expert view. |
| Important boundary | The application must not synthesize its own parallel firewall policy. It must inspect and invoke upstream-managed state. |

Upstream documents two Linux firewall backends:
- nftables;
- iptables (including IPv4/IPv6 handling).

The selected config may explicitly set `FWTYPE=iptables` or `FWTYPE=nftables`; otherwise upstream detects the backend. Do not silently force one backend in the application.

### ENG-02 — Linux engine process

The Linux runtime engine is `nfqws2`. It accepts command-line options and Lua strategy modules. It can be run directly for advanced use, while the standard installation uses upstream startup scripts and configuration.

Do not confuse `nfqws2` with:
- `dvtws2`: BSD divert-socket engine;
- `winws2`: Windows/WinDivert engine.

### ENG-03 — Common nfqws2 process controls

Documented common controls applicable to the Linux engine:

- `--debug=0|1|syslog|android|@filename`: debug output destination;
- `--version`: print version and exit;
- `--dry-run`: validate CLI options and file presence; does not validate Lua script correctness;
- `--comment=...`: ignored annotation;
- `--intercept=0|1`: allow/disable packet interception; with 0, Lua init runs but NFQUEUE is not initialized;
- `--daemon`: detach from console;
- `--chdir[=path]`: change working directory;
- `--pidfile=filename`: write PID;
- `--ctrack-timeouts=S:E:F[:U]`: conntrack timeouts;
- `--ctrack-disable[=0|1]`: conntrack control;
- `--payload-disable[=type,...]`: disable payload recognition;
- `--server[=0|1]`: server/listener mode;
- `--ipcache-lifetime=int`: IP cache lifetime;
- `--ipcache-hostname[=0|1]`: hostname caching for strategy phase zero;
- `--reasm-disable[=type,...]`: disable reassembly for supported payload types.

Application exposure: common safe diagnostics and a small set of validated controls in guided UI; complete supported options in advanced configuration. Do not expose process-internal/debug-only controls as ordinary preferences without a use case.

### ENG-04 — Lua engine initialization

Documented initialization controls:

- `--writable[=dir]`: create one writable Lua directory and set `WRITABLE`;
- `--blob=name:[+offset]@file|0xHEX`: load binary data into Lua;
- `--lua-init=@file|lua_text`: run Lua initialization code once; gzip Lua files are supported and `.gz` is auto-checked;
- `--lua-gc=int`: periodic Lua garbage collection interval.

These options are important for custom strategies and advanced configuration. GUI must preserve them and must not execute arbitrary Lua in the GUI process.

### ENG-05 — Multi-strategy profiles and filters

Native profile controls:

- profile boundaries and identity: `--new[=name]`, `--skip`, `--name=name`;
- reusable templates: `--template[=name]`, `--import=name`;
- profile metadata: `--cookie[=string]`;
- L3/L4 filters: `--filter-l3=ipv4|ipv6`, `--filter-tcp=...`, `--filter-udp=...`, `--filter-icmp=...`, `--filter-ipp=...`;
- flow protocol filter: `--filter-l7=proto[,proto]`;
- packet mark filter: `--filter-mark=mark[/mask]`;
- Wi-Fi SSID filter: `--filter-ssid=ssid1,...`, `--filter-ssid-neg[=0|1]`;
- included/excluded IP sources: `--ipset=file`, `--ipset-ip=list`, `--ipset-exclude=file`, `--ipset-exclude-ip=list`;
- included/excluded host sources: `--hostlist=file`, `--hostlist-domains=list`, `--hostlist-exclude=file`, `--hostlist-exclude-domains=list`;
- auto hostlist: `--hostlist-auto=file` plus the thresholds and diagnostics below.

Auto-hostlist controls:
- `--hostlist-auto-fail-threshold`;
- `--hostlist-auto-fail-time`;
- `--hostlist-auto-retrans-threshold`;
- `--hostlist-auto-retrans-reset`;
- `--hostlist-auto-retrans-maxseq`;
- `--hostlist-auto-incoming-maxseq`;
- `--hostlist-auto-udp-out`;
- `--hostlist-auto-udp-in`;
- `--hostlist-auto-debug`.

Packet-pass filters:
- `--payload=type[,type]`;
- `--out-range=...`;
- `--in-range=...`.

Strategy invocation:
- `--lua-desync=function[:param=value[:...]]`.

The exact value grammar, negation syntax, ranges, ordering, and profile semantics must be taken from the pinned manual and parser. Do not approximate them with free-form GUI string splitting.

### ENG-06 — Linux-specific nfqws2 options

- `--qnum=nfqueue_number`;
- `--user=username`;
- `--uid=uid[:gid1,gid2,...]`;
- `--bind-fix4`;
- `--bind-fix6`;
- `--fwmark=value`.

The `--port` divert option and all `--wf-*` WinDivert options belong to other engine targets and are not Linux nfqws2 controls. Windows logical-network filter options are also out of scope.

### ENG-07 — Protocol and payload recognition

The pinned manual documents these flow protocol/payload pairs:

| Flow protocol | L4 | Recognized payloads |
|---|---|---|
| http | TCP | `http_req`, `http_reply` |
| tls | TCP | `tls_client_hello`, `tls_server_hello` |
| xmpp | TCP | `xmpp_stream`, `xmpp_starttls`, `xmpp_proceed`, `xmpp_features` |
| mtproto | TCP | `mtproto_initial` |
| bt | TCP | `bt_handshake` |
| quic | UDP | `quic_initial` |
| wireguard | UDP | `wireguard_initiation`, `wireguard_response`, `wireguard_cookie`, `wireguard_keepalive` |
| dht | UDP | `dht` |
| utp_bt | UDP | `utp_bt_handshake` |
| discord | UDP | `discord_ip_discovery` |
| stun | UDP | `stun` |
| dns | UDP | `dns_query`, `dns_response` |
| dtls | UDP | `dtls_client_hello`, `dtls_server_hello` |
| ICMP | ICMP | `ipv4`, `ipv6`, `icmp` |

Special payload values include `empty`, `unknown`, `all`, and `known` in the contexts defined by the manual. Profile protocol identity is retained for the flow after the first recognized payload; later payloads may be known or unknown.

Application surface: searchable protocol/payload selectors and help text; preserve all upstream values and allow advanced combinations.

### ENG-08 — Additional engine behavior

The manual also documents:
- protocol recognition and multi-packet payload reassembly;
- IP cache and hostname cache;
- server/listener mode;
- signals and graceful process termination;
- debug logging;
- virtual-machine and sandbox behavior;
- Lua initialization, Lua calls and blob passing;
- per-profile filters;
- execution plans, instance cutoff and cancellation;
- timers and Lua timer callbacks.

These are engine semantics, not separate desktop features in every case. They must be preserved by configuration editing and advanced diagnostics. Do not implement a competing packet engine.

## 4. Lua strategy and extension capabilities

### LUA-01 — Native Lua modules shipped in v1.0.7

The selected source tree contains six Lua modules:

| Module | Purpose | Application treatment |
|---|---|---|
| `zapret-lib.lua` | Shared packet, parsing, protocol, file, IP, timer and orchestration helpers | Ship/use as upstream asset; advanced reference/help |
| `zapret-antidpi.lua` | DPI strategy primitives and strategy functions | Ship/use as upstream asset; expose strategy options without rewriting functions |
| `zapret-auto.lua` | State, success/failure detection and strategy orchestration | Ship/use as upstream asset; explain automatic orchestration behavior |
| `zapret-obfs.lua` | Additional obfuscation helpers (including WireGuard/IPP/UDP-to-ICMP/SYN-related helpers) | Advanced capability; document and preserve |
| `zapret-pcap.lua` | PCAP header/packet writing and packet capture helper | Advanced diagnostics/development capability |
| `zapret-tests.lua` | Lua/runtime self-tests for crypto, parsing, networking, timers and raw-send behavior | Developer/diagnostic capability; run only in a controlled test environment |

The source tree also includes fake packet blobs under `files/fake/`; these are data assets used by strategies and diagnostics, not editable application settings.

### LUA-02 — zapret-antidpi strategy functions

Source functions at v1.0.7 include:

- base actions: `drop`, `send`, `send_timer_delayed`, `pktmod`;
- HTTP: `http_domcase`, `http_hostcase`, `http_methodeol`, `http_unixeol`;
- TCP/window: `wsize`, `wssize`, `synack`, `synack_split`;
- TLS/fake: `tls_client_hello_clone`, `syndata`, `rst`, `fake`;
- segmentation/order: `multisplit`, `multidisorder`, `multidisorder_legacy`, `fakedsplit`, `fakeddisorder`, `hostfakesplit`, `tcpseg`, `oob`;
- UDP: `udplen`, `dht_dn`.

Parameters and semantics are function-specific and must be read from the Lua source/manual; function names alone are not sufficient to build a safe form.

### LUA-03 — zapret-auto orchestration functions

Source functions include:
- state/records: `standard_hostkey`, `automate_host_record`, `automate_conn_record`;
- failure counters: `automate_failure_counter`, `automate_failure_counter_reset`;
- detection: `standard_failure_detector`, `standard_success_detector`, `automate_failure_check`;
- orchestration: `circular`, `condition`, `per_instance_condition`, `stopif`, `repeater`;
- conditions: `cond_true`, `cond_false`, `cond_random`, `cond_payload_str`, `cond_tcp_has_ts`, `cond_lua`.

These are strategy authoring/automation features. Do not expose them as generic consumer toggles without representing their actual parameter schemas and execution semantics.

### LUA-04 — zapret-lib capability families

The documented helper library covers:
- basic desync helpers and debugging;
- variable inspection, deep copy, arrays and string helpers;
- binary blobs, hex/pattern processing and binary data;
- TCP sequence arithmetic and packet position/range helpers;
- packet/L3/L4 protocol inspection and modification;
- IPv4/IPv6 header handling and extension headers;
- HTTP URL/request/reply parsing and reconstruction;
- TLS record/handshake parsing, modification and reconstruction;
- QUIC parsing helpers;
- hostnames, DNS/IP resolution and interface/address helpers;
- file paths, writable directory, file reading/writing;
- gzip compression/decompression;
- TTL/autottl and IP ID/fooling/fragmentation operations;
- packet sending, segmentation, fragmentation and raw packet operations;
- direction/payload filters and multi-packet payload handling;
- timers and execution-plan orchestration/replay.

The manual documents the nfqws2 C interface in addition to Lua helper functions: logging, IP conversion, bitwise/unsigned arithmetic, integer division, random data, parsing, cryptography (hash/AES/AES-GCM/AES-CTR/HKDF), compression, system functions, packet reconstruction/dissection, conntrack, IP discovery, raw send/receive, payload markers, TLS modification, instance execution and timers.

This is an advanced developer API. The desktop application must preserve module files and provide documentation/inspection access; it must not attempt to recreate the API in C++.

### LUA-05 — Source/manual documentation gap

The source includes `zapret-obfs.lua`, `zapret-pcap.lua`, and `zapret-tests.lua`. Not every function in these modules has an equivalent dedicated chapter in the manual. Treat these as source-discovered capabilities. Before exposing individual functions as GUI controls, inspect their source, call sites, parameter contracts and tests. Do not claim a documented GUI interface where the manual does not define one.

## 5. Configuration and profiles

### CFG-01 — Native config file

The upstream `config` file is a **shell include**, not JSON, YAML or INI. It supports shell assignments, comments and shell expressions, including references to other variables and arithmetic.

The engine `nfqws2` does not read this file directly. Upstream init scripts source it and translate values into engine arguments, firewall rules and list behavior.

Application requirements:
- never source the config inside the GUI process;
- preserve comments, unknown variables and shell expressions;
- do not convert the file to a lossy key/value representation;
- edit known values using a safe, syntax-aware strategy;
- keep an advanced raw editor for expert users;
- validate through upstream-supported checks where available, while clearly stating that `nfqws2 --dry-run` does not validate Lua script correctness;
- back up and atomically write user changes;
- display effective values separately from literal text when safely derivable.

### CFG-02 — Variables present in config.default

The v1.0.7 default config defines these active variables:

- list/IP set sizing: `SET_MAXELEM`, `IPSET_OPT`, `IP2NET_OPT4`, `IP2NET_OPT6`;
- auto-hostlist: `AUTOHOSTLIST_INCOMING_MAXSEQ`, `AUTOHOSTLIST_RETRANS_MAXSEQ`, `AUTOHOSTLIST_RETRANS_RESET`, `AUTOHOSTLIST_RETRANS_THRESHOLD`, `AUTOHOSTLIST_FAIL_THRESHOLD`, `AUTOHOSTLIST_FAIL_TIME`, `AUTOHOSTLIST_UDP_IN`, `AUTOHOSTLIST_UDP_OUT`, `AUTOHOSTLIST_DEBUGLOG`;
- DNS/list resolver: `MDIG_THREADS`, `MDIG_EAGAIN`, `MDIG_EAGAIN_DELAY`, `GZIP_LISTS`;
- packet marks: `DESYNC_MARK`, `DESYNC_MARK_POSTNAT`;
- engine: `NFQWS2_ENABLE`, `NFQWS2_PORTS_TCP`, `NFQWS2_PORTS_UDP`, `NFQWS2_TCP_PKT_OUT`, `NFQWS2_TCP_PKT_IN`, `NFQWS2_UDP_PKT_OUT`, `NFQWS2_UDP_PKT_IN`, `NFQWS2_OPT`;
- filtering/offload: `MODE_FILTER`, `FLOWOFFLOAD`;
- startup/firewall: `INIT_APPLY_FW`;
- address families: `DISABLE_IPV6` (and the documented optional `DISABLE_IPV4`);
- ICMP: `FILTER_TTL_EXPIRED_ICMP`.

Documented optional/commented config values include `TMPDIR`, `WS_USER`, `FWTYPE`, `POSTNAT`, `IPSET_HOOK`, `LISTS_RELOAD`, `FILTER_MARK`, keepalive port variables, OpenWrt LAN/WAN interface names, `IFACE_LAN`, `IFACE_WAN`, `IFACE_WAN6`, firewall pre/post up/down hooks and `GETLIST`.

The manual's complete config table is authoritative for meanings, defaults, accepted values and platform applicability. Do not infer a setting's semantics from its variable name.

### CFG-03 — Engine config and strategy editing

Expose:
- engine enabled/disabled state (`NFQWS2_ENABLE`);
- TCP/UDP port selection and packet count limits;
- advanced `NFQWS2_OPT` profile/strategy configuration;
- address-family switches;
- filtering mode;
- firewall selection and mark configuration;
- auto-hostlist tuning;
- list resolution and compression tuning;
- interface and firewall hooks where applicable;
- flow-offload policy where supported.

Do not simplify `NFQWS2_OPT` into a small fixed set of strategies that discards supported upstream CLI/Lua functionality.

## 6. Hostlists, IP lists and list update system

### LIST-01 — Standard list model

Upstream distinguishes user-maintained files from generated files. Generated lists may be overwritten by scripts. Hostlists may contain domain names; user hostlists may also contain IPv4/IPv6 addresses and CIDR entries. Generated hostlists contain domain names only. Lists may be gzip-compressed; generated list compression is controlled by `GZIP_LISTS`. IPv6 IP list files use a `6` suffix before the extension.

Documented standard files:

| File | Type | Purpose |
|---|---|---|
| `zapret-hosts-user.txt` | User hostlist | Include |
| `zapret-hosts-user-exclude.txt` | User hostlist | Exclude |
| `zapret-hosts-user-ipban.txt` | User hostlist | Separate ipban/redirect list |
| `zapret-hosts.txt` | Generated hostlist | Include |
| generated `zapret-ip.txt`, `zapret-ip6.txt` | Generated IP lists | Include |
| `zapret-ip-user.txt`, `zapret-ip-user6.txt` | User-derived IP lists | Include |
| `zapret-ip-exclude.txt`, `zapret-ip-exclude6.txt` | Exclusion IP lists | Exclude |
| `zapret-ip-user-ipban.txt`, `zapret-ip-user-ipban6.txt` | User-derived IP lists | ipban |
| `zapret-ip-ipban.txt`, `zapret-ip-ipban6.txt` | Generated IP lists | ipban |

The manual also describes generated auto-hostlist state (including `zapret-hosts-auto.txt`) and the default user exclusion file `zapret-hosts-user-exclude.txt.default`.

Hostlists may be used directly by nfqws2 or resolved to IP sets through `mdig`, depending on the selected mode. Include IP lists are applied to kernel sets when `MODE_FILTER=ipset`; exclusion IP lists are applied by upstream firewall rules. The application must explain this distinction instead of presenting all lists as interchangeable.

### LIST-02 — Kernel sets and list actions

Upstream `create_ipset.sh` creates/loads the backend-specific kernel sets:
- `nozapret`, `nozapret6`: excluded IPs;
- `zapret`, `zapret6`: included IPs;
- `ipban`, `ipban6`: separate include list for user-managed routing/proxy redirection.

Supported native operations:
- `clear_lists.sh`: remove generated list files;
- `create_ipset.sh`: populate kernel sets; supports `clear` and `no-update`;
- `get_config.sh`: run the script selected by `GETLIST`, defaulting to `get_ipban.sh`;
- list-specific generation/resolution/download scripts listed below.

### LIST-03 — Upstream list provider scripts

The v1.0.7 `ipset/` directory contains:

- `get_user.sh`;
- `get_ipban.sh`;
- `get_exclude.sh`;
- `get_antifilter_allyouneed.sh`;
- `get_antifilter_ip.sh`;
- `get_antifilter_ipresolve.sh`;
- `get_antifilter_ipsmart.sh`;
- `get_antifilter_ipsum.sh`;
- `get_antizapret_domains.sh`;
- `get_refilter_domains.sh`;
- `get_refilter_ipsum.sh`;
- `get_reestr_resolvable_domains.sh`;
- `get_reestr_preresolved.sh`;
- `get_reestr_preresolved_smart.sh`;
- `get_config.sh`;
- `create_ipset.sh`;
- `clear_lists.sh`.

Provider families and upstream sources:
- antifilter: `antifilter.network`;
- antizapret domains: `antizapret.prostovpn.org`;
- Re:filter: `github.com/1andrevich/Re-filter-lists`;
- reestr lists: `github.com/bol-van/rulist`.

The upstream manual itself notes that the old zapret-info source behind some lists may no longer be current. The application must display provider/source, last update, result and errors; it must not imply that every source is current or equally reliable.

### LIST-04 — ipban semantics

ipban is a resolution/list mechanism that produces kernel sets `ipban`/`ipban6`. zapret2 does not itself route or proxy those addresses. Users may connect those sets to policy-based routing or a proxy using their own network configuration. Do not label ipban as an automatic bypass feature.

### LIST-05 — Scheduled updates

For systemd, the release provides:
- `zapret2-list-update.service` → executes `/opt/zapret2/ipset/get_config.sh`;
- `zapret2-list-update.timer` → calendar dates 2,4,6,...,30 at midnight with randomized delay up to 86400 seconds and persistent activation.

The timer is enabled and started by the upstream installer in the systemd path. UI must show timer enabled/active/last result and provide an explicit manual refresh operation that invokes the upstream script, not a duplicate downloader.

### LIST-06 — List editing and ownership

The application must distinguish:
- manually maintained user lists;
- generated/downloaded lists;
- auto-hostlist state;
- kernel sets derived from files.

Do not edit generated lists as if they were durable user input. Provide safe editing/import/export for user-managed files, and explicit refresh/clear actions for generated state.

## 7. Service, startup and firewall integration

### SVC-01 — systemd primary service

Release files:
- `init.d/systemd/zapret2.service`;
- `init.d/sysv/zapret2`;
- helper scripts in `common/`.

The systemd unit is a forking service. It invokes:
- start: `/opt/zapret2/init.d/sysv/zapret2 start`;
- stop: `/opt/zapret2/init.d/sysv/zapret2 stop`.

The init script supports start/stop; manual systemd operations include start, stop, restart, status, enable and disable. Application must read actual systemd state and not infer service health solely from a successful command exit.

### SVC-02 — Autostart and list timer are separate

Service autostart and list-update timer are independent systemd units. Expose their states separately. Do not merge them into one “autostart” toggle.

### SVC-03 — Alternate service managers

The manual also contains OpenRC and generic SysV integration, plus OpenWrt-specific init/hotplug/firewall integration. They are upstream capabilities but outside the initial KDE desktop/systemd support target. Record them as not supported by the initial product release, not as missing from upstream.

### SVC-04 — Advanced per-instance systemd

The manual documents `init.d/systemd/nfqws2@.service`:
- user creates `/etc/zapret2/INSTANCE.conf` with nfqws2 command-line parameters;
- enables/starts/stops/restarts `nfqws2@INSTANCE`.

This is an alternative advanced deployment. It does **not** set up iptables/nftables rules; those must be configured separately. Do not conflate this with the standard zapret2 service. Expose as an advanced/manual workflow only, with a clear warning and validation.

### FW-01 — Firewall lifecycle

Upstream init helpers create/remove tables, chains, sets and queue rules for the selected backend. Linux source areas include:
- `common/linux_fw.sh`;
- `common/ipt.sh`;
- `common/nft.sh`;
- `common/fwtype.sh`;
- `common/linux_iphelper.sh`;
- `common/linux_daemons.sh`.

Supported configuration concerns include:
- automatic or explicit firewall backend;
- IPv4/IPv6 enablement;
- pre-NAT/post-NAT operation;
- packet marks to prevent loops;
- TCP/UDP ports and packet-count windows;
- interface selection where applicable;
- firewall apply enablement;
- pre/post up/down hooks;
- flow-offload policy;
- TTL-expired ICMP filtering;
- custom firewall rules and scripts.

### FW-02 — Flow offload

The installer asks about flow offloading where the selected backend/platform supports it. Upstream documents policies such as `donttouch`, `none`, `software`, and `hardware`; availability and compatibility depend on firewall/backend and hardware. Upstream warns that flow offload may be incompatible with nfqws packet modification and may break traffic shaping. UI must show applicability, current policy and consequences; never silently change system-wide offload settings.

### FW-03 — Custom firewall hooks and scripts

Config supports pre/post up/down firewall hooks. The Linux examples include:
- `10-keenetic-udp-fix`;
- `20-fw-extra`;
- `40-webserver`;
- `50-dht4all`;
- `50-discord-media`;
- `50-nfqws-ipset`;
- `50-quic4all`;
- `50-stun4all`;
- `50-wg4all`;
- `80-dns-intercept`;
- `99-lan-filter`.

These examples are advanced, user-modifiable shell scripts. Preserve and display them; do not auto-enable examples or assume they are compatible with every network.

## 8. Installation, prerequisites, update and removal

### INST-01 — Official installation

Official scripts in the release:
- `install_easy.sh`: primary interactive installer;
- `install_bin.sh`: architecture-appropriate binary selection/linking from release `binaries`;
- `install_prereq.sh`: prerequisite package installation;
- shared installer functions in `common/installer.sh`.

Manual behavior:
- easy installer is interactive and asks questions;
- it can be launched from another location;
- it checks system/source files and binaries;
- it configures architecture-compatible binaries and prerequisites;
- if binaries are absent (e.g. a source clone), it may attempt to build and needs compiler, make and development dependencies;
- it targets `/opt/zapret2`;
- if replacing an existing target tree, it can preserve selected config/custom scripts/user lists/auto-hostlist or replace them;
- it repairs file/directory permissions when copying;
- it configures supported init integration and list updates;
- on unsupported Linux systems, startup integration may require manual setup.

Application flow: stage and verify the full release tree, then launch the official installer in a visible PTY. Never fabricate answers or reimplement its installer logic. Explain package changes, target replacement, preservation choices and service/timer activation before the user proceeds.

### INST-02 — Prerequisite management

`install_prereq.sh` is an upstream helper. Its package-manager coverage is not universal. On unsupported systems, the manual says prerequisites may need manual installation. The app should inspect/report missing dependencies and invoke the upstream helper only with explicit consent; do not assume it works on every Debian/Fedora derivative.

### INST-03 — Binary selection/build

`install_bin.sh` searches architecture-compatible release binaries and creates links under `nfq2`, `mdig`, and `ip2net`. It is designed to tolerate embedded systems but is not guaranteed on every trimmed firmware. When binaries are absent, installer may compile. App must report architecture, selected binary and build outcome.

### INST-04 — Official removal

`uninstall_easy.sh`:
- ensures config exists and sources it;
- requires root;
- detects system type;
- clears kernel sets;
- stops/removes supported service/timer integration;
- removes zapret nftables table and cron entries where applicable;
- on generic Linux without supported service manager, warns that autostart removal is manual;
- does not remove the installation directory;
- on OpenWrt, handles additional firmware hooks/packages.

Application must separate upstream-managed removal from optional deletion of residual `/opt/zapret2` and user data. The latter is a separate destructive action requiring confirmation and preview.

### UPD-01 — Upstream update

The reviewed manual does not define a separate GUI-oriented update API or a stable non-interactive update command. The documented supported workflow is installer/replacement using the official release tree and installer preservation prompts. Do not invent `update <tag>` semantics. Implement a versioned release acquisition + official reinstall workflow only after its staging/prompt contract is validated; preserve config, custom scripts and user lists.

## 9. blockcheck2 diagnostics

### BC-01 — Purpose and interaction model

`blockcheck2.sh` is a POSIX shell script with modular test suites in `blockcheck2.d/<suite>`. It is interactive by default and asks only a subset of its many settings; additional controls are passed as shell variables. It can test multiple domains/URIs, repeat attempts, run tests in parallel, write logs through standard shell redirection, and summarize successful strategies.

The upstream manual explicitly says it is an investigation/automation aid, not a strategy oracle. Interpretation of IP-block checks and results belongs to the user.

### BC-02 — DNS diagnostics

- tests whether DNS answers are spoofed;
- checks external DNS reachability;
- can switch to DoH when spoofing is detected;
- supports configurable DNS servers, domains, DoH server lists, forced DoH on/off and skipping DNS checks.

### BC-03 — IP/port blocking diagnostics

- checks destination port connectivity using nc/ncat;
- performs curl-based cross-domain/IP checks for full, port, partial-IP and SNI-related blocking scenarios;
- does not automatically issue a definitive IP-block verdict because netcat implementations have inconsistent exit-code behavior and responses need human interpretation.

### BC-04 — Protocol coverage

Tests:
- plain HTTP;
- HTTPS with TLS 1.2;
- HTTPS with TLS 1.3;
- HTTP/3 over QUIC.

Availability depends on the installed curl build. URI input is a domain or domain/path without `http://` or `https://`; HTTP uses GET, HTTPS defaults to HEAD, and `CURL_HTTPS_GET=1` changes HTTPS to GET.

### BC-05 — Scan modes

Manual describes:
- `standard`: adaptive test algorithm, retains multiple attempts despite individual failures;
- `quick`: standard with earlier termination after a failed repeated attempt;
- `veryquick`: further short-circuits remaining variants in selected fake-based families after first successful combination;
- `force`: maximizes tested variants without pruning based on previous results.

**Manual inconsistency:** the shell-variable reference table lists `SCANLEVEL=quick|standard|force` and omits `veryquick`, while the scan-level prose explicitly documents `veryquick`. The source must be checked before exposing this value in a GUI. This discrepancy remains recorded; do not silently resolve it.

### BC-06 — Standard test suite

The v1.0.7 source contains these 13 standard modules:

1. `10-http-basic.sh` — HTTP basic tests;
2. `15-misc.sh` — miscellaneous HTTP/TLS tests;
3. `17-oob.sh` — TCP OOB tests;
4. `20-multi.sh` — multi-split/disorder tests;
5. `23-seqovl.sh` — sequence overlap tests;
6. `24-syndata.sh` — SYN data tests;
7. `25-fake.sh` — fake packet tests;
8. `30-faked.sh` — fake/split variants;
9. `35-hostfake.sh` — host fake/split tests;
10. `50-fake-multi.sh` — fake plus multi-strategy tests;
11. `55-fake-faked.sh` — fake/faked combinations;
12. `60-fake-hostfake.sh` — fake/hostfake combinations;
13. `90-quic.sh` — QUIC tests.

The suite uses `def.inc`; it must be shipped with matching scripts. Tests can be skipped using documented `NOTEST_*` variables. The manual describes tuning TTL, autottl deltas, fake repeats, fooling sets, fake blobs, split patterns, legacy multidisorder and per-protocol skip flags.

### BC-07 — Custom test suite

The `custom` suite reads per-protocol strategy files:
- `list_http.txt`;
- `list_https_tls12.txt`;
- `list_https_tls13.txt`;
- `list_quic.txt`.

Each strategy occupies one line; lines beginning with `#` are comments. Custom suite files and `blockcheck2.d/custom/README.txt` are upstream assets. Strategy strings are shell-sensitive; upstream has fallback parsing behavior. Application must not execute user strategy strings through shell evaluation. If offering a custom suite editor, preserve raw files and run the upstream script in its native process environment.

### BC-08 — User-facing blockcheck parameters

Documented shell variables include:
- target selection: `DOMAINS`, `TEST`, `IPVS=4|6|46`;
- protocol toggles: `ENABLE_HTTP`, `ENABLE_HTTPS_TLS12`, `ENABLE_HTTPS_TLS13`, `ENABLE_HTTP3`;
- attempts/performance: `REPEATS`, `PARALLEL`, `PACE`, `SCANLEVEL`;
- batch/diagnostic visibility: `BATCH`, `CURL_CMD`, `CURL_OPT`;
- curl behavior: `CURL`, `CURL_MAX_TIME`, `CURL_MAX_TIME_QUIC`, `CURL_MAX_TIME_DOH`, `CURL_HTTPS_GET`, `CURL_PAD`, `PAD_MAX_HEADER`;
- ports: `HTTP_PORT`, `HTTPS_PORT`, `QUIC_PORT`;
- DNS/DoH: `SKIP_DNSCHECK`, `DNSCHECK_DNS`, `DNSCHECK_DOM`, `SECURE_DNS`, `DOH_SERVERS`, `DOH_SERVER`;
- IP block: `SKIP_IPBLOCK`, `UNBLOCKED_DOM`;
- extra engine args: `PKTWS_EXTRA_PRE`, `PKTWS_EXTRA_PRE_1..9`, `PKTWS_EXTRA_POST`, `PKTWS_EXTRA_POST_1..9`;
- simulation: `SIMULATE`, `SIM_SUCCESS_RATE`.

The standard suite additionally documents `MIN_TTL`, `MAX_TTL`, `MIN_AUTOTTL_DELTA`, `MAX_AUTOTTL_DELTA`, `FAKE_REPEATS`, `FOOLINGS46_TCP`, `FOOLINGS6_TCP`, fake/pattern file paths, `MULTIDISORDER`, and individual `NOTEST_*` switches for the modules above.

Application must expose common inputs in a guided form and all applicable documented settings in advanced options. Keep raw output, exact settings and exit status. Support cancellation only with verified cleanup reporting. Do not stop the active zapret service or other bypass tools without explicit user approval and a tested restore plan.

### BC-09 — Result interpretation and safety

- curl success does not guarantee browser success;
- DNS, IP selection, TLS/QUIC version, browser fingerprint and multi-domain dependencies can change results;
- parallel attempts can trigger server rate limiting;
- `force` is required for reliable COMMON/COVERAGE intersections across multiple targets according to the manual;
- blockcheck temporarily manipulates firewall state and launches test processes; test preconditions and cleanup are mandatory.

## 10. Helper programs

### AUX-01 — ip2net

Purpose: convert IPv4/IPv6 address lists into CIDR subnets to reduce list size. Reads stdin and writes stdout; filters invalid IPs. Documented controls:
- `-4`, `-6`;
- `--prefix-length=min[-max]`;
- `--v4-threshold=mul/div`;
- `--v6-threshold=N`.

Used by upstream list-generation scripts. Normally it is an internal helper; expose its effects through list status rather than a standalone main-page tool.

### AUX-02 — mdig

Purpose: multi-threaded hostname resolver used when resolving hostlists into IP lists. Configurable through:
- `MDIG_THREADS`;
- `MDIG_EAGAIN`;
- `MDIG_EAGAIN_DELAY`.

The binary's own options and DNS behavior must be inspected before adding a direct UI. The normal user journey is list refresh/resolution.

### AUX-03 — Binary assets

Release binary selection is performed by `install_bin.sh` from `binaries/`. Upstream release binary architecture names and compatibility must be surfaced in installation diagnostics. Do not bundle an independently built or unverified binary in the desktop package without a separate provenance/license/compatibility decision.

## 11. Upstream files and extensibility points

Important tracked Linux-relevant paths:
- `config.default`, user `config`;
- `install_easy.sh`, `install_bin.sh`, `install_prereq.sh`, `uninstall_easy.sh`;
- `blockcheck2.sh`, `blockcheck2.d/standard/*`, `blockcheck2.d/custom/*`;
- `common/base.sh`, `custom.sh`, `dialog.sh`, `elevate.sh`, `fwtype.sh`, `installer.sh`, `ipt.sh`, `linux_daemons.sh`, `linux_fw.sh`, `linux_iphelper.sh`, `list.sh`, `nft.sh`, `virt.sh`;
- `init.d/sysv/*`, `init.d/systemd/*`, `init.d/custom.d.examples.linux/*`;
- `ipset/*`;
- `lua/*`;
- `files/fake/*`;
- `nfq2/*`, `ip2net/*`, `mdig/*`.

The source tree at the pinned tag contains 253 tracked files. The complete Git tree, rather than a guessed short list, is the file-level audit reference. Files under Windows, BSD, OpenWrt, Android, and cross-platform build areas remain upstream source but are not Linux KDE desktop features.

## 12. Capability coverage table and current zapret2control status

| ID | Native capability | Native upstream path | zapret2control status | Required exposure |
|---|---|---|---|---|
| ENG-01 | Linux NFQUEUE interception / nftables / iptables | nfqws2 + common Linux firewall scripts | Planned | Guided status + advanced backend details |
| ENG-02 | nfqws2 runtime | `nfqws2` | Planned | Runtime status, logs, advanced process details |
| ENG-03 | Common engine process controls | nfqws2 CLI | Planned | Advanced options |
| ENG-04 | Lua initialization/blobs/writable dir | nfqws2 CLI + Lua files | Planned | Advanced configuration |
| ENG-05 | Multi-strategy profiles and filters | nfqws2 CLI/config | Planned | Guided profiles + complete advanced editor |
| ENG-06 | Linux-specific engine options | nfqws2 CLI | Planned | Advanced settings |
| ENG-07 | Protocol/payload detection | nfqws2 | Planned | Searchable supported values/help |
| ENG-08 | Engine signals/cache/server/reassembly/timers | nfqws2 | Planned | Status and advanced configuration |
| LUA-01..05 | Lua libraries, strategy functions, automation, obfs, pcap, tests | `lua/*.lua` | Planned | Preserve modules; advanced docs/inspection |
| CFG-01..03 | Shell config semantics and settings | `config`, init scripts | Planned | Safe editor + raw expert view |
| LIST-01..06 | User/generated lists, kernel sets, providers, timer | `ipset/*.sh`, systemd timer | Planned | List manager + refresh/status |
| SVC-01..04 | systemd lifecycle, timer, advanced instance, other init managers | `systemd`, sysv, openrc, OpenWrt | Planned / Limited | systemd normal UI; alternative managers out of initial scope |
| FW-01..03 | Firewall, offload, hooks/custom scripts | `common/*.sh`, `init.d/custom.d.examples.linux` | Planned | Status + advanced controls |
| INST-01..04 | Install, prereq, binaries, remove | official scripts | Planned | Visible PTY workflows + postcondition checks |
| UPD-01 | Release discovery/reinstall update | release artifacts + official installer | Planned / Limited | Explicit staged reinstall; no invented updater API |
| BC-01..09 | DNS/IP diagnostics, protocols, suites, custom tests, variables/results | `blockcheck2.sh` and `blockcheck2.d` | Planned | Guided + advanced options, raw logs |
| AUX-01 | IP list subnet compression | `ip2net` | Planned | Integrated in list workflow |
| AUX-02 | Hostname resolution | `mdig` | Planned | Integrated in list workflow |
| AUX-03 | Architecture binary selection | `install_bin.sh`, `binaries/` | Planned | Installation diagnostics |

## 13. Known documentation/source discrepancies and limits

1. **blockcheck scan-level values:** prose documents `veryquick`, while the shell-variable table omits it from the listed `SCANLEVEL` values. Verify source acceptance before exposing.
2. **Lua module documentation coverage:** source ships obfs, pcap and tests modules that do not each have a full dedicated manual API chapter. Source inspection and tests are required before GUI exposure.
3. **Update interface:** manual documents interactive installer/replacement but no stable non-interactive update API. Do not invent one.
4. **List-provider freshness:** the manual itself questions the current relevance of some list sources. Show provenance and refresh results.
5. **IP-block result:** blockcheck does not automatically decide all IP-block cases; interpretation remains human.
6. **Systemd service semantics:** service unit delegates to upstream SysV script. The application must inspect both systemd state and actual process/firewall state.
7. **Config is executable shell syntax:** static parsing cannot fully evaluate arbitrary shell expressions. Do not execute it in the GUI process or promise complete semantic validation without upstream validation.

## 14. Explicit exclusions for the initial product

These are upstream capabilities but not part of the Linux KDE desktop target:
- `dvtws2` and BSD divert/PF/ipfw integration;
- `winws2`, WinDivert, Windows filters and Windows services;
- OpenWrt UCI, hotplug, fw3/fw4-specific setup and embedded firmware packaging;
- OpenRC and generic SysV service-manager support in the first release;
- Android build/integration targets.

Exclusion from the first release must be visible in documentation where users may reasonably expect it. Do not delete or modify upstream files to make excluded platforms appear supported.

## 15. Implementation acceptance gate

Inventory is complete for the selected v1.0.7 Linux source/manual snapshot. Application implementation is **not** complete.

Before a release claims native-function integration:
- map every applicable ID above to a concrete UI/advanced/PTY route;
- verify all parameter grammars and source behavior;
- implement safe configuration and user-data preservation;
- test install/reinstall/uninstall and service/timer/firewall effects in disposable Debian 13 and Fedora VMs;
- test list provider errors, refresh and scheduled updates;
- test blockcheck options, output, cancellation and cleanup;
- test custom Lua/list/script preservation;
- test privilege denial, partial failure, crash recovery and rollback;
- keep full upstream assets from the same pinned release;
- mark each capability Tested or Supported only with reproducible evidence.

No item is Supported merely because it appears in this inventory or has a visible UI control.
