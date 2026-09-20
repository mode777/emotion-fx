# 0013 — Resource taxonomy: eight GC-finalized opaque types

Status: Superseded by 0014 (2026-09, change `js-api-reference`); the
opaque-class model and the MeshData/ImageData promotion remain valid —
only the Skin type and the weights placement it implied are superseded

## Context

ADR 0011 established *how* dynamic-count resources reach scripts
(GC-finalized opaque classes) but left the type set as an open example
list. Before F2 implements the first class, the definitive taxonomy must be
pinned: which types exist, what each contains, and which side of the
CPU/GPU boundary it lives on. Two earlier sketch choices are also revisited
here: mesh data and pixel data were plain JS objects, and skin weights were
vertex attributes inside mesh data.

## Decision

Eight resource types are exposed as GC-finalized opaque JS classes
(ADR 0011) under the ADR 0012 memory discipline:

| Type | Contents | Side |
|---|---|---|
| MeshData | Attributes and indices for a mesh | CPU |
| ImageData | Raw pixel data + size + format | CPU |
| Skin | Bone weights | CPU |
| Skeleton | Skeleton hierarchy | CPU |
| Animation | Channels, keyframes and transforms | CPU |
| Mesh | GPU memory bound mesh resource | GPU |
| Texture | GPU texture resource | GPU |
| RenderTarget | GPU render target resource | GPU |

- **Pipeline:** `createMesh(meshData)` uploads CPU → GPU Mesh;
  `createTexture(imageData)` uploads CPU → GPU Texture. `load*` (F6+) may
  return either stage.
- **Fully opaque for now:** the only member of every type is `destroy()`.
  Methods, getters and setters are explicitly reserved for later — the
  class machinery (ADR 0011) supports adding them without changing call
  sites.
- **Construction inputs are plain option objects**; the *results* are
  opaque.
- **Not in the taxonomy:** materials stay plain JS objects; the F8 font is
  a pure-JS construct (atlas Texture + quad layout) in the `[JS]` layer,
  not a native resource.

## Consequences

- CPU-side native bytes (MeshData attributes/indices, ImageData pixels)
  count toward GC pressure under ADR 0012, same as GPU bytes.
- Scripts can free upload sources early: `destroy()` on MeshData/ImageData
  after the GPU upload releases native buffers deterministically; the
  finalizer covers the rest.
- Future `js-api` deltas extend this taxonomy only through the
  classification rule (new type = explicit decision), keeping the type set
  closed and reviewable.
- Adding query methods/getters/setters later is additive per type and
  requires no API-shape change.

## Rejected alternatives

- **Keeping MeshData/ImageData as plain JS objects**: lost — they can be
  large (megabytes of pixels or vertex data); opaque classes let the engine
  own the storage, count it toward GC pressure (ADR 0012), and later add
  in-place methods (transform, resize, format conversion) without changing
  call sites.
- **Skin as vertex attributes inside mesh data** (previous sketch): lost —
  a first-class Skin allows reuse across meshes, swapping skins on one
  mesh, and clean F6 import; it also keeps the Skeleton/Animation/Skin
  import trio symmetric.
- **A native Font type**: lost for now — the atlas-on-quads font is
  expressible in the public `[JS]` layer over Texture (vision.md F8 scope);
  a native font type would import rasterizer-scale concerns the
  fixed-function core does not need.
