# 0022 — quickjs never ships to the browser; the browser is the JS runtime

Status: Accepted (2026-09, `f2b-web-native-runtime`)

## Context

`f2-2d-layer` added a frame-end `JS_RunGC` to the player loop; in the
browser build that call deterministically segfaults (first collection,
even for a trivial script — forensic trail in
`openspec/changes/f2-2d-layer/tasks.md`). The embedded interpreter buys
nothing on the browser platform — the page already *is* an ES6 engine —
while costing the segfault, a larger wasm, and a second JS dialect
boundary. `vision.md` asks for the opposite: an Emscripten bridge so the
browser's native JS engine drives the API.

## Decision

On Emscripten, quickjs is not linked at all. The web player is the C core
plus a native bridge:

- `src/web/bridge.c` exposes every C-implemented `efx` function to the
  host engine with the same name, signature, semantics and error
  behaviour as the desktop binding (`efx_bridge_*` exports; resource
  wrappers around native handles; the frame-end native release sweep and
  budget checks stay in C).
- `src/web/entry.js` (pre/post-js glue) loads `<root>/main.js` with the
  host engine, wires the global `update`/`render` hooks once per frame
  (`update` before `render`), and mirrors the desktop error/exit
  contract (uncaught error → stop + failure code; missing `main.js` →
  diagnostic + error code).
- Game scripts run with the host globals (`window`, `document`,
  `process`, …) shadowed, so the "no browser/Node dependencies" contract
  is enforced by construction and the same script text runs on both
  runtimes.
- quickjs remains the desktop interpreter (ADR 0002); `src/api/` and
  `src/runtime/` are desktop-only compile units.

## Consequences

- The `JS_RunGC` crash class is structurally absent on web: no embedded
  interpreter, no embedded GC, zero `JS_RunGC` calls (verified by an nm
  check on the web player wasm: no quickjs symbols).
- Two runtime implementations must stay behaviourally identical. They
  share the whole C core (`src/render/`) and are pinned by the same spec
  scenarios, the same portable smoke scripts run on both runtimes
  (ctest matrix + `tools/run_web_compare.mjs`), and identical
  expectations.
- `efx_api_tests` (quickjs bindings + mock sink) is desktop-only; web
  API parity is covered by the host-engine smoke suite, the browser
  harness, and the cross-runtime comparison.
- The web player has no `--script` mode (a browser has no CLI); the
  desktop-style run mode stays desktop-only.
- The web golden gate is enforceable (ADR 0020 tolerance against the
  canonical llvmpipe captures).

## Rejected alternatives

- **Runtime backend flag inside the runtime module** — keeps the
  interpreter and its heap in the wasm and preserves the crash class.
- **Keep a stripped interpreter "just in case"** — two JS dialects to
  test forever, for zero delivered value.
- **Sandbox scripts by deleting host globals** — deleting `window`/
  `document` breaks the engine runtime itself; per-script shadowing via
  the wrapper scope achieves the contract without touching the page.
