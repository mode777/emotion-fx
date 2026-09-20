# 0009 — GitHub Actions is the four-target gate runner

Status: Accepted (F1; change `2026-09-19-f1-player-skeleton`, decided
during apply)

## Context

The F1 gate must build and smoke-test on Windows, Linux, macOS and
Emscripten. The development container hosts none of the four toolchains
faithfully (no MSVC/Xcode, no X11, and emsdk needs ~3 GB the state
volume does not have), while CI runners have all of them natively.

## Decision

`.github/workflows/ci.yml` runs the gate on every push to `main` and on
every PR: a native matrix (`ubuntu-latest`, `windows-latest`,
`macos-latest`, with apt-installed X11/GL dev packages on Linux) plus a
separate Emscripten job (emsdk action). Each job builds with CMake and
runs the ctest smoke suite; the suite is headless (ADR 0007), so it
needs no display anywhere.

## Consequences

- Milestone verification gates are proven in CI; local runs cover only
  the toolchains the dev machine actually has.
- The Emscripten job tracks `latest` emsdk for F1; **pinning settles in
  F2** together with golden-image CI determinism, as the roadmap
  assigns.
- Golden-image tests (F2+) must stay deterministic on shared CI runners;
  software-rendering/lockstep decisions belong to the F2 change.

## Rejected alternatives

- Running the matrix in the dev container: the toolchains are not
  faithful (no MSVC/Xcode/X11) and emsdk does not fit the storage
  budget.
