# Proposal

> **STATUS: PROPOSED.** Stacks on `f2a-sokol-shdc`, which stacks on
> `f2-2d-layer` (incomplete/blocked). The three changes together complete
> milestone F2.

**Roadmap position:** Implements milestone **F2 (2D layer)** — the
browser-side runtime architecture that F2's web verification gate depends
on. Vision.md explicitly calls for it: "Comes with a emscripten javascript
bridge so the browsers native js engine can use the API."

## Why

`f2-2d-layer`'s frame loop runs a frame-end garbage collection over the
embedded quickjs heap. In the browser build, `JS_RunGC` deterministically
segfaults when called from the frame callback — first collection, even
with a trivial script (full forensic trail in `f2-2d-layer/design.md` and
the task notes). The embedded interpreter buys nothing on the browser
platform — the browser *is* an ES6 engine — while costing the segfault, a
larger wasm, and a second JS dialect boundary. Removing quickjs from the
browser platform eliminates the crash class entirely and delivers the
native-JS bridge the vision asks for.

## What Changes

- **Remove quickjs from all Emscripten builds.** The web player compiles
  without the `runtime` and `api` quickjs bindings; the embedded
  interpreter remains desktop-only.
- **Add a native-JS bridge module** (Emscripten export layer) that exposes
  the same `efx` namespace contract to browser code: the C-implemented
  functions (`log`, `quit`, `args`, `setClearColor`, `setCamera2D`,
  `createImageData`, `createTexture`, `drawQuad`, `setBlendMode`,
  `whiteTexture`) become callable from browser JS through the same
  signatures and semantics — the "same high level API" per the original
  vision.
- **Browser-side entry script loading:** in resource-root mode the web
  player fetches and executes `main.js` with the browser's own engine,
  wires the same global `update`/`render` lifecycle hooks (plus explicit
  hook registration per ADR 0016), and surfaces uncaught script errors
  through the same exit-code/`onerror` contract.
- **Resource objects on web:** `createImageData`/`createTexture` return
  JS objects wrapping native handles with the same `destroy()` semantics;
  the frame-end native sweep (deferred releases) already lives in C and
  continues unchanged — the browser's GC now manages the JS side, and no
  embedded-interpreter GC exists to crash.
- **High-level JS layer unchanged:** engine-bundled pure-JS conveniences
  run identically on the browser engine (they never used runtime internals).
- **Web golden gate becomes enforceable:** the `golden tests (emscripten)`
  job is fixed by this change and must pass (six goldens under the ADR
  0020 tolerance against the canonical llvmpipe captures).
- **Docs:** `docs/js-api.md` gains a web-binding note per entry (browser
  calls bridge functions directly; scripts use the same namespace);
  AGENTS.md stack note updated; **new ADR `docs/decisions/0022`** —
  "quickjs never ships to the browser; the browser is the JS runtime" —
  written during apply.

**Dependency evaluation:** no new third-party dependency. The bridge uses
the Emscripten runtime already vendored/built (exports + `cwrap`-style
binding); quickjs is *removed* from the web link, shrinking the wasm.

Non-goals: desktop runtime changes (quickjs stays embedded on Win/Linux/
macOS); `--script` mode on web (a browser has no CLI; smoke tests keep
running under Node via the desktop code path — Emscripten builds keep
their Node test harness unchanged); the REPL (F6); multi-file ES module
loading for game scripts (games remain single-`main.js` + bundled
high-level layer).

## Capabilities

### Modified Capabilities

- `js-runtime`: re-scopes the bundled-interpreter requirement — quickjs is
  the desktop interpreter; on Emscripten the page's JS engine drives the
  core directly through the bridge (the existing requirement already
  gestures at this; it is now normative and the quickjs-in-wasm path is
  removed) — plus a new requirement for the bridge's binding contract.
- `player-runtime`: adds the web entry-script requirement — browser
  execution of `main.js`, global hooks, and error surfacing through the
  same exit-code contract.

### New Capabilities

None. (The bridge is part of `js-runtime`.)

## Impact

- **Code:** CMake Emscripten targets drop the quickjs link; new
  `src/web/bridge.c` (export table) + `src/web/entry.js` glue (script
  loading, hook wiring, error contract); `player.c`/`runtime.c` become
  desktop-only compile units for windowed builds.
- **APIs:** `docs/js-api.md` documents that browser hosts call the same
  functions natively; no signature changes.
- **Verification:** the `golden tests (emscripten)` job becomes green and
  is a required part of the F2 gate (ADR 0020 tolerance against the
  canonical llvmpipe captures); the JS_RunGC segfault class is
  structurally eliminated on web.
- **Roadmap:** F6's REPL and desktop run modes are untouched; the vision's
  native-JS-bridge property is delivered.
