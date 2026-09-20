# 0001 — C11 core with a C ABI for the script-facing API

Status: Accepted (F1; change `2026-09-19-f1-player-skeleton`)

## Context

The player embeds a C-based ES6 interpreter (quickjs-ng) and must build
on four toolchains (MSVC, clang/gcc, Emscripten). The JS-facing API is a
set of C callbacks registered on a host object, so the boundary is a C
ABI regardless of what language the interior uses. The language choice
had to be made once, up front, because every module lands inside it.

## Decision

The core is written in **C11** (CMake ≥ 3.21, warnings-as-errors on the
core — never on vendor code). Every script-facing function is a C
function exposed through the binding layer (ADR 0004). C++ appears only
in dedicated, walled translation units that expose a plain C API — the
first will be the GLM math wrapper (ADR 0005).

## Consequences

- All engine modules stay C11; no exceptions without a new ADR.
- C++ libraries (GLM in F3) pay a small wrapper cost instead of
  infecting the core.
- The Emscripten bridge compiles the same C core; only the platform
  layer differs (ADR 0003).

## Rejected alternatives

- C++ core: buys direct GLM use, but the JS boundary is a C ABI
  regardless (quickjs is a C library), so C++ adds toolchain and ABI
  complexity without removing the one wrapper that actually matters.
