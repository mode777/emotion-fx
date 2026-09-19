# Tasks

Implements F1 (player skeleton). Decisions D1–D7 refer to design.md; behavior
requirements live in the four delta specs (build-system, player-runtime,
js-runtime, verification).

## 1. Vendoring and build skeleton

- [x] 1.1 Vendor pinned sokol sources into `vendor/sokol/` (only the headers F1 needs: `sokol_app.h` plus its implementation dependencies) and create `vendor/README.md` recording source repo + pinned version; verify the pinned commit/release hashes are listed and headers compile in a trivial TU
- [x] 1.2 Vendor a pinned quickjs-ng release snapshot into `vendor/quickjs-ng/` and record it in `vendor/README.md`; verify the snapshot's version string matches the README entry
- [x] 1.3 Create root `CMakeLists.txt` and the `src/` module layout from D1 (`player`, `platform`, `runtime`, `api`; core static lib + thin `main.c`), with vendor warnings suppressed and warnings-as-errors on core code; verify `cmake` configure + build on the host produces a `player` binary that prints usage and exits non-zero when run with no arguments

## 2. JS runtime core (runtime + api modules)

- [x] 2.1 Implement quickjs-ng context lifecycle and registration of the single global `efx` namespace object (D2); verify via a temporary C harness that evaluates `typeof efx` and gets `"object"` back
- [x] 2.2 Implement `efx.log(msg)` in the api module writing to stdout with a trailing flush; verify via the temporary harness that a JS log call appears on stdout
- [x] 2.3 Implement `efx.quit(code)` recording the pending exit code and stopping script execution via an internal sentinel (not a user-visible error); verify via the temporary harness that a script calling `efx.quit(3)` yields pending code 3 and no error state
- [x] 2.4 Implement `efx.args()` returning the host-provided script arguments as a JS array; verify via the temporary harness with sample host args
- [x] 2.5 Implement uncaught-exception handling in the runtime module: dump message + stack to stderr and expose a distinct error state (sentinel quit excluded); verify via the temporary harness that a throwing script produces stderr output and error state

## 3. Run modes and player behavior

- [x] 3.1 Implement CLI parsing and run-mode selection in the player module with the exit-code contract (0 success, 1 generic failure, else script-requested code); verify usage/missing-argument invocations exit non-zero with a stderr diagnostic
- [x] 3.2 Implement `--script <file> [args…]` headless mode: no sokol initialization, evaluate, propagate quit code; verify with a scratch script that `efx.quit(3)` makes the process exit 3, and a missing file exits non-zero with a stderr message
- [x] 3.3 Implement resource-root mode script loading: resolve the root directory, require `main.js` (diagnostic + non-zero if absent), evaluate it once; verify with a fixture directory containing a trivial `main.js` (loads and exits 0 via `efx.quit(0)`) and a fixture without `main.js` (non-zero + stderr)
- [ ] 3.4 Implement the platform module: sokol window + clear-color pass + frame callback, linked only into resource-root mode (D3/D7); verify on the host that launching the fixture root opens a window with the clear color and closes cleanly with exit 0
- [x] 3.5 Implement hook pickup and dispatch: look up `update`/`render` once after evaluation, call them per frame in order, skip missing hooks, and on an uncaught hook exception stop the loop, print to stderr, exit non-zero; verify on the host with a scratch main.js defining both hooks plus a throwing-hooks variant

## 4. Emscripten target

- [x] 4.1 Add the Emscripten build (emcmake toolchain, `.js` + `.wasm` output, single player artifact per build-system spec) using the same core sources; verify the emscripten CI job configures and builds (local container has no emsdk)
- [x] 4.2 Verify the Emscripten `--script` mode runs headless under Node with exit codes propagating (D4); verify the emscripten CI smoke-test job passes the quit(3) and throwing-script tests

## 5. Smoke-test suite and harness

- [x] 5.1 Create `tests/` with the ctest harness: native tests invoke the player directly, Emscripten tests wrap the output in Node (D4); verify `ctest` runs on the host with one placeholder test
- [x] 5.2 Write the F1 smoke scripts per the verification spec: `efx.log` stdout crossing, `efx.quit(n)` exit-code propagation, `efx.args` host-argument reading, a portable ES6-only script (no browser/Node globals) that passes on all targets, uncaught-exception cases (script mode + fixture), missing-entry-script and missing-`--script`-file cases; verify `ctest` passes all of them on the host
- [x] 5.3 Verify failure reporting: temporarily force one test to fail and confirm the suite exits non-zero and names the failing test, then restore it; verify the forced-failure run output

## 6. Example and docs

- [ ] 6.1 Create `examples/hello/` resource root with a `main.js` defining `update`/`render` (window + clear color demo, no drawing API); verify it runs manually on the host and exits 0 on window close
- [x] 6.2 Document per-target build commands and the F1 gate procedure (build matrix, `ctest`, manual window checklist) in the repo README; verify the documented commands match what tasks 1–5 actually used

## 7. F1 gate (broader verification, executed via CI per D8)

- [x] 7.1 Build matrix: the CI workflow builds the player on Windows, Linux, macOS, and Emscripten; verify all four CI jobs produce a runnable player artifact per the build-system spec
- [x] 7.2 Run the full smoke suite via `ctest` in CI on all four targets; verify zero failures per the verification spec's four-target gate
- [ ] 7.3 Execute the manual window checklist on each desktop platform (window opens, hooks run per frame, missing hook tolerated, clean exit 0 on close); verify results are recorded
- [ ] 7.4 After 7.1–7.3 pass, flip the F1 row to done in the AGENTS.md roadmap status table; verify the table matches reality

## 8. GitHub Actions gate runner (added during apply, per D8)

- [x] 8.1 Author `.github/workflows/ci.yml`: native matrix (ubuntu/windows/macos) + Emscripten job, each configure → build → ctest; verify the workflow file is valid YAML and triggers on push to main
- [x] 8.2 Push and confirm a fully green run: all four jobs build and pass the smoke suite; verify via the Actions run summary (https://github.com/mode777/emotion-fx/actions/runs/35457715867)
