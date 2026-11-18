# Product Scope Decision: Linux-only

**Status:** approved product direction  
**Decision date:** 2026-11-19

## Decision

The product is Linux-only. The supported desktop target is KDE Plasma 6 with Qt 6 and current Kirigami APIs.

Windows support is out of scope. Do not implement, maintain, test, package, document as a supported target, or use Windows as a merge/release gate.

Initial Linux packaging targets are Debian 13 DEB and Fedora RPM. Other Linux distributions may work but are not release targets until explicitly added.

## Precedence over project kit v1.1.19

The uploaded project kit v1.1.19 contains cross-platform requirements that designate Linux and Windows as first-class targets, plus Windows-specific workflows, skills, references, and release gates.

Those requirements conflict with this approved product decision. For this project, the Linux-only decision in this document and the current rewrite plan supersede cross-platform/Windows requirements in the kit.

Retain generally useful Qt, KDE, security, GitHub, testing, and release practices. Do not apply Windows-specific implementation or CI requirements. The project kit itself should be revised in a later documentation maintenance change so its normative files no longer contradict the approved scope.

## Technical baseline

- KDE Plasma 6
- Qt 6
- Current supported Kirigami
- C++ and QML
- CMake
- systemd for the initial desktop integration
- polkit for narrowly scoped privileged operations
- Debian 13 and Fedora package validation

## Product boundary

zapret2control is a desktop control application for upstream zapret2. It must use upstream-supported scripts and documented behavior rather than duplicating installation, removal, diagnostics, firewall setup, or service orchestration.

## Change control

Any future expansion beyond Linux-only or addition of another Linux init system/package target requires an explicit product-scope decision and corresponding compatibility tests.
