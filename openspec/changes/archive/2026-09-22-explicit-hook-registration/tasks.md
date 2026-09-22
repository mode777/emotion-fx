# Tasks

## 1. Desktop runtime — explicit registration

- [x] 1.1 Add ordered, mark-inactive hook lists to `struct efx_host_state` in `src/runtime/runtime_internal.h` (update and render), with append / mark-inactive helpers, and free every stored `JSValue` in `efx_runtime_destroy` before `JS_FreeContext`; verify with a build (`cmake --build build`) and a quickjs runtime create/destroy under `efx_api_tests`
- [x] 1.2 Implement `efx_js_registerUpdateHook` / `efx_js_registerRenderHook` in `src/api/api.c` (declared in `src/api/api.h`): `TypeError` on non-function, append a duplicated reference, return a `JS_NewCFunctionData` unsubscribe closure (list kind via `magic`, entry index via `func_data[0]`) whose repeated calls are no-ops; register both on the `efx` object in `efx_runtime_new` (`src/runtime/runtime.c`)
- [x] 1.3 Rework `efx_runtime_call_hook(rt, which, dt)` (`src/runtime/runtime.h` + `.c`) to run all active hooks of the kind in registration order, passing `dt` to update hooks and no arguments to render hooks, stopping at the first quit request or exception; update `efx_runtime_pick_hooks` to register function-valued global `update`/`render` as load-time sugar in load order (guarded against double registration) and report whether any hook exists

## 2. Frame timing through the platform layer

- [x] 2.1 Widen `efx_frame_hooks.on_frame` to `int (*)(void *ud, double dt)` in `src/platform/platform.h` and pass `dt` from `efx_frame_cb` in `src/platform/platform.c` (`0.0` on the first frame, `sapp_frame_duration()` afterwards); verify all `on_frame` call sites compile
- [x] 2.2 Update `efx_player_frame` (`src/player/player.h` + `src/player/player.c`) to accept and forward `dt`, and update `tests/dev_harness.c` to pass a fixed `1.0/60.0`; verify `EFX_BUILD_DEV_HARNESS=ON` builds and the existing `hooks_order_and_quit` harness test still passes

## 3. Web runtime parity

- [x] 3.1 In `src/web/entry.js`, add `registerUpdateHook` / `registerRenderHook` to the `efx` object with update/render hook arrays, an idempotent unsubscribe closure, and a `dt`-aware `st.dispatch(which, dt)` that passes `dt` to update hooks only; register the global `update`/`render` returned by the `main.js` epilogue as load-time sugar instead of storing them as single hooks
- [x] 3.2 In `src/web/bridge.c`, pass `dt` through `efx_web_call_hook_js(which, dt)` and compute `dt` for the direct Node harness path in `efx_bridge_frame` (via `emscripten_get_now()`, `0.0` on its first frame); verify the Emscripten build succeeds

## 4. Tests

- [x] 4.1 Add `efx_api_tests` case(s) in `tests/unit/api_tests.c` that evaluate a script registering multiple hooks, drive `efx_runtime_call_hook` from C, and assert registration order, `dt` being a finite number, idempotent unsubscribe, global-sugar registration, and `TypeError` on a non-function; wire the new case names into the `foreach(CASE ...)` list in `tests/CMakeLists.txt` and verify it passes headless
- [x] 4.2 Add a web fixture root `tests/fixtures/web/root_explicit_hooks/main.js` exercising stacking + unsubscribe + `dt`, add an `add_web_test` case in `tests/CMakeLists.txt`, and add a matching scenario to `tools/run_web_harness.mjs`; verify under Node (Emscripten build) and, where a browser is pinned, headless Chrome
- [x] 4.3 Add dev-harness scripts under `tests/scripts/` (explicit hooks order/quit and unsubscribe) and `add_harness_test` cases in `tests/CMakeLists.txt`; verify with `EFX_BUILD_DEV_HARNESS=ON`
- [x] 4.4 Update `examples/hello/main.js` to demonstrate explicit registration (or add a short explicit-registration sample) while leaving one global-hook example in place; verify it still runs headless via the resource-root smoke path

## 5. Docs and ADR

- [x] 5.1 Rewrite the Lifecycle hooks section of `docs/js-api.md` as current behavior (registration, stacking order, `dt`, unsubscribe, globals as load-time sugar), update the F1 catalog entry and the F2–F8 sample comments, remove the "Hook registration + `dt` delivery" open question, update the vision traceability row, and refresh the status header; verify every `register*` entry is tagged F1 and not provisional
- [x] 5.2 Amend `docs/decisions/0016-explicit-hook-registration-implicit-init.md` so the readiness guarantee is scoped to the script-visible `efx` namespace/API and the GL-context reordering is recorded as deferred (no new ADR; the decision itself is unchanged), and confirm the `docs/decisions/README.md` index row still describes it
- [x] 5.3 Update the `AGENTS.md` current-state script-facing API line to include `efx.registerUpdateHook` / `efx.registerRenderHook`, and confirm `docs/js-api.md` remains the single catalog pointer

## 6. Verification

- [x] 6.1 Run the full local ctest suite (smoke + `efx_render_tests` + `efx_api_tests`) and confirm the existing F1/F2 smoke tests, the six golden scenes (where a display exists, `-DEFX_BUILD_GOLDEN_TESTS=ON`), and the new hook tests all pass, proving globals-as-sugar did not change output
- [x] 6.2 Build and run the Emscripten target's web tests under Node plus `tools/run_web_compare.mjs`, confirming desktop/web parity for the portable scripts and the new explicit-hook fixture
- [x] 6.3 Per AGENTS.md/ADR 0023, trigger a manual CI run (`gh workflow run ci.yml`) and verify the four-target matrix — Linux first, then Windows, then macOS — is green before archive
