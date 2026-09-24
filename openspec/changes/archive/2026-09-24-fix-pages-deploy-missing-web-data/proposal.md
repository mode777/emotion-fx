# Proposal

**Roadmap position:** Cross-cutting CI/deployment repair — no feature
milestone. It fixes a regression in the Pages deployment workflow owned by
the `verification` capability (ADR 0023, as amended by
`separate-pages-deploy`); it does not start or reopen a ladder milestone.

## Why

The public demo at <https://mode777.github.io/emotion-fx/> is broken: the
browser requests `player_web.data`, gets a 404, and the Emscripten loader
throws (`Uncaught Error` at `xhr.onload`), so the demo never starts. Root
cause (verified in the repo history and against the live site): the
`player_web` target embeds the `examples/browser` resource root via
`--preload-file`, so the build emits a fourth file, `player_web.data`,
which its JS glue fetches at startup. The original deploy step copied all
four files (6242388); the `ci-tag-releases` change (d6f554c) accidentally
dropped `player_web.data` from that copy list, and `separate-pages-deploy`
(ba7dc69) moved the already-truncated list into `.github/workflows/pages.yml`.
The live site confirms the symptom: `player_web.js` and `player_web.wasm`
return 200, `player_web.data` returns 404. The downloadable web-bundle
artifact is unaffected — it packages all four files with an existence
check; only the Pages collect step misses the `.data` file.

## What Changes

- **Restore `player_web.data` to the Pages deploy content.** The
  "Collect pages content" step in `.github/workflows/pages.yml` copies
  `player_web.html` → `index.html` plus `.js` and `.wasm`; it shall copy
  `.data` as well, so the deployed demo loads its preloaded resource root.
- **Fail loudly on an incomplete file set.** The collect step shall
  verify each expected file exists before copying (same guard the
  web-bundle packaging step in `ci.yml` already uses), so a future file-set
  drift fails the deploy job at build time instead of silently publishing a
  demo that 404s at runtime.

**Assumption recorded:** the fix is confined to `pages.yml`'s collect
step. The build itself is correct — the Emscripten output set
(html/js/wasm/data) matches what the loader fetches, and the web-bundle
artifact added by the same `ci-tag-releases` change already lists all four
files correctly. The bug is purely in which of those files Pages deploys.

**Non-goals:**

- No change to the gate workflow (`ci.yml`): jobs, triggers, release
  packaging, or the four downloadable artifacts.
- No change to the `player_web` build flags or output shape — in
  particular, not switching to `-sSINGLE_FILE=1` or `--embed-file`
  (see design.md for rejected alternatives).
- No change to Pages triggers or environment gating (push to `main` +
  manual dispatch gated to the default branch stay as they are).
- No change to web player code (`src/web/`, `src/platform/web_pre.js`) or
  to the demo content (`examples/browser/main.js`).
- No ADR — the deployment decision in ADR 0023 (as amended) is unchanged;
  this restores behavior the current spec already requires. `docs/js-api.md`
  is unaffected (no script-facing API delta).

## Capabilities

### New Capabilities

None.

### Modified Capabilities

- `verification`: the "Separate Pages deployment workflow" requirement is
  tightened so the deployed Pages content SHALL be the complete
  `player_web` output set (HTML loader, JS glue, wasm, preload data), and
  the deploy SHALL fail (not silently publish) when any expected file is
  missing — closing the exact loophole this regression fell through.

## Impact

- **Workflows:** `.github/workflows/pages.yml` only — the collect step's
  file list gains `player_web.data` plus an existence check.
- **Specs:** delta against `openspec/specs/verification/spec.md`
  (Separate Pages deployment workflow requirement).
- **Docs:** none — no ADR, no `docs/js-api.md` change; the deployment
  invariant this fixes is already owned by the spec, not by an ADR.
- **APIs / source / tests:** none — no `src/`, `tests/`, or CMake changes.
  Verification is workflow-level (YAML validity, green Pages run, live-site
  file checks), not ctest/golden-level; the four-target gate is unaffected
  because no code it builds changes.
