# 0004 — Every engine function hangs off one global `efx` namespace

Status: Accepted (F1; change `2026-09-19-f1-player-skeleton`)

## Context

Scripts need a discoverable, collision-free surface for engine
functionality, and the layering rule (low/mid-level in C, high-level
conveniences in pure JS — `vision.md`) needs one place where both layers
meet.

## Decision

All engine-provided functions are registered as C callbacks on a single
global object `efx` (`efx.log`, `efx.quit`, `efx.args` in F1). Every
future low/mid-level function (`drawQuad`, `setMaterial`, …) follows the
same pattern; high-level conveniences (`drawModel`, `drawText`) are
engine-provided pure JS built on the public `[C]` surface, also exposed
via `efx`. The catalog is `docs/js-api.md`; script-facing changes
require a `js-api` spec delta plus a `docs/js-api.md` update in the same
change.

## Consequences

- Scripts import nothing; the whole surface is greppable as `efx.`.
- The namespace name is a compatibility contract — renaming it later is
  a breaking change to every script.
- No quickjs types leak past the binding trampolines (`api` module,
  ADR 0003).

## Rejected alternatives

- Per-function globals or module imports: rejected — scripts must stay
  import-free with zero non-ES6 dependencies, and one namespace keeps
  the C registration, the layering seam, and the docs catalog in one
  place.
