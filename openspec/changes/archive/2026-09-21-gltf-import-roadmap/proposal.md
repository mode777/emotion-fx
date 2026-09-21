# Proposal: gltf-import-roadmap

## Why

The roadmap defers the asset-format decision to F6, but the codebase has
already committed to glTF as the data model (ADR 0014: skinning follows the
glTF data model; ADRs 0017/0018 assume rigs and clips arrive from glTF
assets). Leaving "which asset format" nominally open past that point is a
fiction that forces every load* signature, the F6 zip layout, and F7's
import path to stay provisional for no reason. Relatedly, pinning glTF makes
it obvious that several milestones will need third-party C libraries (image
codec for the F2 golden-image harness; glTF loader, image decoder, and zip
reader for F6), yet the roadmap has no requirement that such dependencies be
evaluated before implementation starts.

## What Changes

- **Pin glTF 2.0 as the import format** in the roadmap's F6 scope:
  "real asset import (asset format decided here)" becomes "glTF 2.0 asset
  import (meshes, images, skins, animation clips — profile decided in F6)".
- **Narrow the F6 deferred decision** from "asset format" to "glTF profile":
  .glb vs .gltf container, allowed glTF extensions, image embedding —
  settled by the F6 change per ADR 0014's data mapping.
- **Add a third-party-dependency evaluation requirement** to the roadmap:
  any proposal that introduces a new third-party dependency MUST name
  candidate libraries and settle the choice at proposal time — before any
  implementation — against: license, vendoring fit (ADR 0006 pinned source
  snapshots), four-target coverage including Emscripten, and C11 fit
  (ADR 0001). First customer is F2 (golden-image harness image I/O).
- **Docs syncs**: AGENTS.md (roadmap table F6 row, deferred-decisions
  paragraph, plus a note that OpenSpec commands run via `npx openspec …`
  since the CLI is not on PATH), `docs/js-api.md` (asset-format open
  question and the F6 "signatures final once the asset format is decided"
  note updated to the narrowed glTF-profile wording).
- No code changes in this change; it amends the roadmap capability and docs
  only.

Milestone placement: this change **amends the roadmap capability itself**
(`feature-roadmap`) rather than implementing one milestone. The glTF scope
it pins lands in **F6**; the dependency-evaluation policy applies from the
next dependency-introducing proposal onward — **F2** is the first.

## Capabilities

### New Capabilities

(none)

### Modified Capabilities

- `feature-roadmap`: F6 scope pins glTF 2.0 asset import and narrows the
  F6 deferred decision to the glTF profile; new requirement that proposals
  introducing third-party dependencies evaluate candidates at proposal
  time (license, vendoring per ADR 0006, four-target coverage, C11 fit).

## Impact

- `openspec/specs/feature-roadmap/spec.md` — requirement and scope changes
  (via this change's delta).
- `AGENTS.md` — roadmap table F6 row, deferred-decisions paragraph, new
  `npx openspec` invocation note.
- `docs/js-api.md` — open-questions section ("Asset format") and F6
  provisional note reworded; no signatures change (they stay provisional
  until the F6 profile decision).
- Downstream (future changes, not this one): F2 harness needs an image
  read/write library; F6 needs glTF loader, image decoder, zip reader —
  all vendored per ADR 0006 and evaluated at proposal time per the new
  policy.

**No ADR** — the glTF data model is already pinned (ADR 0014); the
remaining glTF-profile decision is deliberately deferred to F6 and recorded
in the roadmap, and the dependency-evaluation policy is roadmap process,
not a cross-cutting architecture trade-off.

## Non-goals

- Implementing glTF loading or the zip root — that is the F6 change's work.
- Choosing specific libraries now (cgltf vs tinygltf vs fastgltf, image
  codec, zip reader) — evaluations happen in the proposals that need them,
  per the new policy.
- The ADR 0016 hook-registration runtime change and its `player-runtime` /
  `js-runtime` spec deltas — separate change, own verification.
- Archiving the completed `js-api-reference` change — housekeeping, done
  separately.
- Any change to `vision.md` — glTF is an implementation-format commitment;
  vision stays at the intent level.
