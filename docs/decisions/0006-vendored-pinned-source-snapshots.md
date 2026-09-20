# 0006 — Dependencies are pinned source snapshots vendored in-repo

Status: Accepted (F1; change `2026-09-19-f1-player-skeleton`)

## Context

The player builds on four toolchains, including CI runners and machines
that must build offline; sokol is a rolling project without release
tags, quickjs-ng releases as tarballs. The build must never touch the
network at configure or build time.

## Decision

Third-party dependencies are committed under `vendor/` at pinned
versions, and `vendor/README.md` records source and pin per dependency
(sokol: master commit SHA, only the headers a milestone needs;
quickjs-ng: release tarball). No FetchContent, no submodules, no
downloads at configure/build time. Pins move by replacing the snapshot
and updating the table. A recorded future dependency (GLM, ADR 0005) is
deliberately not vendored until first use.

## Consequences

- Every build is reproducible from a fresh clone, offline.
- Version bumps are manual — accepted at this dependency count; revisit
  via a proposal if the count grows.
- `vendor/` changes are reviewable like any other code diff.

## Rejected alternatives

- CMake FetchContent: violates the offline-build requirement.
- Git submodules: an extra setup step and a recurring source of agent
  friction.
