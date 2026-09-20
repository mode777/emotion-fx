# 0007 — Headless `--script` mode with an exit-code contract

Status: Accepted (F1; change `2026-09-19-f1-player-skeleton`)

## Context

The F1 verification gate — and every later milestone's automated checks
— must run on machines with no display, and assertions need a
machine-readable success signal that behaves identically across the four
targets (stdout/stderr flushing differs between them).

## Decision

The player has two run modes:

- `player <resource-root>` — evaluate `main.js`, open the sokol window,
  run the frame loop (each frame: C dispatches JS `update`, then JS
  `render`); exit 0 on window close or `efx.quit(0)`. Hooks are looked
  up once after evaluation; missing ones are skipped.
- `player --script <file> [args…]` — **no sokol initialization at all**;
  evaluate the script and propagate its requested exit code.

Exit codes: `0` success; `1` generic runtime failure (missing
root/entry script, uncaught exception, file-not-found); otherwise the
script-requested code. Test scripts assert via exit codes only; log
output is diagnostic, never an assertion.

## Consequences

- Every milestone's automated gate is expressible as scripts + exit
  codes, identical on Win/Linux/macOS/Emscripten.
- Cross-target flushing differences cannot corrupt test results.
- Windowed frame-loop behavior is not covered by automated tests until
  F2's golden-image harness; until then it stays a manual per-platform
  checklist.

## Rejected alternatives

- Windowed script mode in F1: rejected — no draw API exists yet to
  justify it.
