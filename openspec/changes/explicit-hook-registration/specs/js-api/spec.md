# Spec Delta: js-api

## ADDED Requirements

### Requirement: Explicit lifecycle hook registration
The engine SHALL expose `efx.registerUpdateHook(fn)` and
`efx.registerRenderHook(fn)` as public C-implemented functions on the single
`efx` namespace, on every runtime binding, with identical names, signatures,
semantics, and error behavior. Each call SHALL require `fn` to be a function
and SHALL throw `TypeError` otherwise. Registration SHALL append to the
respective hook list; hooks SHALL stack and run in registration order, with
all update hooks (each receiving `dt`, seconds since the previous frame)
before all render hooks (called with no arguments), once per frame. Each
registration SHALL return an unsubscribe function; calling it SHALL remove
that registration and SHALL be idempotent. An uncaught exception in any hook
SHALL stop the run with the non-zero exit-code contract. The F1 global
`update`/`render` functions SHALL remain supported as load-time sugar: when
defined at the end of evaluating `main.js`, they SHALL be registered in load
order, after any hooks registered during evaluation, and scripts that use only
the globals SHALL keep working (their update callback now also receives `dt`).

#### Scenario: Hooks stack in registration order
- **WHEN** a script registers two update hooks and one render hook, then a
  frame runs
- **THEN** both update hooks run in registration order, each receiving `dt`,
  before the render hook runs

#### Scenario: Unsubscribe removes a hook
- **WHEN** a script registers a hook and then calls the returned unsubscribe
  function
- **THEN** the hook no longer runs on subsequent frames, and calling the
  unsubscribe function again is a no-op

#### Scenario: Update hooks receive frame time
- **WHEN** an update hook runs
- **THEN** its first argument is a finite number of seconds since the previous
  frame

#### Scenario: Global hooks are load-time sugar
- **WHEN** a `main.js` defines global `update` and `render` functions and
  registers no explicit hooks
- **THEN** both globals are registered after evaluation and run once per frame
  in load order, with no behavior change other than the global `update`
  receiving `dt`

#### Scenario: Non-function registration is rejected
- **WHEN** a script calls `efx.registerUpdateHook` or
  `efx.registerRenderHook` with a non-function value
- **THEN** the call throws `TypeError` and registers nothing

#### Scenario: Hook exception stops the run
- **WHEN** any registered hook throws an uncaught exception
- **THEN** the frame loop stops, the error is reported on stderr/error
  channel, and the player exits non-zero

## MODIFIED Requirements

### Requirement: Normative API reference document
The project SHALL maintain `docs/js-api.md` as the normative, developer-facing
reference of the entire script API. It SHALL contain an entry for every public
API function with a signature sketch, a description, its layer tag, and the
roadmap milestone (F1–F8) that delivers it. Entries for functions whose
milestone has not passed its verification gate SHALL be explicitly marked
provisional. The document SHALL also document the lifecycle model — loading `main.js`
as the implicit init, with the `efx` namespace and engine API ready before it
executes (the rendering surface is initialized when the frame loop starts and
is not script-visible at load time), plus explicit, stacking hook registration
(`registerUpdateHook` / `registerRenderHook`, update hooks receiving `dt`,
unsubscribe returned, F1 globals as load-time sugar) — and how API errors
surface (exceptions, exit codes). Any change that adds, modifies, or removes a public API function
MUST update the document in the same change.

#### Scenario: Callable-today vs planned is distinguishable
- **WHEN** a reader opens the reference
- **THEN** the F1 functions (`efx.log`, `efx.quit`, `efx.args`,
  `efx.registerUpdateHook`, `efx.registerRenderHook`) are presented as current
  behavior, and later-milestone entries are marked provisional

#### Scenario: Milestone change updates the reference
- **WHEN** a feature change adds or changes an API function
- **THEN** the same change contains the matching `docs/js-api.md` update with
  the function's signature, layer, and milestone tags

#### Scenario: Catalog derived from vision
- **WHEN** the document's function catalog is checked against vision.md
- **THEN** every capability vision.md names for the consumer API (2D quads,
  meshes, vertex colors, cameras, lights, Phong materials with maps, alpha
  masks, blending modes, render targets, post FX, resource loading,
  skinning/animation, high-level model and text drawing) has a corresponding
  catalog entry or an explicitly noted open question
