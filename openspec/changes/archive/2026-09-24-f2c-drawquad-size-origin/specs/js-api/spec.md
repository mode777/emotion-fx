# Spec Delta

## MODIFIED Requirements

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
