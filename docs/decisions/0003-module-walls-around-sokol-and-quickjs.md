# 0003 — Sokol and quickjs live behind module walls

Status: Accepted (F1; change `2026-09-19-f1-player-skeleton`)

## Context

F1 lays the skeleton every milestone F2–F8 builds on. If the two foreign
libraries — rendering (sokol) and scripting (quickjs) — are reachable
from anywhere, later strategy changes (F4's canned-shader decision, a
hypothetical interpreter swap) would cut across the whole tree.

## Decision

`src/` is one core static library plus a thin `main.c`, with four
internal modules:

- `platform` — sokol wrapper: window creation, frame callback, clear
  pass. The **only** module that may include sokol headers, compiled
  per-platform via sokol's implementation-macro pattern in one TU.
- `runtime` — JS engine host: context create/destroy, global-namespace
  registration, script evaluation, hook pickup/dispatch, error
  extraction. The **only** module that includes quickjs headers.
- `api` — the engine functions exposed to JS; pure C, no quickjs types
  past thin trampolines.
- `player` — CLI parsing, run-mode selection (ADR 0007), exit-code
  plumbing.

## Consequences

- A canned-shader strategy change (F4) or an interpreter swap touches
  one module each.
- Rendering (F2+) enters through `platform`'s wrapper surface, not raw
  sokol calls scattered through feature code.

## Rejected alternatives

- One flat `main.c`: rejected — F2+ would tangle immediately.
