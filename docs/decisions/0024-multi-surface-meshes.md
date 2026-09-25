# 0024 — Meshes are multi-surface; materials bind per surface; no global material state

Status: Accepted (2026-09, change `f3-3d-core`)

## Context

The F3 proposal review (with the product owner, before any proposal text
was written) reshaped the provisional single-attribute-set mesh contract:
meshes should work like Godot meshes — a mesh holds multiple surfaces, each
surface owning its attribute arrays and its material. The provisional F4
contract had answered the material question the other way: one global
`setMaterial` call. Deciding now, at F3, matters because the data model
determines the F4 material system and the F6 glTF import mapping, and
changing it after either milestone ships would break scripts.

## Decision

- A MeshData holds 1..16 **surfaces** (fixed limit; vision.md limits
  table). Each surface is one set of attribute arrays plus optional
  indices — a Godot surface / glTF primitive. A Mesh uploads every surface;
  `drawMesh` plays every surface back in surface order. There is no
  single-surface draw (split meshes instead).
- **Materials bind to surfaces, never to engine state.** The provisional
  global `setMaterial` is removed from the F4 contract and never ships.
  From F4, `efx.setMeshSurfaceMaterial(mesh, surfaceIndex, mat)` (Godot
  `surface_set_material` analog; efx-verb, resource-first-arg — the
  `poseMesh` precedent) binds a snapshot of a JS-managed material to one
  surface; surfaces without a binding render with an engine default
  material. `createMeshData` accepts a parallel `materials` array from F4
  for creation-time bindings.
- Surface attribute arrays are stored engine-side (deep-copied CPU
  MeshData), and the GPU upload interleaves every attribute slot
  (pos/normal/uv/color, defaults filled) so F4 lighting/maps add shader
  and uniform work only — no second layout migration.
- glTF import mapping (ADR 0014): `mesh.primitives[]` → **surfaces** of one
  MeshData; per-primitive material → its surface's binding. Weights stay
  per-primitive attributes (`JOINTS_0`/`WEIGHTS_0`).

Full process record: `openspec/changes/archive/2026-09-25-f3-3d-core/`.

## Consequences

- F4 cannot introduce global material state later — surfaces always carry
  their own bindings, and scene drawing is stateless with respect to
  materials (display-list records stay self-contained; ADR 0019).
- F6 glTF import is structural (primitive → surface) with no
  re-plumbing between formats; multi-material assets load as one Mesh.
- The 16-surface cap joins the fixed limits; scripts that need more
  surfaces split meshes.
- Per-draw material overrides (`drawMesh({ material })`) were rejected;
  if a need appears, it must arrive as an additive option that beats
  surface bindings, documented as a `js-api` delta.

## Rejected alternatives

- **Keep the single-attribute-set mesh + global `setMaterial`**: lost —
  multi-material models need N meshes and a material swap per draw, and the
  global state fights the value-snapshot display list; the provisional F4
  contract would have shipped an API reshaped twice (F4, F6).
- **Godot-style mutable builder (`meshData.addSurface(...)`)**: lost —
  mutability after upload raises surface-invalidation questions a
  fixed-function PS2-era engine does not need; batch construction is
  immutable and matches the existing `createImageData`/option-bag style.
- **Method form `mesh.setSurfaceMaterial(i, mat)`**: deferred on naming
  grounds — the efx-verb form follows `poseMesh(mesh, …)`; the class
  machinery (ADR 0011) allows adding methods later without breaking call
  sites.
- **Uncapped surface count**: lost — fixed limits keep budgets
  deterministic (vision.md ethos); glTF meshes rarely exceed a handful of
  primitives.
