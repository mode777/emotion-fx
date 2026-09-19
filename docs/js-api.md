# EmotionFX JavaScript API Reference

**Status:** F1 is implemented (current behavior). Everything from F2 onward is
a provisional contract — names and signatures may be reshaped by the change
that delivers them (every API change must update this document in the same
change). See `vision.md` for product goals and
`openspec/specs/feature-roadmap` for the milestone ladder.

## Overview

- **One namespace.** All engine-provided functions — C-implemented and
  pure-JS high-level — live on a single global object `efx`, available to
  every script without imports or setup. Scripts reach engine functionality
  only through `efx` and standard ES6 built-ins.
- **Two layers.** `[C]` entries are implemented in C and registered through
  the engine binding. `[JS]` entries are engine-provided, bundled pure ES6
  built on top of the public `[C]` API — nothing else.
- **Entry tags.** Each catalog entry carries its delivering milestone and
  layer:

  ```js
  // F2 · C · provisional
  efx.drawQuad(x, y, w, h, opts?)
  ```

  Entries without a `provisional` marker describe current, shipped behavior.
- **Immediate mode, deferred rendering.** `draw*` calls record into an
  internal display list that the renderer may re-order; there is no explicit
  flush in the API.

## Conventions

Every rule below traces to vision.md or to F1's implemented behavior.

- **Namespace** (F1 implementation): engine functions are members of the
  global `efx` object — `efx.log(...)`, `efx.quit(...)` — never free globals
  and never behind imports. The only free globals a script interacts with are
  the lifecycle hooks it *provides* (see below).
- **Naming**: camelCase, verb-first. `set*`/`get*` configure engine state,
  `draw*` record into the display list, `make*` build data in JS, `load*`
  fetch data from the resource root, `destroy*` release handles.
- **Parameters**: hot immediate-mode calls take scalar arguments first
  (`drawQuad(x, y, w, h, opts?)`); configuration beyond ~3 values goes in a
  trailing option object so signatures can grow without breaking calls.
- **Units**: angles in **degrees** (radians never appear in the API), time in
  **seconds**, positions and sizes in world units.
- **Colors**: `[r, g, b, a]` arrays of normalized floats in `0..1`
  (e.g. `[1, 0.5, 0, 1]`).
- **Errors**: invalid input throws standard ES6 errors (`TypeError` for wrong
  types, `RangeError` for out-of-range slots/indices). An uncaught exception
  stops the run with a non-zero exit code and the error on stderr — in
  `--script` mode and in both frame hooks alike.
- **Dependencies**: scripts use only the `efx` namespace and standard ES6.
  Browser and Node.js APIs are unavailable — not even transitively.

## Lifecycle hooks

The entry script is `main.js` at the resource root (a zip in F6+). The
script may define these free globals, which the engine calls:

```js
// script-provided (called by the engine, not part of efx)
function init()     { } // provisional — once, after the script is loaded, before the first frame
function update(dt) { } // once per frame, before render
function render()   { } // once per frame
```

- All hooks are optional; an undefined hook is skipped without error.
- **F1 current behavior:** the engine evaluates top-level `main.js` code once
  at load time and calls `update`/`render` per frame with **no arguments**;
  `init()` is not invoked yet.
- **Target contract:** `init()` (setup needing the engine fully ready) and
  `update(dt)` (`dt` = seconds since the previous frame) are ratified by the
  next runtime change (F2 at the latest). Both are additive and
  backward-compatible; until then, setup goes in top-level code. The F2–F8
  samples below use them.
- An uncaught exception inside a hook (or at load time) stops the run and
  exits non-zero.

## Resource & memory model

Every resource type scripts can create or reference is classified exactly
one way — this is the rule that keeps a GC'd language from leaking unmanaged
memory (vision.md):

| Class | Meaning | Release path |
|---|---|---|
| **JS-managed** | Plain script objects; garbage collected | Drop the reference |
| **Slot-based** | Fixed pre-allocated set of indexed native resources | Overwrite the slot (`setMesh(slot, data)`) |
| **Handle-based** | Dynamically created native resource | Explicit `destroy*(handle)` |

| Resource | Class | Delivered | Notes |
|---|---|---|---|
| Materials (Phong parameter objects) | JS-managed | F4 | Passed to `efx.setMaterial` |
| Mesh/vertex data before upload | JS-managed | F3 | Output of `make*` / input to `setMesh` |
| Meshes (native storage) | Slot-based | F3 | `efx.setMesh(slot, data)` |
| Textures | Slot-based | F2 | `efx.setTexture(slot, data)` |
| Lights | Slot-based | F4 | 4 point slots + 1 directional (fixed) |
| Render targets | Handle-based | F5 | `createRenderTarget` / `destroyRenderTarget` |
| Skeletons / animations | Slot-based | F7 | `setSkeleton` / `setAnimation` |

**Fixed limits** (vision.md — not configurable):

| Limit | Value |
|---|---|
| Point lights | 4 |
| Directional lights | 1 |
| Cameras | 1 (the active camera is set, never created) |

**Slot counts** are fixed constants; the values below are provisional until
pinned by the delivering milestone:

| Slot family | Count | Status |
|---|---|---|
| Textures | 8 | provisional — pinned by F2 |
| Meshes | 8 | provisional — pinned by F3 |
| Skeletons / animations | 8 each | provisional — pinned by F7 |

## API catalog

### F1 — Environment & utilities (current)

Implemented in `src/api/api.c` and registered on the `efx` object by
`src/runtime/runtime.c`. These are current behavior, not provisional.

```js
// F1 · C
efx.log(msg?)
```

Prints `msg` to stdout followed by a newline and flushes. Non-string values
are converted with their standard string representation; a missing argument
prints an empty line.

```js
// F1 · C
efx.quit(code?)
```

Requests engine termination with exit code `code` (default `0`). The call
never returns normally: the engine unwinds the current script execution and
exits with the requested code. Works identically in `--script` mode (the
vehicle for smoke tests) and inside the frame loop.

```js
// F1 · C
efx.args()
```

Returns a `string[]` of the arguments the host passed to the script run
(the `--script <file> [args...]` tail). Empty array when none were given.

The lifecycle hooks `init`, `update`, and `render` are described in
[Lifecycle hooks](#lifecycle-hooks); they are provided by the script, not
part of `efx`.

```js
// main.js — F1 sample (current behavior only)
let frames = 0;

function update() {
    frames++;
    if (frames === 1) {
        efx.log('hello from efx ' + efx.args().join(' '));
    }
    if (frames >= 60) {
        efx.quit(0); // exits the player with code 0
    }
}

function render() {}
```

### F2 — 2D drawing (provisional)

Scope from roadmap F2: `drawQuad`, ortho camera, texture slots, blending
modes, display list (record → playback).

```js
// F2 · C · provisional
efx.setClearColor(color)          // [r,g,b,a] clear color for each frame
efx.setCamera2D(opts)             // { x, y, zoom, rotation? } — the one ortho camera
efx.drawQuad(x, y, w, h, opts?)   // opts: { color?, textureSlot?, uv? }
efx.setTexture(slot, data)        // slot 0..TEXTURE_SLOTS-1; data shape pinned by F2
efx.setBlendMode(mode)            // 'alpha' (default) | 'additive' | 'subtractive'
```

- Quads are recorded into the display list in call order; the renderer may
  re-order for state changes — order is not a batching contract.
- Blending covers additive and subtractive (vision.md); alpha is the default
  mode for ordinary 2D drawing.

```js
// main.js — F2 sample (provisional API)
const LOGO = 0;
let t = 0;

function init() {
    efx.setClearColor([0.08, 0.09, 0.12, 1]);
    efx.setCamera2D({ x: 0, y: 0, zoom: 1 });
    efx.setTexture(LOGO, { width: 64, height: 64, pixels: makeLogoPixels() });
}

function update(dt) {
    t += dt;
}

function render() {
    efx.setBlendMode('alpha');
    efx.drawQuad(64, 64, 128, 128, { textureSlot: LOGO });
    efx.setBlendMode('additive');
    efx.drawQuad(224, 96, 64, 64, { color: [1, 0.5, 0, 1] });
}
```

### F3 — 3D core (provisional)

Scope from roadmap F3: camera, mesh slots, `drawMesh`, matrix math, depth
test, vertex colors, procedural primitives.

```js
// F3 · C · provisional
efx.setCamera3D(opts)     // { pos, target, fov } — fov in degrees; the one camera
efx.setMesh(slot, data)   // slot 0..MESH_SLOTS-1; data: { positions, normals?, uvs?, colors? }
efx.drawMesh(opts)        // { mesh, transform?, color? } — depth-tested; vertex colors used when present
```

```js
// F3 · JS · provisional — pure-JS math helpers, engine-bundled
efx.mat4.identity()  efx.mat4.perspective(fovY, aspect, near, far)
efx.mat4.ortho(...)  efx.mat4.translate(m, v)   efx.mat4.rotate(m, deg, axis)
efx.mat4.scale(m, v) efx.mat4.multiply(a, b)
efx.vec3.add(a, b)   efx.vec3.sub(a, b)  efx.vec3.scale(v, s)
efx.vec3.normalize(v) efx.vec3.cross(a, b) efx.vec3.dot(a, b)
efx.quat.*  // quaternion helpers, delivered with F3 math, consumed by F7
```

```js
// F3 · JS · provisional — procedural primitives producing mesh data for setMesh
efx.makeCube(opts?)   // { size }
efx.makePlane(opts?)  // { size, segments? }
efx.makeSphere(opts?) // { radius, segments? }
```

```js
// main.js — F3 sample (provisional API)
const CUBE = 0;
let yaw = 0;

function init() {
    efx.setClearColor([0.08, 0.09, 0.12, 1]);
    efx.setCamera3D({ pos: [0, 2, 5], target: [0, 0, 0], fov: 60 });
    efx.setMesh(CUBE, efx.makeCube({ size: 1 }));
}

function update(dt) {
    yaw += dt * 45;
}

function render() {
    efx.drawMesh({
        mesh: CUBE,
        transform: efx.mat4.rotate(efx.mat4.identity(), yaw, [0, 1, 0]),
        color: [0.9, 0.4, 0.2, 1],
    });
}
```

### F4 — Materials & lights (provisional)

Scope from roadmap F4a/F4b: 4 point + 1 directional light; 4-channel Phong
(Ambient, Diffuse, Specular, Emissive) on solids/vertex colors (F4a);
per-channel maps + alpha masks (F4b).

```js
// F4a · C · provisional
efx.setLight(slot, opts)         // slot 0..3 — point light { pos, color, range? }
efx.setDirectionalLight(opts)    // { dir, color } — the single directional light
efx.setMaterial(mat)             // Phong channels:
// {
//   ambient:  { color },
//   diffuse:  { color },
//   specular: { color, shininess },
//   emissive: { color },
// }
```

```js
// F4b · C · provisional — per-channel maps and alpha masks extend the same material object
// {
//   ambient:  { color, map: textureSlot },
//   diffuse:  { color, map: textureSlot },
//   specular: { color, shininess, map: textureSlot },
//   emissive: { color, map: textureSlot },
//   alphaMask: textureSlot,
// }
```

- `mat` is a **JS-managed** object; the engine reads it at `setMaterial`
  time. Re-calling `setMaterial` with a different object switches materials.

```js
// main.js — F4 sample (provisional API)
const BALL = 0;

function init() {
    efx.setClearColor([0.05, 0.05, 0.08, 1]);
    efx.setCamera3D({ pos: [0, 2, 5], target: [0, 0, 0], fov: 60 });
    efx.setMesh(BALL, efx.makeSphere({ radius: 1, segments: 24 }));
    efx.setLight(0, { pos: [3, 4, 2], color: [1, 0.95, 0.9, 1], range: 20 });
    efx.setDirectionalLight({ dir: [-0.5, -1, -0.3], color: [0.2, 0.25, 0.35, 1] });
    efx.setMaterial({
        ambient:  { color: [0.05, 0.05, 0.05, 1] },
        diffuse:  { color: [0.8, 0.3, 0.2, 1] },
        specular: { color: [1, 1, 1, 1], shininess: 32 },
        emissive: { color: [0, 0, 0, 1] },
    });
}

function render() {
    efx.drawMesh({ mesh: BALL });
}
```

### F5 — Render targets & post FX (provisional)

Scope from roadmap F5: RTT, fullscreen-quad passes, color filter, blur.

```js
// F5 · C · provisional — render targets are the handle-based family
efx.createRenderTarget(opts?)     // { width, height } → handle
efx.destroyRenderTarget(handle)  // explicit release (required)
efx.beginRenderTarget(handle)    // redirect drawing into the target
efx.endRenderTarget()            // back to the default target
efx.drawRenderTarget(handle, x, y, w, h, opts?)  // draw a target as a textured quad
```

```js
// F5 · C · provisional — fullscreen post passes
efx.setColorFilter(opts)  // { brightness?, contrast?, saturation?, tint? } — null disables
efx.setBlur(opts)         // { radius } — null disables
```

```js
// main.js — F5 sample (provisional API)
let scene = null;

function init() {
    efx.setCamera2D({ x: 0, y: 0, zoom: 1 });
    scene = efx.createRenderTarget({ width: 512, height: 512 });
    efx.setColorFilter({ saturation: 0.6, contrast: 1.1 });
    efx.setBlur({ radius: 2 });
}

function render() {
    efx.beginRenderTarget(scene);
    efx.drawQuad(96, 96, 320, 320, { color: [1, 0.4, 0.1, 1] });
    efx.endRenderTarget();

    efx.drawRenderTarget(scene, 256, 144, 512, 512);
}
```

### F6 — Resources (provisional)

Scope from roadmap F6: zip resource root, real asset import (asset format
decided here), interactive REPL. Paths are relative to the resource root
(`res://`-style: `loadText('data/level.json')`).

```js
// F6 · C · provisional — signatures final once the asset format is decided (F6)
efx.loadText(path)           // → string
efx.loadImage(path)          // → texture data for efx.setTexture
efx.loadMesh(path)           // → mesh data for efx.setMesh
```

- The console/REPL run mode drives this same `efx` namespace interactively;
  no separate API.

```js
// main.js — F6 sample (provisional API)
const MESH = 0;

function init() {
    efx.log(efx.loadText('data/welcome.txt'));
    efx.setCamera3D({ pos: [0, 1, 4], target: [0, 0, 0], fov: 60 });
    efx.setMesh(MESH, efx.loadMesh('models/teapot.mesh'));
}

function render() {
    efx.drawMesh({ mesh: MESH });
}
```

### F7 — Skinning & animation (provisional)

Scope from roadmap F7: CPU skinning into a mesh slot, skeleton/animation
import, play/pause/blend.

```js
// F7 · C · provisional
efx.setSkeleton(slot, data)            // slot 0..SKELETON_SLOTS-1 — joints + inverse bind matrices
efx.setAnimation(slot, data)           // slot 0..ANIMATION_SLOTS-1
efx.loadSkeleton(path)                 // → skeleton data for setSkeleton
efx.loadAnimation(path)                // → animation data for setAnimation
efx.setSkin(skelSlot, meshSlot)        // bind a skeleton to a skinned mesh slot
efx.playAnimation(skelSlot, opts?)     // { animation, loop?, speed? }
efx.pauseAnimation(skelSlot)
efx.blendAnimations(skelSlot, a, b, t) // blend pose of animations a and b at weight t
```

- The skinning pipeline: a **skinned mesh** is ordinary F3 mesh data extended
  with per-vertex `joints` + `weights` attributes, uploaded in bind pose via
  `efx.setMesh`. `efx.setSkin` binds it to a skeleton; from then on every
  frame the CPU computes the posed vertices **in place** and the bound mesh
  slot always holds the current pose — `efx.drawMesh({ mesh })` renders it.
- To keep the bind pose, upload the same data to a second mesh slot first;
  skinning only rewrites the slot given to `setSkin`.

```js
// main.js — F7 sample (provisional API)
const SKEL = 0, HERO = 0; // HERO: mesh slot 0, starts as the bind pose
let t = 0;

function init() {
    efx.setCamera3D({ pos: [0, 1.5, 4], target: [0, 1, 0], fov: 60 });
    efx.setMesh(HERO, efx.loadMesh('actors/hero.mesh')); // bind pose + joints/weights
    efx.setSkeleton(SKEL, efx.loadSkeleton('actors/hero.skel'));
    efx.setAnimation(0, efx.loadAnimation('actors/hero.walk'));
    efx.setAnimation(1, efx.loadAnimation('actors/hero.run'));
    efx.setSkin(SKEL, HERO); // posed vertices are written back into slot 0 each frame
    efx.playAnimation(SKEL, { animation: 0, loop: true });
}

function update(dt) {
    t += dt;
    efx.blendAnimations(SKEL, 0, 1, Math.min(1, t / 2)); // walk → run over 2s
}

function render() {
    efx.drawMesh({ mesh: HERO }); // renders the current CPU-skinned pose
}
```

### F8 — High-level drawing (provisional)

Scope from roadmap F8: high-level JS layer — `drawModel`, `drawText` (font
atlas built on quads), demo resource pack. These are engine-bundled pure ES6
built only on the public `[C]` API above.

```js
// F8 · JS · provisional
efx.loadFont(path)                   // → font for drawText (atlas built on quads)
efx.drawModel(meshSlot, mat, opts?)  // { transform? } — one-call model drawing
                                     // over setMaterial + drawMesh
efx.drawText(text, x, y, opts?)      // { font, size?, color? } — text as quads
```

```js
// main.js — F8 sample (provisional API)
const MESH = 0;
let yaw = 0;
let font = null;

function init() {
    efx.setClearColor([0.08, 0.09, 0.12, 1]);
    efx.setCamera3D({ pos: [0, 2, 5], target: [0, 0, 0], fov: 60 });
    efx.setMesh(MESH, efx.loadMesh('models/teapot.mesh'));
    font = efx.loadFont('fonts/perfect.ttf');
}

function update(dt) {
    yaw += dt * 30;
}

function render() {
    efx.drawModel(MESH, {
        diffuse:  { color: [0.8, 0.3, 0.2, 1] },
        specular: { color: [1, 1, 1, 1], shininess: 32 },
    }, { transform: efx.mat4.rotate(efx.mat4.identity(), yaw, [0, 1, 0]) });
    efx.drawText('score: 1200', 24, 24, { font, size: 32, color: [1, 1, 1, 1] });
}
```

## Vision traceability

Every consumer-API property named in `vision.md` maps to exactly one catalog
section (or an open question below):

| vision.md property | Where |
|---|---|
| 2D drawing via quads | F2 |
| Additive and subtractive blending modes | F2 (`setBlendMode`) |
| 1 camera fixed | F2 `setCamera2D`, F3 `setCamera3D`, limits table |
| Rendering meshes | F3 (`setMesh` / `drawMesh`) |
| Vertex colours | F3 (mesh `colors?` attribute, `drawMesh` color) |
| Matrix math | F3 (`efx.mat4` / `efx.vec3` / `efx.quat`) |
| Procedural primitives | F3 (`makeCube` / `makePlane` / `makeSphere`) |
| 4 point lights, 1 directional light | F4, limits table |
| Phong material system, 4 channels + maps | F4a/F4b (`setMaterial`) |
| Alpha masks | F4b (`alphaMask`) |
| Rendering to textures | F5 (render targets) |
| Simple post processing (color filter, blur) | F5 (`setColorFilter` / `setBlur`) |
| Resource folder / zip root (`res://`-like) | F6 (load paths, zip in F6) |
| REPL console mode | F6 (drives the same `efx` namespace) |
| Skinning and animations | F7 |
| High-level functions in pure JS (`drawModel`, `drawText`) | F8 |
| Callbacks for update and rendering | Lifecycle hooks (`init` extends this contract) |
| Low/mid C + high-level JS layering | Overview (two layers), every entry tag |
| No browser/Node dependencies (incl. transitively) | Conventions (Dependencies) |
| Handles or pre-allocated slots for unmanaged resources | Resource & memory model |
| Fixed-function pipeline (no programmable shaders) | Engine-internal constraint — shapes what the API can express; no API entry |
| Immediate-mode API with re-orderable display list | Overview (immediate mode, deferred rendering) |
| Single-binary player for resource folders | Player runtime, not this API — see `openspec/specs` (`player-runtime`) |

## Open questions

Flagged gaps and deferred decisions — recorded here rather than inventing
API for them:

- **Input handling** — absent from vision.md. No keys/mouse/gamepad API is
  cataloged. If vision grows this capability, it enters through a future
  change with a `js-api` delta.
- **Audio** — absent from vision.md. Same treatment as input.
- **Texture/mesh/skeleton slot counts** — provisional (8 / 8 / 8 each) until
  pinned by F2 / F3 / F7 respectively.
- **Asset format** — decided in F6; until then `load*` signatures stay
  provisional.
- **`update(dt)` argument** — target contract; ratified by F2 (see Lifecycle
  hooks).
- **REPL introspection helpers** — whether the F6 console mode needs extra
  `efx` functions beyond the interactive namespace is deferred to F6.
