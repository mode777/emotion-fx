# 0002 — quickjs-ng is the embedded ES6 runtime, without quickjs-libc

Status: Accepted (F1; change `2026-09-19-f1-player-skeleton`)

## Context

`vision.md` names QuickJS as the ES6 runtime but leaves the flavor open;
it was F1's deferred decision. The runtime must build on all four
targets — including Emscripten — with CMake, and reference knowledge
(sokol-samples, rayjs) assumes bellard's quickjs C API.

## Decision

The engine embeds **quickjs-ng** (pinned release; see
`vendor/README.md`), consumed via its own CMake static-lib target with
its tests/examples/CLI/install disabled. quickjs-libc is **not** compiled
into the runtime: scripts get only the engine `efx` API (ADR 0004) plus
the ES6 standard library. All interpreter calls live in the `runtime`
module (ADR 0003).

## Consequences

- API drift vs bellard's quickjs is contained inside `runtime`; a swap
  stays localized.
- Pin bumps are manual (ADR 0006).
- The sandbox is structural: no `std`/`os` host modules exist to
  accidentally depend on, keeping scripts free of browser/Node APIs
  (ADR 0008).

## Rejected alternatives

- bellard's quickjs: most reference knowledge transfers either way, but
  quickjs-ng is actively maintained, builds CMake-native, and builds
  under Emscripten.
