# 0008 — Node is a test launcher, never a script dependency

Status: Accepted (F1; change `2026-09-19-f1-player-skeleton`)

## Context

The hard rule "game scripts have zero browser/Node dependencies, not
even transitively" binds shipped scripts. But the Emscripten build's
`--script` mode emits JS glue + wasm that runs under plain Node.js, and
the CI gate (ADR 0009) must drive it. Without a clarification the two
policies look contradictory.

## Decision

The no-Node rule binds **game scripts** (pure ES6) and the shipped
player. The *test harness* may use Node purely as a launcher —
equivalent to a shell — to execute the wasm player's `--script` mode.
`ctest` drives all four targets; the Emscripten test wraps the
invocation in Node.

## Consequences

- Game scripts may never `require`/`import` Node or browser APIs, even
  transitively — the engine compiles no quickjs-libc (ADR 0002) to keep
  it enforceable.
- Test authors may use Node tooling freely, without a new ADR.

## Rejected alternatives

- Headless Chromium via Playwright: heavier; kept only as the documented
  fallback if Node-run proves unworkable.
