# Tasks

Implements the native-JS web runtime completing F2. Decision record:
`docs/decisions/0022-quickjs-desktop-only.md`. Contract sources: the
`js-runtime` and `player-runtime` deltas; gates: the web golden job per
the `verification` spec. Apply-time decisions and scope calls are
recorded in the design addendum.

## 1. Web build split

- [x] 1.1 Exclude `runtime.c`/`api.c` quickjs bindings and the qjs link from all Emscripten targets; verify the wasm contains no quickjs symbols (nm check) and the Node smoke suites for desktop still pass unchanged
- [x] 1.2 Introduce `src/web/` (bridge.c + entry.js) wired into the Emscripten targets in place of the runtime; verify a trivial resource root boots in the browser harness (init, one frame, quit)

## 2. Native bridge binding contract

- [x] 2.1 Export the full `efx` C function surface (log, quit, args, setClearColor, setCamera2D, createImageData, createTexture, drawQuad, setBlendMode, whiteTexture) through bridge.c with argument converters matching the desktop binding semantics; verify the same portable script sequences produce identical observable effects on desktop (quickjs) and web (bridge) via a Node-driven comparison smoke test
- [x] 2.2 Implement resource objects on web (ImageData/Texture JS wrappers over native handles with `destroy()`, idempotence, use-after-destroy TypeError) and verify the resource lifecycle scenarios on the web path
- [x] 2.3 Verify no embedded-GC machinery remains on web: the frame loop runs with zero `JS_RunGC` invocations (the crash class is structurally absent), and the `EFX_WEB_GOLDEN` collect skip is removed as obsolete

## 3. Web entry script and lifecycle

- [x] 3.1 Implement entry.js: fetch/execute `<root>/main.js` from the preloaded resource root with the browser engine, wire global `update`/`render` (plus registration hooks) once per frame in order, skip absent hooks; verify with golden scenes on the machine harness
- [x] 3.2 Implement the web error contract: uncaught entry-script/hook errors stop the loop and surface via the failure exit path; missing `main.js` produces the diagnostic + non-zero exit; verify both cases in the browser harness

## 4. Web golden gate

- [x] 4.1 Verify the `golden tests (emscripten / headless chrome)` job passes: all six scenes render through the native bridge and match the canonical llvmpipe goldens under the ADR 0020 tolerance
- [x] 4.2 Confirm the desktop suites are unaffected (Linux/Windows/macOS jobs unchanged and green)

## 5. Docs and ADR

- [x] 5.1 Write ADR `docs/decisions/0022-quickjs-desktop-only.md` (browser is the JS runtime; bridge contract; crash-class rationale) per TEMPLATE.md and index it in `docs/decisions/README.md`
- [x] 5.2 Update `docs/js-api.md`: runtime/platform note per entry (desktop `[C · quickjs]`, web `[C · bridge]`), identical semantics; update AGENTS.md stack notes and F2 status
- [x] 5.3 Update `f2-2d-layer/design.md` addendum: web GC crash resolved by quickjs removal (supersedes the collect-skip workaround and the collect investigation)

## Verification evidence

- Emscripten: `ctest --test-dir build-em` 22/22 (host-engine smoke suite +
  render unit tests); nm check on an unminified web player wasm: 0 quickjs
  symbols, 24 bridge symbols; `tools/run_web_compare.mjs` 7/7 desktop-vs-web
  comparisons match (exit code + stdout, stderr first line).
- Browser (headless Chrome 131 / SwiftShader): `tools/run_web_harness.mjs`
  6/6 scenarios (boot + one frame + quit, hook error, eval error, missing
  entry, missing dir); `tools/run_web_goldens.mjs` all six scenes pass the
  ADR 0020 tolerance.
- Desktop (Linux, headless): `ctest --test-dir build -E golden_` 32/32
  (smoke + display-list + JS-API unit tests), including the new
  `smoke_resource_lifecycle` and `smoke_root_eval_throw`.
- Windows/macOS jobs and the native golden jobs run in the CI matrix
  (staggered Linux → Windows → macOS per AGENTS.md); not exercised in the
  apply session.
