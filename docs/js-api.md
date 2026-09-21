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
  `draw*` record into the display list, `make*` build data in JS,
  `load*`/`create*` fetch or upload resources and return resource objects.
- **Resources**: loaders and creators return opaque resource objects (the
  resource taxonomy — see [Resource & memory
  model](#resource--memory-model)); they are fully opaque for now —
  `res.destroy()` releases deterministically, GC is the backstop, and
  methods/getters/setters are reserved for later.
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

The entry script is `main.js` at the resource root (a zip in F6+).
**Loading `main.js` is the implicit init**: the engine is fully ready —
window, GL context, the `efx` namespace, the bundled high-level layer —
*before* the script executes, and top-level code is where setup happens.
There is no separate `init()` hook.

Frame callbacks are registered explicitly and stack in registration order:

```js
// provisional — target contract (ADR 0016)
const offUpdate = efx.registerUpdateHook(dt => { ... }); // dt: seconds since previous frame
const offRender = efx.registerRenderHook(() => { ... });

offUpdate(); // optional unsubscribe
```

- Update hooks run before render hooks, once per frame, in registration
  order; an exception in any hook (or at load time) stops the run and exits
  non-zero.
- **F1 current behavior:** the engine picks up global `update`/`render`
  functions (called with no arguments). These remain supported as
  load-time sugar — equivalent to a registration at the end of loading
  `main.js` — so F1 scripts keep working unchanged.
- `registerUpdateHook`/`registerRenderHook`, the `dt` argument, and the
  readiness-before-load ordering are delivered by the next runtime change
  (F2 at the latest). The REPL (F6) registers through the same functions —
  the reason registration, not globals, is the normative model.

## Resource & memory model

Every resource type scripts can create or reference is classified exactly
one way — the rule that keeps a GC'd language from leaking unmanaged memory
(vision.md). Seven dynamic-count resource types are opaque **native-backed
classes**; only the fixed light bank is slot-based (model: ADR 0011,
memory discipline: ADR 0012, taxonomy + glTF skinning model: ADR 0014,
all under `docs/decisions/`).

| Class | Meaning | Release path |
|---|---|---|
| **JS-managed** | Plain data objects; garbage collected | Drop the reference |
| **Native-backed class** | Opaque object wrapping a native handle — fully opaque for now (`destroy()` only); GC finalizer backstop | `res.destroy()` (primary), GC / shutdown (backstop) |
| **Slot-based** | Fixed pre-allocated bank of indexed resources | Overwrite the slot |

| Resource | Contents | Class | Side | Delivered | Notes |
|---|---|---|---|---|---|
| MeshData | Attributes + indices; skinned meshes add `joints`/`weights` vertex attributes (glTF-style) | Native class | CPU | F3 | `createMeshData` / `loadMeshData` (F6) |
| ImageData | Raw pixels + size + format | Native class | CPU | F2 | `createImageData` / `loadImage` (F6) |
| Skeleton | Joint hierarchy + inverse bind matrices (≈ glTF `skin`) | Native class | CPU | F7 | `createSkeleton` / `loadSkeleton` |
| Animation | Channels, keyframes, transforms | Native class | CPU | F7 | `createAnimation` / `loadAnimation` |
| Mesh | GPU memory bound mesh | Native class | GPU | F3 | `createMesh(meshData)`; `mesh.destroy()` |
| Texture | GPU texture | Native class | GPU | F2 | `createTexture(imageData)`; `tex.destroy()` |
| RenderTarget | GPU render target | Native class | GPU | F5 | `createRenderTarget`; `rt.destroy()` |
| Materials (Phong parameter objects) | — | JS-managed | — | F4 | Passed to `efx.setMaterial` |
| Fonts (atlas + quad layout) | — | JS-managed | — | F8 | Pure JS over Texture; passed to `drawText` |
| Lights | — | Slot-based | — | F4 | 4 point slots + 1 directional (fixed) |

**Resource lifecycle rules:**

- `destroy()` is deterministic and idempotent; using a destroyed resource
  throws.
- Native byte cost counts toward GC pressure and the player collects at
  frame end — unreferenced native resources are reclaimed within roughly a
  frame even if the script never calls `destroy()`.
- Resources recorded into the display list stay alive until playback
  finishes; `destroy()` during a frame defers the native release to frame
  end.
- Everything still alive at shutdown is finalized by runtime teardown —
  scripts cannot leak past process exit.

**Fixed limits** (vision.md — not configurable):

| Limit | Value |
|---|---|
| Point lights | 4 |
| Directional lights | 1 |
| Cameras | 1 (the active camera is set, never created) |

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

F1 connects frame callbacks via the global `update`/`render` functions
(still supported as load-time sugar — see
[Lifecycle hooks](#lifecycle-hooks)); the provisional
`efx.registerUpdateHook`/`efx.registerRenderHook` pair is the target
contract.

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
efx.createImageData(opts)         // → ImageData; opts: { width, height, pixels, format? } (shape pinned by F2)
efx.createTexture(imageData)      // → Texture; uploads CPU → GPU
efx.drawQuad(x, y, w, h, opts?)   // opts: { color?, texture?, uv? }
efx.setBlendMode(mode)            // 'alpha' (default) | 'additive' | 'subtractive'
```

- Quads are recorded into the display list in call order; the renderer may
  re-order for state changes — order is not a batching contract.
- Blending covers additive and subtractive (vision.md); alpha is the default
  mode for ordinary 2D drawing.
- All resource types are fully opaque for now (`destroy()` only); count is
  unbounded, memory-bounded only (see Resource & memory model).

```js
// main.js — F2 sample (provisional API)
const logo = efx.createTexture(
    efx.createImageData({ width: 64, height: 64, pixels: makeLogoPixels() }));
efx.setClearColor([0.08, 0.09, 0.12, 1]);
efx.setCamera2D({ x: 0, y: 0, zoom: 1 });

efx.registerRenderHook(() => {
    efx.setBlendMode('alpha');
    efx.drawQuad(64, 64, 128, 128, { texture: logo });
    efx.setBlendMode('additive');
    efx.drawQuad(224, 96, 64, 64, { color: [1, 0.5, 0, 1] });
});
```

### F3 — 3D core (provisional)

Scope from roadmap F3: camera, mesh slots, `drawMesh`, matrix math, depth
test, vertex colors, procedural primitives.

```js
// F3 · C · provisional
efx.setCamera3D(opts)      // { pos, target, fov } — fov in degrees; the one camera
efx.createMeshData(data)   // → MeshData; data: { positions, normals?, uvs?, colors? }
efx.createMesh(meshData)   // → Mesh; uploads CPU → GPU
efx.drawMesh(opts)         // { mesh, transform?, color? } — depth-tested; vertex colors used when present
```

- `MeshData` and `Mesh` are fully opaque for now (`destroy()` only).
  Skinned meshes extend MeshData with `joints`/`weights` vertex attributes
  in F7 (glTF-style).

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
// F3 · JS · provisional — procedural primitives producing mesh data for createMesh
efx.makeCube(opts?)   // { size }
efx.makePlane(opts?)  // { size, segments? }
efx.makeSphere(opts?) // { radius, segments? }
```

```js
// main.js — F3 sample (provisional API)
const cube = efx.createMesh(efx.createMeshData(efx.makeCube({ size: 1 })));
efx.setClearColor([0.08, 0.09, 0.12, 1]);
efx.setCamera3D({ pos: [0, 2, 5], target: [0, 0, 0], fov: 60 });

let yaw = 0;
efx.registerUpdateHook(dt => { yaw += dt * 45; });

efx.registerRenderHook(() => {
    efx.drawMesh({
        mesh: cube,
        transform: efx.mat4.rotate(efx.mat4.identity(), yaw, [0, 1, 0]),
        color: [0.9, 0.4, 0.2, 1],
    });
});
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
//   ambient:  { color, map: tex },      // tex: a Texture object
//   diffuse:  { color, map: tex },
//   specular: { color, shininess, map: tex },
//   emissive: { color, map: tex },
//   alphaMask: tex,
// }
```

- `mat` is a **JS-managed** object; the engine reads it at `setMaterial`
  time. Re-calling `setMaterial` with a different object switches materials.

```js
// main.js — F4 sample (provisional API)
const ball = efx.createMesh(efx.createMeshData(efx.makeSphere({ radius: 1, segments: 24 })));
efx.setClearColor([0.05, 0.05, 0.08, 1]);
efx.setCamera3D({ pos: [0, 2, 5], target: [0, 0, 0], fov: 60 });
efx.setLight(0, { pos: [3, 4, 2], color: [1, 0.95, 0.9, 1], range: 20 });
efx.setDirectionalLight({ dir: [-0.5, -1, -0.3], color: [0.2, 0.25, 0.35, 1] });
efx.setMaterial({
    ambient:  { color: [0.05, 0.05, 0.05, 1] },
    diffuse:  { color: [0.8, 0.3, 0.2, 1] },
    specular: { color: [1, 1, 1, 1], shininess: 32 },
    emissive: { color: [0, 0, 0, 1] },
});

efx.registerRenderHook(() => {
    efx.drawMesh({ mesh: ball });
});
```

### F5 — Render targets & post FX (provisional)

Scope from roadmap F5: RTT, fullscreen-quad passes, color filter, blur.

```js
// F5 · C · provisional — render targets are native-backed classes
efx.createRenderTarget(opts?)     // { width, height } → RenderTarget
efx.beginRenderTarget(rt)         // redirect drawing into the target
efx.endRenderTarget()             // back to the default target
efx.drawRenderTarget(rt, x, y, w, h, opts?)  // draw a target as a textured quad
// rt.destroy() releases it — deferred to frame end if the list still holds it
```

```js
// F5 · C · provisional — fullscreen post passes
efx.setColorFilter(opts)  // { brightness?, contrast?, saturation?, tint? } — null disables
efx.setBlur(opts)         // { radius } — null disables
```

```js
// main.js — F5 sample (provisional API)
efx.setCamera2D({ x: 0, y: 0, zoom: 1 });
efx.setColorFilter({ saturation: 0.6, contrast: 1.1 });
efx.setBlur({ radius: 2 });
const scene = efx.createRenderTarget({ width: 512, height: 512 });

efx.registerRenderHook(() => {
    efx.beginRenderTarget(scene);
    efx.drawQuad(96, 96, 320, 320, { color: [1, 0.4, 0.1, 1] });
    efx.endRenderTarget();

    efx.drawRenderTarget(scene, 256, 144, 512, 512);
});
```

### F6 — Resources (provisional)

Scope from roadmap F6: zip resource root, real asset import (asset format
decided here), interactive REPL. Paths are relative to the resource root
(`res://`-style: `loadText('data/level.json')`).

```js
// F6 · C · provisional — signatures final once the asset format is decided (F6)
efx.loadText(path)        // → string
efx.loadImage(path)       // → ImageData
efx.loadMeshData(path)    // → MeshData
efx.loadMesh(path)        // → Mesh (data + GPU upload in one step)

// F6 · JS · provisional — convenience composition on the public C layer
efx.loadTexture(path)     // → Texture (createTexture(loadImage(path)))
```

- The console/REPL run mode drives this same `efx` namespace interactively;
  no separate API.

```js
// main.js — F6 sample (provisional API)
efx.log(efx.loadText('data/welcome.txt'));
efx.setCamera3D({ pos: [0, 1, 4], target: [0, 0, 0], fov: 60 });
const teapot = efx.loadMesh('models/teapot.mesh');

efx.registerRenderHook(() => {
    efx.drawMesh({ mesh: teapot });
});
```

### F7 — Skinning & animation (provisional)

Scope from roadmap F7: CPU skinning into a mesh slot, skeleton/animation
import, play/pause/blend.

```js
// F7 · C · provisional
efx.createSkeleton(data)             // → Skeleton — joints + inverse bind matrices (≈ glTF `skin`)
efx.loadSkeleton(path)               // → Skeleton
efx.createAnimation(data)            // → Animation
efx.loadAnimation(path)              // → Animation
efx.setSkin(skel, mesh)              // bind a Skeleton to a skinned Mesh (≈ node.mesh + node.skin)
efx.playAnimation(skel, opts?)       // { animation, loop?, speed? } — Animation object
efx.pauseAnimation(skel)
efx.blendAnimations(skel, a, b, t)   // blend the poses of Animations a and b at weight t
```

- The skinning pipeline follows the glTF data model (ADR 0014): skinned
  MeshData carries per-vertex `joints` + `weights` attributes (typically
  4 influences) alongside its bind-pose positions, uploaded with
  `efx.createMesh`. `efx.setSkin` binds a Skeleton to that Mesh; every
  frame the CPU computes the posed vertices **in place** —
  `efx.drawMesh({ mesh })` renders it. Destroying a bound Mesh or Skeleton
  unbinds first.
- To keep the bind pose, create a second Mesh from the same MeshData;
  skinning only rewrites the Mesh given to `setSkin`. A different skeleton
  on the same model is a rebind (or a second Mesh) — mirroring glTF's
  per-node skins.

```js
// main.js — F7 sample (provisional API)
efx.setCamera3D({ pos: [0, 1.5, 4], target: [0, 1, 0], fov: 60 });
const hero = efx.loadMesh('actors/hero.mesh');    // bind pose + joints/weights attributes
const skel = efx.loadSkeleton('actors/hero.skel');
const walk = efx.loadAnimation('actors/hero.walk');
const run  = efx.loadAnimation('actors/hero.run');
efx.setSkin(skel, hero);
efx.playAnimation(skel, { animation: walk, loop: true });

let t = 0;
efx.registerUpdateHook(dt => {
    t += dt;
    efx.blendAnimations(skel, walk, run, Math.min(1, t / 2)); // walk → run over 2s
});

efx.registerRenderHook(() => {
    efx.drawMesh({ mesh: hero }); // renders the current CPU-skinned pose
});
```

### F8 — High-level drawing (provisional)

Scope from roadmap F8: high-level JS layer — `drawModel`, `drawText` (font
atlas built on quads), demo resource pack. These are engine-bundled pure ES6
built only on the public `[C]` API above.

```js
// F8 · JS · provisional
efx.loadFont(path)                   // → font object (JS-managed: atlas Texture + quad layout)
efx.drawModel(mesh, mat, opts?)      // { transform? } — one-call model drawing
                                     // over setMaterial + drawMesh
efx.drawText(text, x, y, opts?)      // { font, size?, color? } — text as quads
```

```js
// main.js — F8 sample (provisional API)
efx.setClearColor([0.08, 0.09, 0.12, 1]);
efx.setCamera3D({ pos: [0, 2, 5], target: [0, 0, 0], fov: 60 });
const teapot = efx.loadMesh('models/teapot.mesh');
const font = efx.loadFont('fonts/perfect.ttf');

let yaw = 0;
efx.registerUpdateHook(dt => { yaw += dt * 30; });

efx.registerRenderHook(() => {
    efx.drawModel(teapot, {
        diffuse:  { color: [0.8, 0.3, 0.2, 1] },
        specular: { color: [1, 1, 1, 1], shininess: 32 },
    }, { transform: efx.mat4.rotate(efx.mat4.identity(), yaw, [0, 1, 0]) });
    efx.drawText('score: 1200', 24, 24, { font, size: 32, color: [1, 1, 1, 1] });
});
```

## Vision traceability

Every consumer-API property named in `vision.md` maps to exactly one catalog
section (or an open question below):

| vision.md property | Where |
|---|---|
| 2D drawing via quads | F2 |
| Additive and subtractive blending modes | F2 (`setBlendMode`) |
| 1 camera fixed | F2 `setCamera2D`, F3 `setCamera3D`, limits table |
| Rendering meshes | F3 (`createMesh` / `drawMesh`) |
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
| Callbacks for update and rendering | Lifecycle hooks (explicit registration, ADR 0016) |
| Low/mid C + high-level JS layering | Overview (two layers), every entry tag |
| No browser/Node dependencies (incl. transitively) | Conventions (Dependencies) |
| Handles (resource objects) or pre-allocated slots for unmanaged resources | Resource & memory model |
| Fixed-function pipeline (no consumer-facing programmable shaders) | Engine-internal constraint — shapes what the API can express; internals use Sokol canned shaders (ADR 0015); no API entry |
| Immediate-mode API with re-orderable display list | Overview (immediate mode, deferred rendering) |
| Single-binary player for resource folders | Player runtime, not this API — see `openspec/specs` (`player-runtime`) |

## Open questions

Flagged gaps and deferred decisions — recorded here rather than inventing
API for them:

- **Input handling** — absent from vision.md. No keys/mouse/gamepad API is
  cataloged. If vision grows this capability, it enters through a future
  change with a `js-api` delta.
- **Audio** — absent from vision.md. Same treatment as input.
- **Asset format** — decided in F6; until then `load*` signatures stay
  provisional.
- **Hook registration + `dt` delivery** — target contract (ADR 0016);
  delivered by the next runtime change (F2 at the latest); F1 globals
  remain as load-time sugar.
- **REPL introspection helpers** — whether the F6 console mode needs extra
  `efx` functions beyond the interactive namespace is deferred to F6.
