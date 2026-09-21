# 0017 — Skins and skeletons are implicit Mesh payload; `skinned` is a drawMesh flag

Status: Accepted (2026-09, change `js-api-reference`)
Amends: 0014 (resource-exposure aspect only — the glTF data mapping stands)
Amended by 0018: the playback APIs (`playAnimation`/`pauseAnimation`/
`blendAnimations`) are replaced by script-driven `poseMesh`; implicit rig
payload and the `skinned` flag stand

## Context

ADR 0014 exposed Skeleton and Animation as script resources (7 types), with
`setSkin(skel, mesh)` binding and clip objects passed to playback. Review
asked for a simpler model: skins, skeletons, and animation clips should not
be script-visible resources at all — a skinned asset loads as one Mesh that
carries its rig and clips internally, and playback state lives inside the
Mesh. That raises one API-shape question: how does a draw request the posed
vertices — a `skinned` flag on `drawMesh`, or a separate
`drawMeshSkinned`?

## Decision

- **Five native resource types**: MeshData, ImageData, Mesh, Texture,
  RenderTarget. Skeleton, Animation (and already Skin, per 0014) are no
  longer script resources.
- **Implicit rig payload**: loading (or creating) a skinned mesh bundles
  its skin weights, skeleton hierarchy, inverse bind matrices, and
  animation clips *inside* the Mesh (import-time, glTF mapping per 0014
  unchanged). Playback state — current clip(s), time, blend weights, posed
  vertex buffer — lives in the Mesh.
- **Playback operates on the Mesh**: `efx.playAnimation(mesh, clip, opts?)`
  (`clip` = name or index; `{ loop?, speed? }`), `efx.pauseAnimation(mesh)`,
  `efx.blendAnimations(mesh, clipA, clipB, t)`.
- **`skinned` is a `drawMesh` option, not a second method**:
  `efx.drawMesh({ mesh, transform?, color?, skinned? })`. Absent/false
  draws the bind-pose (rest) buffer; `skinned: true` draws the current
  posed buffer. `skinned: true` on a mesh without a rig throws
  (`TypeError`, per the error conventions — Mesh is opaque, so there is no
  script-side pre-check).
- **Dual buffers for skinned Meshes**: the bind-pose vertex buffer is kept
  (immutable), CPU skinning writes the posed buffer only while playback
  state is active/dirty. Cost: ~2× vertex memory for skinned meshes —
  accepted.
- **Rationale for flag over method**: both variants share the identical
  option set and display-list record shape; a second function would
  duplicate every future draw option. Rest-vs-posed is a per-draw choice
  (like color), not an object identity — three.js/Godot put skin-ness in
  the object type because their skinning is GPU-intrinsic; with CPU posing
  the faithful equivalent is the draw-time flag. Consistent with the
  option-object convention (design D3) and the minimal-surface goal.

## Consequences

- Five fewer-to-maintain types: no Skeleton/Animation class machinery,
  finalizers, or taxonomy entries; F7 shrinks to playback + the flag.
- Scripts cannot inspect or share rigs across meshes (Mesh is opaque):
  sharing a rig means authoring it in the asset twice, or a future
  `js-api` delta re-exposing it — deferred until a real need appears.
- Procedural (non-imported) skeletons have no construction path in F7
  (`createMeshData` carries `joints`/`weights`, but rig/clips come from
  assets); deferred as an open question.
- `drawModel` (F8, high-level JS) forwards the `skinned` option.
- One-Mesh-one-pose (0014) is refined: bind pose is always retained; the
  posed buffer holds the single current pose.

## Rejected alternatives

- **Separate `drawMeshSkinned(...)`**: lost — identical signature and
  display-list record, so it only duplicates the API surface and every
  future option.
- **Keep Skeleton/Animation resources (0014 exposure)**: lost — the
  binding/clip bookkeeping is engine work users shouldn't repeat; import
  bundles it naturally, and playback-by-clip-name matches the
  old-school model authoring format.
- **Implicit auto-skinning when animations play** (no flag): lost — the
  same call drawing different buffers based on hidden playback state is
  unpredictable; the flag keeps per-draw behavior explicit.
