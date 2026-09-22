# Design

## Context

See `proposal.md` — Why. The relevant current state:

- **Desktop** (`src/runtime/runtime.c`): `efx_runtime` holds two `JSValue`
  globals (`hook_update`, `hook_render`) picked once by
  `efx_runtime_pick_hooks` after `main.js` evaluation. `efx_runtime_call_hook`
  calls at most one hook, with zero arguments. The `efx` function list is
  registered in `efx_runtime_new`; C implementations live in `src/api/api.c`
  and reach per-context state through `efx_host_state` (the quickjs context
  opaque, `src/runtime/runtime_internal.h`).
- **Web** (`src/web/entry.js` + `src/web/bridge.c`): the host JS engine drives
  the same surface; `entry.js` builds the `efx` object, evaluates `main.js`
  with `new Function`, stores the returned global hooks in `st.update` /
  `st.render`, and exposes `globalThis.__efxDispatchHook(which)`. The C side
  calls it through `efx_web_call_hook_js`. On the Node test path
  `efx_bridge_frame` drives frames directly.
- **Frame loop** (`src/platform/platform.c`): sokol's `frame_cb` calls
  `g_hooks.on_frame(ud)` — signature `int (*)(void*)` — which for desktop is
  `efx_player_frame` and for web is `web_frame`. Platform is the only module
  allowed to touch sokol (ADR 0003), so frame timing belongs here.
- **Tests**: headless `efx_api_tests` runs real quickjs against a mock GPU sink
  and can call runtime entry points from C; the dev harness
  (`tests/dev_harness.c`, `EFX_BUILD_DEV_HARNESS`) drives frames; web fixtures
  run under Node via `add_web_test` and in headless Chrome via
  `tools/run_web_harness.mjs`.
- ADR 0016 already fixes the semantics: registration normative, stacking in
  registration order, unsubscribe returned, `dt` on update hooks, globals as
  load-time sugar, REPL uses the same functions.

## Goals / Non-Goals

**Goals:**

- One registration contract, implemented identically on desktop and web:
  `efx.registerUpdateHook(fn)` / `efx.registerRenderHook(fn)`, stacking in
  registration order, `dt` for update hooks, idempotent unsubscribe, `TypeError`
  on non-functions, error/exit contract preserved.
- Globals remain load-time sugar so every existing script, golden scene,
  example, and smoke test keeps working.
- `dt` reaches hooks without scripts touching host timing APIs and without the
  runtime module depending on sokol.

**Non-Goals:**

- Reordering window/GL-context creation ahead of `main.js` evaluation.
- Hook introspection, named/ticket removal, or REPL session/reset semantics.
- Any change to rendering, resources, or other milestone behavior.

## Decisions

### D1: Hooks live in `efx_host_state` as ordered, mark-inactive lists

Add to `struct efx_host_state` a small dynamic list per kind:

```c
struct efx_hook_entry { JSValue fn; int active; };
struct efx_hook_list { struct efx_hook_entry *entries; int count, cap; };
```

Registration appends a `JS_DupValue`d function with `active = 1`. Dispatch walks
entries in order, skipping inactive ones. Unsubscribe sets `active = 0` and
releases the stored reference **at teardown**, not during dispatch: a hook may
unsubscribe itself while it is being called, and freeing the only reference to
the running function from inside its own invocation risks a use-after-free.
Entries are freed in `efx_runtime_destroy` before `JS_FreeContext`.

The lists are reached through the existing `host_state(ctx)` accessor, so the
`efx_js_*` functions in `src/api/api.c` stay where the rest of the surface
lives. No new native resource class is introduced: hook callbacks are
JS-managed values the runtime merely holds references to; unsubscribe drops the
reference, teardown releases the rest (the ADR 0011/0012 class/`destroy()`
discipline applies to native-backed resources, not to held JS callbacks). No
fixed bank is involved.

*Rejected:* a JS `Array` in the runtime (iteration + removal during dispatch is
awkward and still needs a C-side handle); a linked list (no benefit over the
array at expected hook counts); freeing entries eagerly on unsubscribe (the
self-unsubscribe hazard above).

### D2: Unsubscribe is a data-carrying C closure

Return `JS_NewCFunctionData` whose callback carries the list kind as `magic`
and the entry index as `func_data[0]`. Indices are stable because entries are
never compacted (D1). The callback marks the entry inactive; a stale/duplicate
call is a no-op. *Rejected:* `removeHook(id)`/tickets (ADR 0016 already rejects
the heavier API); a pure-JS closure wrapper (would require the web and desktop
bindings to diverge or add a JS helper layer the API does not need).

### D3: `dt` is computed in the platform layer and passed through the frame callback

Widen `efx_frame_hooks.on_frame` to `int (*)(void *ud, double dt)`. In
`efx_frame_cb`, `dt` is `0.0` on the first frame and `sapp_frame_duration()`
thereafter; the callback forwards it to the runtime. This keeps sokol usage
inside `src/platform/` (ADR 0003) and gives one deterministic first-frame value.

- Desktop: `efx_player_frame(void *ud, double dt)` →
  `efx_runtime_call_hook(rt, which, dt)`.
- Web (DOM): `web_frame(void *ud, double dt)` → `efx_web_call_hook_js(which,
  dt)`; `entry.js` dispatch passes `dt` to update hooks only.
- Web (Node harness): `efx_bridge_frame` computes `dt` with
  `emscripten_get_now()` (0 on its first frame) since it bypasses sokol.

*Rejected:* `sokol_time`/`sapp_*` inside the runtime (breaks the module wall);
`clock_gettime` in the runtime (not portable to Windows, and duplicates what
platform already owns).

### D4: Globals become load-time sugar, registered after evaluation

Desktop keeps `efx_runtime_pick_hooks`, but it now appends any function-valued
global `update`/`render` to the lists (in that order) and reports whether any
hook exists. It is guarded so a second call cannot double-register. Web does the
same in `__efxBoot` after evaluating `main.js`: the epilogue's global functions
are appended to the hook lists instead of stored as `st.update`/`st.render`.
Because evaluation finishes before sugar registration, explicitly registered
hooks run before the globals — exactly "a registration at the end of loading
`main.js`" (ADR 0016).

### D5: Web parity is explicit in `entry.js`, exercised by shared fixtures

`entry.js` grows `registerUpdateHook`/`registerRenderHook`, hook arrays, the
unsubscribe closure, and a `dt`-aware `st.dispatch`. The same portable fixture
runs under Node through `add_web_test` and under the desktop player through the
unit/dev tests, so the two bindings are checked against the same observable
order/`dt`/unsubscribe behavior rather than trusted to match.

### D6: Readiness is scoped to the script-visible API; ADR 0016 wording amended

The script-visible contract (the `efx` namespace, every function, resource
creation) already holds before `main.js` executes; only the internal GL-context
timing differs, and it is not observable at load time (draws are frame-transient
and belong in a render hook). Rather than restructure both platform loops to
create the GL context before evaluation, this change amends ADR 0016's
readiness sentence to scope the guarantee to the script-visible API and notes
the GL reordering as deferred. *Rejected for now:* moving context setup ahead of
evaluation on desktop (`sapp_run` is monolithic) and on web (boot/loop
restructuring) — high cross-target risk for no observable script benefit; can
be revisited if a future feature needs load-time GPU access.

## Risks / Trade-offs

- **Golden/smoke regressions from changing hook dispatch** → globals stay
  supported as sugar and all six golden scenes plus the existing smoke suite
  are re-run; no scene uses explicit hooks, so their output must not change.
- **Web/desktop behavior drift** → shared fixture + `add_web_test` under Node +
  `tools/run_web_harness.mjs` scenario; the Node path and DOM path share
  `entry.js` dispatch.
- **Self-unsubscribe during dispatch** → inactive-marking with teardown-time
  free (D1).
- **`dt` differs across runtimes** (sokol filtered duration vs
  `emscripten_get_now`) → tests assert `dt` is a finite number and first frame
  0, never exact durations.
- **Hook-list growth under repeated register/unsubscribe** (matters for the F6
  REPL) → inactive entries are skipped and freed at teardown; compaction can be
  added with the REPL if it proves necessary.
- **Function signature ripple** (`on_frame` gains `dt`, `efx_player_frame`
  gains `dt`) → small, mechanical; compiler catches every call site.

## Migration Plan

Additive and backward compatible: globals keep working, so no script, example,
golden, or test migration is required. Rollback is reverting the change; no
data or format migration exists. ADR 0016 is amended in place (not superseded)
because only its readiness sentence narrows.

## Open Questions

- Whether to compact inactive hook entries at frame end for long REPL sessions
  is deferred to F6, where the REPL actually creates the pressure; it does not
  change this change's specs or tasks.
