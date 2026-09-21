# Design: gltf-import-roadmap

## Context

The roadmap (`openspec/specs/feature-roadmap`) defers the asset-format
decision to F6, but ADR 0014 already fixed glTF as the data model and ADRs
0017/0018 assume glTF-shaped import payloads. `docs/js-api.md` keeps every
`load*` signature provisional on the undecided "asset format", and nothing
in the repo requires third-party C libraries to be evaluated before they are
vendored — while F2 (golden-image harness image I/O) and F6 (glTF loader,
image decoder, zip reader) are both about to need them. This change edits
roadmap and docs only; see proposal.md for motivation, the delta spec for
the requirement changes.

## Goals / Non-Goals

- Goals: make the roadmap state the truth (glTF 2.0 is the import format);
  shrink the F6 open decision to the profile; create the proposal-time gate
  that future dependency-introducing changes must pass; keep AGENTS.md and
  `docs/js-api.md` consistent with the spec; document the `npx openspec`
  invocation (CLI not on PATH).
- Non-Goals: no library selection, no code, no ADR 0016 hook runtime
  change, no archiving of `js-api-reference` (proposal Non-goals).

## Decisions

### D1 — Pin glTF 2.0 in the roadmap; defer only the profile to F6
The F6 scope line in "Fixed milestone order" names glTF 2.0 asset import
(meshes, images, skins, animation clips); "Early risk retirement" gains the
sentence that pins the format at roadmap level and defers only container,
extensions, and image embedding to the F6 change.
- *Why*: ADR 0014 already committed the data model; keeping the format
  nominally open forces `load*` signatures, the F6 zip layout, and F7
  import planning to stay provisional for no benefit. The profile genuinely
  needs F6 work (asset corpus, loader evaluation) and stays there.
- *Rejected — decide format + profile now via ADR*: premature; profile
  choices (e.g. which extensions) are meaningless before a loader is
  evaluated. *Rejected — leave everything to the F6 proposal*: the roadmap
  would keep advertising a decision that has effectively been made.

### D2 — Dependency-evaluation policy lives in the roadmap spec, not an ADR
The gate is process (when proposals are reviewed), and the roadmap spec
already owns proposal gating ("Proposals declare roadmap position"); the
policy sits beside it as "Third-party dependency evaluation at proposal
time".
- *Rejected — ADR*: ADRs record architecture trade-offs; this is a workflow
  gate. If a *specific* library choice later sets a durable precedent (e.g.
  "single-header C only"), that lands in an ADR inside the change that
  chooses it.
- *Rejected — AGENTS.md convention only*: AGENTS.md points rather than
  restates; an unenforced convention would not gate proposals.

### D3 — Fixed evaluation criteria
License, vendoring fit (pinned source snapshots, ADR 0006), four-target
coverage including Emscripten, C11 compatibility (ADR 0001). The criteria
are the four house constraints every dependency must satisfy anyway;
evaluations are expected to be one paragraph per candidate for
single-header libraries, not spikes.

### D4 — `npx openspec` note in AGENTS.md
One short block stating the OpenSpec CLI is not on PATH: run `npm install`
once, then `npx openspec …` from the repo root. Placed with the
OpenSpec-related guidance already in AGENTS.md.

## Risks / Trade-offs

- [Policy feels ceremonial for tiny deps] → single-header libraries get a
  one-paragraph evaluation; the requirement is recording the four checks,
  not a study.
- [glTF pin could age badly if the profile decision reveals a format-level
  problem] → the profile is explicitly F6 scope; format-level changes go
  through a roadmap change, which is the designed escape hatch.
- [Docs drift: spec, AGENTS.md, js-api.md say different things about the
  asset format] → one tasks sweep updates all three in this change;
  AGENTS.md points, js-api.md's open question links the narrowed decision.

## Migration Plan

Docs-only change; no runtime migration. Land the spec delta + docs syncs
together; revert is a plain git revert.

## Verification

No code changes, so the four-target ctest gate is not exercised. This
change's gate is `npx openspec validate --strict` plus a docs
cross-check (roadmap spec ↔ AGENTS.md table/deferred list ↔ js-api.md open
question all agree).

## Open Questions

None — profile choices (container, extensions, embedding) are deliberately
F6 scope and the library candidates are deliberately first evaluated in the
F2/F6 proposals.
