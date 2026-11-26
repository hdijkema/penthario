---
title: macOS packaging
description: "Install and package the Apple Silicon macOS DMG for penthario, including signing and validation notes."
---

# macOS packaging

The native workflow builds an Apple Silicon arm64 application for macOS 15 or
newer. Intel/universal builds are not configured. Homebrew supplies libpq@18,
Ninja and dylibbundler.

`tools/build-macos.sh` builds Release, runs native tests and invokes
`tools/package-macos.py`. The packager stages penthario.app, PostgreSQL client
tools, notices and dependent dylibs; rewrites dependencies into the bundle;
audits executable/library imports and rejects external development
paths; and ad-hoc signs the result. It creates a DMG, mounts it, and performs
isolated startup/client-tool checks before publishing the artifact.

The DMG includes an Applications shortcut. Drag the application there to install.
The package currently has no Developer ID signature or Apple notarisation;
Gatekeeper policy can require explicit user approval. Ad-hoc signing is not
notarisation. Release signing needs an Apple developer identity and CI secrets
and is outside this migration.

Dependency-audit tests run on Windows too, but actual bundle relocation, signing,
DMG mounting and native GUI behavior require macOS. On 26 November 2026, Hans
reported that the macOS build worked out of the box on his MacBook Air M1.
This confirms a real-device check on Apple Silicon; the exact application and
macOS versions and individual feature checks were not recorded. See the
[validation report](VALIDATION.md) for the evidence and remaining limits.
