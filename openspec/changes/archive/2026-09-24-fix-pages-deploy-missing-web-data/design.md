# Design

## Context

The `player_web` target (CMakeLists.txt) links with
`--preload-file=examples/browser@/examples/browser`, so Emscripten emits a
four-file output set: `player_web.html` (loader page), `player_web.js`
(JS glue), `player_web.wasm` (module), and `player_web.data` (the packed
resource root the glue fetches via XHR at startup). `.github/workflows/pages.yml`
deploys only the first three — a file list that has been wrong since
`ci-tag-releases` (d6f554c) dropped `.data` from the copy line and was then
moved verbatim into `pages.yml` by `separate-pages-deploy` (ba7dc69). The
same d6f554c change also introduced the downloadable web-bundle packaging
step in `ci.yml`, which lists all four files behind a `test -f` existence
check — so the correct complete-set list already exists in the repo, just
not in the Pages workflow. See proposal.md — Why for the live-site evidence.

Constraint: Pages deploys only from `main` (the deploy job is gated to the
default branch), so the fix cannot be observed from a feature branch; it
ships by merging to `main` and watching the resulting `pages` run.

## Goals / Non-Goals

**Goals:**

- Deployed Pages content is the complete `player_web` output set; the demo
  boots without resource-load errors.
- A future output-set drift fails the deploy job loudly at build time, not
  silently at the user's browser.
- Keep the fix a minimal, reviewable workflow edit.

**Non-Goals:**

- Not reworking the Pages workflow's triggers, gating, or environment
  (settled by `separate-pages-deploy` / ADR 0023).
- Not touching the gate workflow, build flags, web bridge code, or demo
  content.
- Not deduplicating the file list between `pages.yml` and `ci.yml` at any
  cost — see Decisions for why literal duplication was kept.

## Decisions

- **D1 — Restore `.data` via an explicit file list with an existence check.**
  The collect step checks each expected file (`test -f`, mirroring the
  `ci.yml` web-bundle step) and then copies `player_web.html` → `index.html`
  plus `.js`, `.wasm`, `.data`.
  *Why:* the failure mode being guarded against is a missing expected file;
  an explicit list plus existence check turns that failure into a red deploy
  job. *Rejected alternatives:*
  - **Glob copy (`cp build-web/player_web.*`)** — lost: it would also sweep
    in whatever else emscripten emits (e.g. `.map`, worker files), so Pages
    content becomes whatever the toolchain happens to produce; the bug class
    (silent divergence between built set and deployed set) stays open, just
    in the opposite direction.
  - **Derive the list from CMake (manifest file written at configure time)** —
    lost: adds a build-system coupling for a four-file set that has been
    stable since the web player was introduced; the bundle step's existing
    literal-list pattern is proven and already reviewable.
  - **`-sSINGLE_FILE=1` (embed wasm + data base64-inlined into the HTML)** —
    lost: eliminates the external files but inflates `index.html`, defeats
    http caching of the wasm, delays streaming wasm compilation, and changes
    the deployed artifact shape (and the bundle step's file set) for no
    product need.
  - **`--embed-file` instead of `--preload-file`** — lost: packs the demo
    into the wasm image instead of a sidecar, growing the module and slowing
    startup; it changes the output shape rather than fixing the deploy, and
    the same missing-file drift class remains.

- **D2 — No CI-gate impact; Pages run is the verification vehicle.** No
  `ci.yml` change and no ctest/golden job exercises a workflow YAML, so the
  four-target gate is intentionally not part of this change's verification;
  the `pages` run on `main` plus live-URL checks are (see tasks.md).
  *Rejected alternative:* adding a post-deploy smoke job that curls the four
  URLs inside `pages.yml` — deferred as overkill for now: the existence
  check already fails before deploy, and a follow-up can add a runtime
  check (e.g. headless Chrome load) if deploys regress again. Noted here so
  the deferral is deliberate, not accidental.

- **D3 — No ADR, no spec-level file-list churn.** The complete-output-set
  invariant now lives in the `verification` spec (delta); naming the
  concrete four files in the spec is consistent with the house style (the
  artifacts requirement already names the bundle contents "HTML loader,
  JS glue, wasm, and data"). A durable architecture decision is not being
  settled here, so no ADR; the design-rule item about unmanaged script-facing
  resources does not apply — this change touches no script-facing surface.

## Risks / Trade-offs

- [File list now lives in two places (`pages.yml` collect, `ci.yml` bundle
  step) and can drift again] → both copies carry a `test -f` guard, so any
  drift fails its job loudly; deduplication (shared script or manifest) is a
  deliberate follow-up, not part of this repair.
- [Emscripten upgrade changes the output file set (rename/new file)] → the
  existence check fails the Pages deploy immediately with a clear message;
  the fix is then a one-line list update in the same PR as the toolchain
  bump.
- [Cannot observe the deploy from a feature branch] → accepted Pages
  constraint (default-branch-only); verification plan merges to `main`,
  watches the run, and curls the four URLs; rollback is a revert commit
  followed by the next push-triggered deploy.
- [Stale CDN/browser cache after redeploy] → verification curls each URL
  with a cache-busting query; human check is a hard refresh.

## Migration Plan

1. Merge the `pages.yml` edit to `main` — the push itself triggers the
   Pages deploy of the fixed content; no data migration, no user action.
2. Rollback: revert the commit; the next push to `main` redeploys (back to
   the broken state, but nothing new breaks — the site is already broken
   today).

## Open Questions

None — the output set is pinned by the build, the fix site is one step in
one workflow, and verification is observable on `main`.
