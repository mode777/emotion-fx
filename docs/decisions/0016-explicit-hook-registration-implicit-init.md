# 0016 — Lifecycle via explicit hook registration; loading main.js is the implicit init

Status: Accepted (2026-09, change `js-api-reference`)
Supersedes: the `init()` hook portion of the D9 sketch in
`openspec/changes/js-api-reference/design.md`

## Context

F1 picks global `update`/`render` functions once after script evaluation
(the player-runtime contract) and sketches an explicit `init()` hook as the
target contract. Two forces break that model: the **F6 REPL** drives the
same `efx` namespace interactively — global-function hooks need hook
re-pickup after every evaluation and cannot stack competing experiments —
and both the bundled high-level JS layer and multi-module resource roots
want independent per-frame callbacks without coordinating on one global
function. Separately, `init()` existed only because F1 evaluates `main.js`
*before* window/context creation; if the runtime instead guarantees full
readiness before script evaluation, top-level code *is* the init.

## Decision

- **`efx.registerUpdateHook(fn)` / `efx.registerRenderHook(fn)` are the
  normative lifecycle API.** `fn` is a plain ES6 callback; hooks stack and
  run in registration order — all update hooks (each receiving `dt`,
  seconds since the previous frame), then all render hooks, once per frame.
  Each registration returns an **unsubscribe function** — essential for
  REPL iteration, where re-registering would otherwise accumulate stale
  hooks.
- **Loading `main.js` is the implicit init.** Runtime contract: the engine
  is fully ready — window, GL context, the `efx` namespace, the bundled
  high-level layer — *before* `main.js` executes (F1's current order flips:
  context setup moves before script evaluation). Top-level code is setup;
  the separate `init()` hook is dropped as redundant.
- **F1's global `update`/`render` remain supported as load-time sugar**: if
  defined after evaluation, the engine registers them in load order. F1
  examples, the smoke suite, and the player-runtime gate contract stay
  valid unchanged.
- The REPL (F6) registers and unregisters through the same functions — no
  separate REPL lifecycle API.

## Consequences

- Player runtime: hook pickup becomes dynamic (a list, not a one-time
  property read); the readiness-before-eval ordering is a one-time runtime
  change delivered with the registration pair (F2 at the latest).
- Callback count is unbounded and order matters; an exception in any hook
  halts the run (existing error contract applies).
- REPL sessions that re-register accumulate hooks; unsubscribe (or session
  reset) is the documented remedy.
- Future `js-api` deltas model lifecycle as registration; examples and
  samples never define hook globals (except in the F1 current-behavior
  section).

## Rejected alternatives

- **Global-function hooks as the normative model**: lost — no stacking,
  no REPL story, forces modules to coordinate on a single global name.
- **Explicit `init()` hook**: lost — redundant once readiness-before-load
  is guaranteed; two setup places invite ordering confusion; the REPL has
  no init concept. (This reverses the earlier design-sketch rationale,
  which assumed F1's load-before-window order was fixed.)
- **Ticket/ID-based removal (`removeHook(id)`)**: heavier than the ES6-
  idiomatic unsubscribe return; revisit only if REPL feedback demands it.
