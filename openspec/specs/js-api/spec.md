# js-api

## Purpose

Defines the contract for the engine's script-facing JavaScript API: one global
namespace, the two-layer (C-implemented / pure-JS) structure, the resource
memory model with fixed limits, and the normative developer-facing reference
document that catalogs every public API function by delivery milestone.

## Requirements

### Requirement: Single global API namespace
All engine-provided script functions SHALL be exposed as members of one
well-known global namespace object (the `efx` object established by F1),
available to every script without imports or setup. Scripts SHALL access
engine functionality only through this namespace and standard ES6 built-ins;
the reference document SHALL state this rule. This covers both C-implemented
functions and engine-provided high-level JS functions.

#### Scenario: Namespace available without setup
- **WHEN** a script calls `efx.log` without any import or setup code
- **THEN** the call succeeds on every target platform

#### Scenario: No scattered engine globals
- **WHEN** a reviewer checks how a script reaches an engine function
- **THEN** every engine-provided function is reachable as a member of the
  single namespace, not as an additional free global

### Requirement: Two-layer API with strict layering
The script API SHALL consist of exactly two layers: low/mid-level functions
implemented in C/C++ and registered through the engine binding, and
high-level convenience functions implemented in pure ES6. High-level
functions MUST be implemented using only the public low/mid-level API and
standard ES6 built-ins — they MUST NOT use private bindings or host
facilities that are not part of the public API. Every API function in the
reference SHALL be tagged with its layer.

#### Scenario: High-level function built on public API
- **WHEN** a high-level convenience function (e.g. a model or text drawer) is
  implemented
- **THEN** it calls only documented public API functions and standard ES6

#### Scenario: Layer tag present
- **WHEN** a function entry is read in the API reference
- **THEN** its entry marks it as either C-implemented or pure-JS

### Requirement: Resource classification and fixed limits
Every engine resource type that scripts can create or reference SHALL be
classified in the reference as exactly one of: JS-managed (plain script
objects, garbage collected), native-backed class (an opaque JS object
wrapping a native handle with an explicit `destroy()` release method; such a
class MAY additionally expose documented read-only query properties, which
MUST be listed in the reference — the first instance is Texture's `width` and
`height`), or slot-based (a fixed pre-allocated bank of indexed resources).
The native-backed classes SHALL be exactly: MeshData, ImageData, Mesh,
Texture, and RenderTarget; skins, skeletons, and animation clips are
implicit Mesh payload — loaded with the mesh and posed by the script
(`efx.poseMesh`) — and are not script resources; extending the class list
requires a `js-api` delta. Every resource requiring native storage MUST be
a native-backed class — released deterministically by its `destroy()`,
reclaimed by its GC finalizer if the script never calls it, and finalized
at runtime teardown — unless its count is fixed by design, in which case it
is slot-based. The runtime MUST factor native allocation sizes (CPU and GPU)
into GC pressure and MUST run collection at frame end, bounding
unreferenced native waste to roughly one frame. Resources recorded into the
display list MUST stay alive until playback completes. The reference SHALL
document the engine's fixed limits: 4 point lights, 1 directional light,
and 1 camera; lights are the only slot bank.

#### Scenario: Fixed limits stated
- **WHEN** the reference document's limits section is read
- **THEN** it states 4 point lights, 1 directional light, and 1 camera,
  matching vision.md

#### Scenario: Unreleased native resource is reclaimed
- **WHEN** a script creates textures in a loop and never calls
  `destroy()` on them
- **THEN** the native sizes drive GC pressure, finalizers reclaim the
  objects within roughly a frame of them becoming unreachable, and nothing
  leaks at runtime shutdown

#### Scenario: Destroyed resource is safe
- **WHEN** a script calls `destroy()` on a resource that the display list
  recorded earlier in the same frame
- **THEN** the native release is deferred until playback completes, and
  subsequent use of the destroyed resource throws

#### Scenario: Query properties are documented per class
- **WHEN** the reference document's Texture entry is read
- **THEN** it lists the read-only `width` and `height` query properties, and
  every other native-backed class entry states that it has none

#### Scenario: Resource without a classification
- **WHEN** a change proposes exposing a new resource type to scripts without
  classifying it as JS-managed, native-backed class, or slot-based
- **THEN** the change is incomplete and MUST NOT update the API reference

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
