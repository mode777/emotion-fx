# Tasks

Implements the native-JS web runtime completing F2. Decision record:
`docs/decisions/0022` (write during apply, task 5.1). Contract sources:
the `js-runtime` and `player-runtime` deltas; gates: the web golden job
per the `verification` spec.

## 1. Web build split

- [ ] 1.1 Exclude `runtime.c`/`api.c` quickjs bindings and the qjs link from all Emscripten targets; verify the wasm contains no quickjs symbols (nm check) and the Node smoke suites for desktop still pass unchanged
- [ ] 1.2 Introduce `src/web/` (bridge.c + entry.js) wired into the Emscripten targets in place of the runtime; verify a trivial resource root boots in the browser harness (init, one frame, quit)

## 2. Native bridge binding contract

- [ ] 2.1 Export the full `efx` C function surface (log, quit, args, setClearColor, setCamera2D, createImageData, createTexture, drawQuad, setBlendMode, whiteTexture) through bridge.c with argument converters matching the desktop binding semantics; verify the same portable script sequences produce identical observable effects on desktop (quickjs) and web (bridge) via a Node-driven comparison smoke test
- [ ] 2.2 Implement resource objects on web (ImageData/Texture JS wrappers over native handles with `destroy()`, idempotence, use-after-destroy TypeError) and verify the resource lifecycle scenarios on the web path
- [ ] 2.3 Verify no embedded-GC machinery remains on web: the frame loop runs with zero `JS_RunGC` invocations (the crash class is structurally absent), and the `EFX_WEB_GOLDEN` collect skip is removed as obsolete

## 3. Web entry script and lifecycle

- [ ] 3.1 Implement entry.js: fetch/execute `<root>/main.js` from the preloaded resource root with the browser engine, wire global `update`/`render` (plus registration hooks) once per frame in order, skip absent hooks; verify with golden scenes on the machine harness
- [ ] 3.2 Implement the web error contract: uncaught entry-script/hook errors stop the loop and surface via the failure exit path; missing `main.js` produces the diagnostic + non-zero exit; verify both cases in the browser harness

## 4. Web golden gate

- [ ] 4.1 Verify the `golden tests (emscripten / headless chrome)` job passes: all six scenes render through the native bridge and match the canonical llvmpipe goldens under the ADR 0020 tolerance
- [ ] 4.2 Confirm the desktop suites are unaffected (Linux/Windows/macOS jobs unchanged and green)

## 5. Docs and ADR

- [ ] 5.1 Write ADR `docs/decisions/0022-quickjs-desktop-only.md` (browser is the JS runtime; bridge contract; crash-class rationale) per TEMPLATE.md and index it in `docs/decisions/README.md`
- [ ] 5.2 Update `docs/js-api.md`: runtime/platform note per entry (desktop `[C · quickjs]`, web `[C · bridge]`), identical semantics; update AGENTS.md stack notes and F2 status
- [ ] 5.3 Update `f2-2d-layer/design.md` addendum: web GC crash resolved by quickjs removal (supersedes the collect-skip workaround and the collect investigation)
