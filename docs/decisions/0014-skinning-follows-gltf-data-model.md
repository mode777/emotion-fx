# 0014 — Skinning follows the glTF data model; seven resource types

Status: Accepted (2026-09, change `js-api-reference`); the glTF data
mapping stands, but the resource-exposure aspect (Skeleton/Animation as
script resources) is superseded by 0017 — rigs and clips are implicit Mesh
payload
Supersedes: 0013

## Context

ADR 0013 pinned an eight-type resource taxonomy with **Skin** (bone
weights) as a first-class resource. Review questioned the split, and glTF
2.0 — the de facto interchange standard and the likely F6 asset format —
resolves it: per-vertex joint indices and weights are **mesh primitive
attributes** (`JOINTS_0`/`WEIGHTS_0`, four influences), while the glTF
`skin` object holds only the **joint list plus `inverseBindMatrices`**, and
the mesh–skeleton link lives on the node (`node.mesh` + `node.skin`).
Following glTF keeps our in-memory model isomorphic to the interchange
format, so import is near zero-plumbing.

## Decision

The taxonomy is seven types — **Skin is removed**; weights return to mesh
data, mirroring glTF:

| Type | Contents | Side |
|---|---|---|
| MeshData | Attributes (incl. optional `joints`/`weights` for skinned meshes) + indices | CPU |
| ImageData | Raw pixel data + size + format | CPU |
| Skeleton | Joint hierarchy + inverse bind matrices (≈ glTF `skin`) | CPU |
| Animation | Channels, keyframes and transforms | CPU |
| Mesh | GPU memory bound mesh resource | GPU |
| Texture | GPU texture resource | GPU |
| RenderTarget | GPU render target resource | GPU |

- All seven remain GC-finalized opaque classes (ADR 0011) under the ADR
  0012 discipline, fully opaque at first (`destroy()` only; methods,
  getters, setters reserved).
- **`efx.setSkin(skel, mesh)`** is the `node.mesh + node.skin` equivalent:
  it binds a Skeleton to a skinned Mesh. Rebinding swaps skeletons — the
  glTF "same mesh, different skin per node" case maps to rebinding or to a
  second Mesh created from the same MeshData.
- What we deliberately do **not** copy: glTF skins at draw time in a vertex
  shader. The consumer-facing pipeline stays fixed-function (ADR 0015) and
  CPU skinning was chosen on engineering grounds — no F4 canned-shader
  permutations, no joint-palette render state, no performance need at
  PS2-era scene sizes, direct CPU-reference testability; posed vertices
  are written **in place** into the bound Mesh, so one Mesh holds one
  current pose.

## Consequences

- F6 import of glTF assets is structural: primitives → MeshData (attributes
  incl. `JOINTS_0`/`WEIGHTS_0`), `skins[]` → Skeleton, `animations[]` →
  Animation — no weight re-plumbing between formats.
- One fewer resource type to implement, document, and finalize.
- In-place posing means a Mesh cannot simultaneously hold two poses; two
  poses of one model require two Mesh objects (acceptable at PS2-era scene
  sizes).
- Skeleton, not "Skin", is the animation target — `playAnimation`/
  `blendAnimations` operate on Skeletons, consistent with glTF channels
  targeting joint nodes.

## Rejected alternatives

- **First-class Skin resource (ADR 0013)**: lost — deviates from the glTF
  data model, forcing weight re-plumbing on import; the reuse benefit it
  offered is available through `setSkin` rebinding and duplicate Mesh
  objects.
- **GPU-side skinning via canned shaders**: rejected on engineering
  economics, not impossibility — the consumer API never exposes shaders
  (ADR 0015), and internally it would inflate the F4 canned-shader
  permutation surface, add joint-palette render state to the display
  list, and buy throughput PS2-era scene sizes do not need (CPU-reference
  testability also favors CPU skinning).
