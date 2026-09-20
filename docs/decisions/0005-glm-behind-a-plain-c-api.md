# 0005 — GLM is the math library, wrapped behind a plain C API

Status: Accepted (F1; change `2026-09-19-f1-player-skeleton`); not yet
integrated — first use is F3

## Context

The roadmap assigns the math-library decision to F1, but F1 ships no
math code; the first consumer is F3 (3D core: camera, meshes, matrix
math). GLM is C++-only while the core is C11 (ADR 0001), so the language
mismatch had to be resolved at decision time, not integration time.

## Decision

**GLM** is the engine's math library. When F3 introduces it, GLM usage
lives in small C++-compiled translation units exposing a plain C API to
the core — the same module-wall pattern as ADR 0003. GLM sources are not
vendored until first use (ADR 0006).

## Consequences

- The core consumes math only through the C wrapper surface; GLM types
  never cross into C11 translation units.
- If the wrapper cost proves unjustified in F3, the decision is
  revisited via a change proposal — no sunk cost exists today.

## Rejected alternatives

- Integrating GLM in F1: rejected — no consumer exists until F3;
  recording the decision without the dependency keeps F1 lean.
- Rewriting the core in C++ to use GLM directly: rejected in ADR 0001.
