# Spec Delta

## Purpose

Defines the 3D core behavior delivered by milestone F3: the perspective
camera and its separation from the 2D projection frame, multi-surface mesh
data resources (Godot-style surfaces), GPU mesh upload and lifecycle,
whole-mesh depth-tested drawing, procedural primitives with pinned defaults,
and the engine-bundled pure-JS math layer.

## ADDED Requirements

### Requirement: 3D camera
`efx.setCamera3D(opts)` SHALL configure the engine's single 3D camera from an
option object `{ pos, target, fov, near?, far? }`: `pos` and `target` are
`[x, y, z]` world points (the eye position and the looked-at point), `fov` is
the **vertical** field of view in **degrees**, `near` and `far` are the depth
range in world units with defaults 0.1 and 100. The up vector SHALL be
`[0, 1, 0]`. The 3D camera SHALL be set, never created, and there SHALL be
exactly one (vision.md fixed limits). The 3D camera SHALL be a projection
state separate from the F2 2D projection frame: `setCamera3D` SHALL affect
only 3D draws (mesh records), while 2D draws (`drawQuad`, render-target quad
draws) continue to render under the 2D camera state established by the most
recent `setCamera2D` (or the F2 default when never called), leaving F2
behavior unchanged. Like all recorded state (ADR 0019), the camera SHALL be
value-snapshotted at record time: a mesh draw records the 3D camera state in
effect when the draw is recorded and MUST NOT observe later camera changes.
Calling `setCamera3D` with a malformed bag (missing or non-array `pos`/
`target`, non-number `fov`/`near`/`far`, unknown fields) SHALL throw
`TypeError` and change nothing.

#### Scenario: Perspective projection is observable
- **WHEN** two equal meshes are drawn at different distances from the camera
  eye along its view axis with the same `drawMesh` parameters
- **THEN** the nearer mesh appears larger in the frame, and a wider `fov`
  renders a visibly larger field of the scene

#### Scenario: 2D draws are unaffected by the 3D camera
- **WHEN** `setCamera3D` is active and a `drawQuad` is recorded
- **THEN** the quad renders under the most recent `setCamera2D` state (or the
  F2 default camera when `setCamera2D` was never called), and the committed
  F2 golden output is unchanged

#### Scenario: Camera is value-snapshotted per record
- **WHEN** a mesh is drawn, then `setCamera3D` moves the camera, all in one
  render hook
- **THEN** playback renders that mesh with the camera state at its record
  time

#### Scenario: Defaults for near and far
- **WHEN** `setCamera3D({ pos, target, fov })` is called without `near`/`far`
- **THEN** the depth range is 0.1 to 100 and the draw succeeds

#### Scenario: Malformed camera options throw
- **WHEN** `setCamera3D` is called with a missing `pos`, a non-number `fov`,
  or an unknown field
- **THEN** the call throws `TypeError` and the previously set camera state
  remains in effect

### Requirement: Multi-surface mesh data
`efx.createMeshData(data)` SHALL build a CPU-side MeshData (native-backed
class, ADR 0011/0013) holding 1..16 **surfaces**. Two construction forms
SHALL be accepted: a batch bag `{ surfaces: [surface, ...] }`, or a
single-surface shorthand `{ positions, normals?, uvs?, colors?, indices? }`
(equivalent to a one-element `surfaces` array). Passing both `surfaces` and
`positions` SHALL throw `TypeError`; passing neither SHALL throw `TypeError`.
A **surface** is one Godot-style surface / glTF primitive — a set of
attribute arrays plus optional indices:

- `positions` — required, a flat array (or typed array) of finite numbers,
  length a multiple of 3 (xyz per vertex) and greater than 0;
- `normals?` — flat xyz array with exactly the same vertex count as
  `positions`;
- `uvs?` — flat uv array with exactly the same vertex count as `positions`;
- `colors?` — flat rgba array (normalized floats, the F2 color convention)
  with exactly the same vertex count as `positions`;
- `indices?` — flat array of non-negative integers: a triangle list whose
  length is a multiple of 3 and whose every value is less than the surface's
  vertex count.

When `indices` is omitted, the vertex count MUST be a multiple of 3
(non-indexed triangle list). A wrong attribute count, a non-multiple-of-3
`positions`/`indices` length, an out-of-range index, an empty `surfaces`
array, or a surface count above 16 SHALL throw `RangeError`; wrong element
types SHALL throw `TypeError`; unknown fields SHALL throw `TypeError`. The
fixed limit is **16 surfaces per mesh** (vision.md fixed limits, recorded in
the reference). Each surface carries an optional material binding slot; the
binding is inert for rendering until F4 delivers material objects, and a
`materials` field on the data bag SHALL be rejected as unknown until F4.
MeshData SHALL expose the read-only query property `surfaceCount` (the
number of surfaces; throws `TypeError` when destroyed). `destroy()` releases
the native storage deterministically and is idempotent; using a destroyed
MeshData SHALL throw.

#### Scenario: Batch construction creates multiple surfaces
- **WHEN** `createMeshData({ surfaces: [s0, s1] })` is called with two valid
  surfaces
- **THEN** the returned MeshData's `surfaceCount` is 2 and each surface
  retains its own attribute arrays and indices

#### Scenario: Single-surface shorthand
- **WHEN** `createMeshData({ positions, colors })` is called with valid
  arrays
- **THEN** the result is identical to a one-element `surfaces` array
  containing that surface, and `surfaceCount` is 1

#### Scenario: Validation errors
- **WHEN** a surface's `normals` length does not match its vertex count, an
  `indices` value equals the vertex count, an empty `surfaces` array is
  passed, or a 17th surface is passed
- **THEN** `createMeshData` throws `RangeError` and records nothing

#### Scenario: Unknown fields and types throw
- **WHEN** the data bag or a surface contains an unknown field, or an
  attribute array holds a non-number
- **THEN** the call throws `TypeError`

#### Scenario: Materials are an F4 concept
- **WHEN** `createMeshData` is called with a `materials` array before F4
  ships
- **THEN** the call throws `TypeError` (unknown field), and from F4 onward
  the parallel `materials[i]` array binds material `i` (or the default when
  `null`) to surface `i` at creation time

#### Scenario: Query property and destroy
- **WHEN** a script reads `surfaceCount` on a MeshData, destroys it, and
  reads `surfaceCount` again
- **THEN** the first read returns the surface number and the second throws
  `TypeError`

### Requirement: Mesh upload and lifecycle
`efx.createMesh(meshData)` SHALL upload a live MeshData's **every** surface
CPU → GPU into one Mesh (native-backed class, ADR 0011/0013: deterministic
`destroy()`, idempotent, GC-finalizer backstop, display-list references keep
it alive until playback completes). The Mesh SHALL expose the read-only
query property `surfaceCount` (equal to the MeshData's at upload time;
throws `TypeError` when destroyed). A Mesh is a copy: later changes to the
source MeshData object MUST NOT affect the Mesh. Passing a non-MeshData or a
destroyed MeshData SHALL throw `TypeError`. Surface material bindings
carried by the MeshData (from F4) SHALL carry over to the Mesh at upload;
from F4, `efx.setMeshSurfaceMaterial(mesh, surfaceIndex, mat)` SHALL rebind
one surface's material after upload (the Godot `surface_set_material`
analog: `mat` is a JS-managed object snapshotted at call time; an index out
of range throws `RangeError`); a surface without a bound material SHALL
render with an engine default material. Meshes are never slot-based; scripts
manage Mesh objects directly (ADR 0011 resource model — the roadmap's
"mesh slots" phrase is realized as resource objects, per the pinned js-api
taxonomy).

#### Scenario: All surfaces upload
- **WHEN** `createMesh` is called with a 3-surface MeshData
- **THEN** the Mesh's `surfaceCount` is 3 and drawing it renders all three
  surfaces

#### Scenario: Upload copies
- **WHEN** a Mesh is created from a MeshData and the MeshData is then
  destroyed
- **THEN** the Mesh keeps rendering identically (destroyed-use rules apply
  only to the destroyed object)

#### Scenario: Destroyed mesh is safe
- **WHEN** `destroy()` is called on a Mesh twice and a `drawMesh` references
  it after the first call
- **THEN** the second `destroy()` is a no-op and the draw throws `TypeError`

### Requirement: Whole-mesh drawing with depth
`efx.drawMesh(opts)` SHALL record one draw for the whole mesh from the bag
`{ mesh, transform?, color? }`: `mesh` is required and MUST be a live Mesh
(nothing, a non-Mesh, or a destroyed Mesh throws `TypeError`);
`transform?` is a flat array (or typed array) of exactly 16 finite numbers —
a column-major 4×4 matrix, default identity (a wrong length throws
`RangeError`, non-number elements throw `TypeError`); `color?` is a
`[r, g, b, a]` tint, default opaque white. Unknown option fields SHALL throw
`TypeError`. Playback SHALL draw every surface in surface order under the
recorded camera, with the depth test enabled and depth writing on: a
nearer surface occludes a farther one regardless of record order, and
equal-depth fragments resolve by record order (deterministic). Per-surface
color SHALL be the tint multiplied by the surface's vertex color where the
`colors` attribute is present, or the tint alone otherwise (no lighting in
F3 — the F3 canned fill is unlit). 2D quad records SHALL be untouched by
mesh depth (F2 behavior and goldens unchanged). Mesh draws participate in
the per-frame record budget like any record.

#### Scenario: Multi-surface mesh draws all surfaces
- **WHEN** a 2-surface mesh is drawn where the surfaces occupy different
  screen areas
- **THEN** both surfaces appear, in surface order

#### Scenario: Depth test occludes by distance
- **WHEN** a nearer mesh is recorded before a farther mesh that occupies the
  same screen area
- **THEN** the nearer mesh's fragments win the depth test despite the later
  record

#### Scenario: Vertex colors multiply the tint
- **WHEN** a surface with per-vertex colors is drawn with
  `color: [r, g, b, a]`
- **THEN** each fragment is the vertex color multiplied component-wise by
  the tint; a surface without vertex colors renders in the tint color

#### Scenario: Transform positions the mesh
- **WHEN** the same mesh is drawn twice with different `transform` matrices
  (e.g. translation versus identity)
- **THEN** the two renders appear at different world positions per the
  recorded matrices

#### Scenario: Validation errors
- **WHEN** `drawMesh` is called with no `mesh`, a 15-element `transform`, or
  an unknown option field
- **THEN** the call throws `TypeError` / `RangeError` / `TypeError`
  respectively and records nothing

### Requirement: Procedural primitives
The engine-bundled pure-JS primitives `efx.makeCube(opts?)`,
`efx.makePlane(opts?)`, and `efx.makeSphere(opts?)` SHALL each return a
single-surface MeshData (directly usable by `createMesh`), with pinned
defaults so scenes are reproducible:

- `makeCube({ size? = 1 })` — an axis-aligned cube centered on the origin
  with side length `size`, outward per-face normals, outward-facing (CCW)
  winding, and per-face uv mapping of the full 0..1 square;
- `makePlane({ size? = 1, segments? = 1 })` — a plane in the XZ plane facing
  `+Y`, centered on the origin with side length `size`, subdivided into
  `segments × segments` quads, uv coordinates spanning 0..1;
- `makeSphere({ radius? = 1, segments? = 16 })` — a UV sphere centered on
  the origin with radius `radius` and `segments` latitude and longitude
  bands, normals equal to normalized positions, equirectangular uv spanning
  0..1.

A non-finite or non-positive `size`/`radius` SHALL throw `RangeError`;
`segments` SHALL be a positive integer (`RangeError` otherwise); unknown
fields SHALL throw `TypeError`. Missing option objects take all defaults.

#### Scenario: Defaults produce a usable mesh
- **WHEN** `efx.makeCube()` is called with no arguments
- **THEN** the result is a single-surface MeshData with `surfaceCount` 1
  that uploads and draws like any other MeshData

#### Scenario: Parameter overrides
- **WHEN** `makeSphere({ radius: 2, segments: 24 })` is called
- **THEN** the surface's positions lie on a radius-2 sphere and the vertex
  count follows the 24-band layout deterministically

#### Scenario: Invalid parameters throw
- **WHEN** `makeCube({ size: 0 })` or `makePlane({ segments: 1.5 })` is
  called
- **THEN** the call throws `RangeError`

### Requirement: Script math layer
The engine SHALL bundle pure-JS math helpers on the `efx` object — `efx.mat4`
(`identity`, `perspective(fovY, aspect, near, far)`, `ortho(width, height,
near, far)`, `translate(m, v)`, `rotate(m, deg, axis)`, `scale(m, v)`,
`multiply(a, b)`), `efx.vec3` (`add`, `sub`, `scale(v, s)`, `normalize`,
`cross`, `dot`), and `efx.quat` (`identity`, `fromAxisAngle(deg, axis)`,
`multiply(a, b)`, `toMat4(q)`) — implemented entirely in the `[JS]` layer on
standard ES6 (zero browser/Node dependencies, per the two-layer rule). All
helpers SHALL be pure functions that never mutate their arguments and return
plain JS data: matrices are flat 16-number column-major arrays, vectors are
3-number arrays, angles are **degrees** (ADR 0010: script math is plain JS
data; the API never uses radians). Matrix composition SHALL follow the
column-major convention `multiply(a, b)` computes `a·b` (b applies to the
vector first), and `rotate(m, deg, axis)` computes `m·R(deg, axis)` — a
right-handed rotation, counter-clockwise about `axis` looking down the axis
toward the origin, matching the F2 degree convention's 3D counterpart. The
helpers MUST produce values consistent with the engine's own camera math so
script-built transforms and `setCamera3D` compose predictably.

#### Scenario: Perspective matrix is correct
- **WHEN** a math unit test computes `efx.mat4.perspective(60, 4/3, 0.1, 100)`
  and checks selected entries against the expected perspective values
- **THEN** the entries match (degrees-to-tan conversion, aspect on the x
  axis, near/far depth mapping)

#### Scenario: Multiplication order
- **WHEN** `multiply(translate(m, t), rotate(m2, deg, axis))` transforms a
  point
- **THEN** the rotation applies to the point first, then the translation —
  `v' = T · R · v`

#### Scenario: Pure functions
- **WHEN** a helper such as `translate(m, v)` is called
- **THEN** the input matrix `m` is unchanged and the result is a new plain
  array

#### Scenario: Degrees everywhere
- **WHEN** `efx.mat4.rotate(identity, 90, [0, 1, 0])` is applied to
  `[1, 0, 0]` (w = 1)
- **THEN** the result is approximately `[0, 0, -1]` — a quarter turn taken
  as 90 degrees, not radians
