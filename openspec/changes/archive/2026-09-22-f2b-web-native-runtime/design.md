# Design

## Context

`f2-2d-layer` added a frame-end `JS_RunGC` to the player frame loop. In
the browser build that call deterministically segfaults (first collection,
even for trivial scripts; traces and forensic notes in
`f2-2d-layer/tasks.md`). The embedded interpreter provides no value on the
browser platform — vision.md explicitly wants the browser's native JS
engine to use the API through an Emscripten bridge. This change stacks on
`f2a-sokol-shdc` (native canned shaders) and completes F2.

Decision record: `docs/decisions/0022-quickjs-desktop-only.md` (written
during apply).

## Goals / Non-Goals

**Goals:**

- Web player = compiled C core + browser JS engine; no quickjs in the
  wasm; the JS_RunGC crash class structurally eliminated on web.
- Same `efx` namespace contract for browser code as for desktop scripts
  (same names, signatures, semantics, resource lifecycle).
- Web golden gate green and enforceable (ADR 0020 tolerance vs the
  canonical llvmpipe captures).

**Non-Goals:**

- Desktop runtime changes (quickjs stays on Win/Linux/macOS).
- `--script` mode on the web (no CLI in a browser); Node smoke tests keep
  using the desktop code path and the existing Emscripten Node harness for
  headless suites.
- ES-module multi-file game scripts; REPL (F6).

## Decisions

### D1 — Compile-time platform split, not a runtime flag

The Emscripten targets stop linking `runtime.c`/`api.c`'s quickjs
bindings entirely (CMake excludes the interpreter from web targets), and
a new `src/web/` module provides: (a) `bridge.c` — the export table
mapping the same `efx_*` C functions to JS-visible exports with thin
argument converters, and (b) `entry.js` glue — fetch/execute `main.js`
from the preloaded resource root, read the global `update`/`render` (plus
`registerUpdateHook`/`registerRenderHook` once ADR 0016 lands), dispatch
frames, and translate uncaught errors to the failure exit path.
`player.c`'s windowed flow (pick hooks, frame loop, collect, quit codes)
is mirrored by the bridge; `runtime.c` and the quickjs link become
desktop-only.

*Rejected:* a runtime "backend" flag inside the existing runtime module —
keeps the interpreter and its heap in the wasm and preserves the crash
class. *Rejected:* keeping a stripped interpreter "just in case" — two
JS dialects to test forever, for zero delivered value.

### D2 — The bridge mirrors the binding contract, mechanically

`bridge.c` exports one function per `efx` C function with the same name
(`efx.log`, `efx.setClearColor`, `efx.drawQuad`, …). String/array/marshal
points follow the existing binding code; resource objects become small JS
objects wrapping integer handles with `destroy()` (the native handle
table, deferred release sweep, and budget checks already live in C).
`efx_whiteTexture()` returns a handle-backed object. The `js-api`
reference marks the C layer `[C · bridge]` on web and `[C · quickjs]` on
desktop with identical semantics.

### D3 — Entry script loading on web

`entry.js` fetches `<root>/main.js` (resource root preloaded via
`--preload-file`), executes it as a classic script in the page (the same
global-object contract as desktop: `update`/`render` globals, top-level
setup runs once), then starts the frame loop. Uncaught errors surface via
`window.onerror`/hook try/catch → the same failure exit contract. The
high-level JS layer ships bundled and is imported by game scripts the
same way on both platforms.

### D4 — GC story on web

No embedded interpreter → no embedded GC exists to crash. The frame-end
native release sweep stays in C (already implemented in the render
module). The `EFX_WEB_GOLDEN` collect skip from `f2-2d-layer` is removed
by this change (nothing to collect on web); the segfault investigation
findings stay recorded in `f2-2d-layer/tasks.md` as the rationale.

## Risks / Trade-offs

- [Two runtime implementations to keep behaviorally identical] → the
  contract is pinned by the same spec scenarios and the same portable
  smoke scripts run on both; the js-api reference documents per-platform
  binding tags.
- [Browser script errors have no quickjs stack traces] → browser engines
  provide richer diagnostics; the error contract (message + failure exit)
  is preserved.
- [Bridge surface drift vs desktop bindings] → one shared C function per
  feature consumed by both paths; a smoke-test matrix (same scripts, both
  runtimes) guards drift and already exists in the suite.

## Migration Plan

Land the bridge behind the existing Emscripten targets; the web golden
job flips green or the change is not merged. Desktop behavior is
untouched (same binaries, same tests). Rollback = revert.

## Addendum (apply)

Decisions settled while implementing, recorded for the archived record:

- **Golden capture mode is a runtime flag, not a compile-time define.**
  `EFX_WEB_GOLDEN` is defined on the `player_web_golden` target, but the
  root/capture selection lives in `src/web/bridge.c`, which compiles once
  into `efx_core`. `src/web/web_main.c` (per-target) calls
  `efx_web_set_golden_mode()` before `efx_web_main`, so the shared core
  carries the mode at runtime.
- **The sokol loop starts after `main.js` evaluates** (desktop ordering:
  script before window/GL). `entry.js` boots from `postRun`, and only a
  successful boot calls `efx_web_start_loop()`; an eval-time `quit()` or
  a missing entry script therefore never opens a frame loop. On web the
  full render-stack teardown moved into the sokol cleanup callback
  (`src/platform/platform.c`), since no C caller resumes after
  `sapp_run` returns.
- **Game scripts get a portable environment.** The entry script is
  evaluated through a wrapper whose parameters shadow `window`,
  `document`, `require`, `process`, `fetch`, `XMLHttpRequest`, `module`,
  `exports`, `Buffer`, `global`, plus a `globalThis` Proxy that denies
  those names — so `s_portable.js` and the js-api "no host
  dependencies" rule hold identically on desktop and web. `efx` is
  passed in as a parameter and also published on the real global.
- **`_malloc`/`_free` are exported** on web targets: Emscripten's
  `emscripten_run_script_string` helper (used for query parsing) needs
  `_free`, and the bridge's ImageData scratch buffer needs `_malloc`.
- **`efx_api_tests` is desktop-only.** "No quickjs in any Emscripten
  target" is enforced for the shipped web player artifacts
  (`player`/`player_web`/`player_web_golden`, verified by an nm check
  with zero quickjs symbols). The quickjs-binding unit test binary stays
  a desktop test; web API parity is covered by the host-engine ctest
  suite, the browser harness and `tools/run_web_compare.mjs`.
- **Explicit hook registration (ADR 0016) is not delivered here.** The
  bridge wires the global `update`/`render` hooks exactly like the
  desktop runtime; `registerUpdateHook`/`registerRenderHook` do not
  exist on either binding yet (docs/js-api.md still marks them as the
  target contract), and the task's parenthetical ("plus registration
  hooks") stays future runtime work so both runtimes remain identical.
- **`--script` mode stays desktop-only** (per non-goals); the web player
  runs resource-root mode only, and the Node-hosted smoke suite stages
  each script as a root's `main.js`.

## Open Questions

None.
