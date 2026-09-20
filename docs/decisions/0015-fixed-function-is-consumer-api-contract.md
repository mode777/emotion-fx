# 0015 — Fixed-function is a consumer-API contract; internals use Sokol's programmable pipeline

Status: Accepted (2026-09, change `js-api-reference`) — clarification of
vision.md's "fixed function pipeline – no programmable shaders"

## Context

vision.md's shader ban has been read literally — including during the
`js-api-reference` review, where it produced the wrong wording in ADR 0014
("GPU-side skinning: impossible by constraint"). The literal reading is
also unsatisfiable: Sokol (ADR 0003) has no fixed-function mode at all;
every draw in the engine already goes through programmable-pipeline
shaders. The constraint was always meant at the consumer-API level and is
now stated that way.

## Decision

- **The consumer-facing API is fixed-function, forever**: scripts can
  never create, modify, upload, or select shaders. The rendering surface
  stays canned — fixed lights/limits, the Phong material model, blending
  modes, post FX. No shader-shaped feature (`onShader`, material shaders,
  effect passes users author) ever enters the `js-api` surface.
- **The internal implementation MUST use Sokol's programmable pipeline**
  with engine-owned canned shaders implementing that fixed-function model.
  The single-mega-shader vs build-time-permutations choice remains F4's
  deferred decision (`feature-roadmap`); it is purely internal and cannot
  leak into the script API.
- **Review test:** a feature may use programmable features internally
  freely; it is rejected only if it would grow a shader-shaped surface on
  the consumer API. "No shaders" is not, by itself, a valid argument
  against an internal implementation choice.
- Decision records must cite the real rejection reasons: e.g. GPU-side
  canned-shader skinning is *permitted* by this contract but was rejected
  for CPU skinning on engineering economics — F4 permutation surface, new
  joint-palette render state, no performance need at PS2-era scene sizes,
  and CPU-reference testability (ADR 0014, as corrected).

## Consequences

- vision.md, AGENTS.md, `openspec/config.yaml` context, and
  `docs/js-api.md` carry the clarified wording; ADR 0014's
  "impossible by constraint" phrasing is corrected to the economics-based
  rejection.
- Future ADRs and proposals must not cite the shader constraint against
  internal implementation choices — only against consumer-API surface.
- The F4 canned-shader strategy debate is exclusively an internal
  engineering decision; its outcome is invisible to scripts by contract.

## Rejected alternatives

- **Literal no-shaders-anywhere**: unsatisfiable on Sokol; pursuing it
  would mean abandoning the four-target rendering stack.
- **Consumer shader hooks** (script-authorable shaders or material
  graphs): rejected — exactly the surface this ADR closes, permanently.
