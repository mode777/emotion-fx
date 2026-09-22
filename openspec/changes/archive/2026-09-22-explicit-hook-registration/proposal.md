# Proposal

**Roadmap position:** Implements **F1 (player skeleton)** — specifically the
lifecycle-hook contract owned by F1's `player-runtime` capability. F1 shipped
the frame loop with global `update`/`render` functions only; ADR 0016 later
made explicit, stacking hook registration the normative lifecycle API, but the
runtime never delivered it. This is a follow-up completing F1's contract, in
the same spirit as `f2a`/`f2b` completing F2. It adds no new milestone and
gates no predecessor (F1 and F2 are done).

## Why

The `js-api` spec and `docs/js-api.md` already describe explicit hook
registration (`efx.registerUpdateHook` / `efx.registerRenderHook`) as the
normative lifecycle model, with the F1 globals framed as load-time sugar kept
only for compatibility. ADR 0016 records that decision. But the runtime still
implements *only* the globals: there is no registration function on either
binding, no stacking, no unsubscribe, and update hooks receive no `dt`. The
documented contract and the shipped behavior disagree, and the reference marks
the target model "provisional" while the compatibility path is the only thing
that actually works. This change makes the documented model real, so scripts,
the bundled high-level layer, and the future F6 REPL can register independent
per-frame callbacks instead of coordinating on one global function.

## What Changes

- Add `efx.registerUpdateHook(fn)` and `efx.registerRenderHook(fn)` to the
  `efx` namespace on **both** bindings (desktop quickjs and the Emscripten
  native bridge), with identical semantics.
  - Hooks stack and run in registration order: all update hooks (each
    receiving `dt`, seconds since the previous frame) before all render hooks,
    once per frame.
  - Each registration returns an unsubscribe function; calling it removes that
    registration and is idempotent.
  - A non-function argument throws `TypeError`; an exception in any hook stops
    the run with the existing non-zero exit-code contract.
- Keep the F1 global `update`/`render` functions as **load-time sugar**: if
  defined when `main.js` finishes evaluating, they are registered in load
  order (after any hooks registered during evaluation). Existing scripts,
  golden scenes, examples, and the smoke suite keep working; the only
  observable change is that the global `update` now receives `dt`.
- Thread `dt` from the platform frame loop into the runtime so update hooks can
  use frame time without scripts depending on host timing APIs.
- Update `docs/js-api.md` to present registration as current behavior (drop the
  "provisional / target contract" framing) and the globals as sugar; add the
  `js-api` and `player-runtime` spec deltas.
- Amend **ADR 0016** wording: the readiness guarantee is the script-visible
  `efx` namespace/API before `main.js` executes; moving window/GL-context
  creation ahead of evaluation is **not** part of this change and stays
  deferred.

**No new ADR** — this change implements the existing accepted decision
(ADR 0016); it only refines that record's readiness sentence and does not
settle a new cross-cutting trade-off. Affected docs: `docs/js-api.md` (API
delta, required) and `docs/decisions/0016-explicit-hook-registration-implicit-init.md`
(amended wording).

## Capabilities

### New Capabilities

None.

### Modified Capabilities

- `js-api`: add the public contract for explicit lifecycle hook registration
  (names, stacking order, `dt`, unsubscribe, sugar, errors) and correct the
  reference-document requirement's readiness wording to the script-visible
  API.
- `player-runtime`: replace the "globals only" lifecycle-hooks requirement
  with the explicit-registration model, keeping globals as load-time sugar and
  preserving the error/exit-code contract.

## Impact

- **Code:** `src/runtime/runtime.c`, `src/runtime/runtime.h`,
  `src/runtime/runtime_internal.h`, `src/api/api.c`, `src/api/api.h`
  (desktop registration + hook dispatch); `src/platform/platform.h`,
  `src/platform/platform.c` (`dt` through the frame callback);
  `src/player/player.c`, `src/player/player.h` (frame call signature);
  `src/web/entry.js`, `src/web/bridge.c` (web parity + `dt`).
- **Tests:** new headless `efx_api_tests` cases, a new web fixture + ctest
  case, dev-harness scripts, and web-harness scenario; existing smoke and
  golden suites must stay green (globals remain supported).
- **Docs:** `docs/js-api.md`, `docs/decisions/0016-...`, `AGENTS.md`
  current-state script-API line.
- **Risk:** low for behavior (additive; globals preserved); the main risk is
  web/desktop parity drift, guarded by the portable fixture and the existing
  cross-runtime comparison.

## Non-goals

- Moving window/GL-context creation ahead of `main.js` evaluation (the
  "readiness-before-load" reordering in ADR 0016). Deferred; the script-visible
  API is already ready before `main.js` runs.
- Removing or deprecating the global `update`/`render` functions — they remain
  load-time sugar.
- The F6 REPL, REPL session/reset semantics, or hook introspection; no
  `removeHook(id)`/ticket-based removal (ADR 0016 rejects it).
- Any rendering, resource, or milestone-behavior change beyond lifecycle
  dispatch.
