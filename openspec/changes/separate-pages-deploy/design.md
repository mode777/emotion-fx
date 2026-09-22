# Design

## Context

See `proposal.md` — Why. Current state that shapes the approach:

- `.github/workflows/ci.yml` triggers on `push.tags: ['v*']` and
  `workflow_dispatch`. Its jobs are `native` (Linux/Windows/macOS matrix),
  `emscripten`, `golden-web`, `generate-goldens` (manual-only), `release`
  (tag-only), and `pages` (`startsWith(github.ref, 'refs/tags/v') ||
  github.event_name == 'workflow_dispatch'`).
- The `pages` job checks out, installs `setup-emsdk@v14` with
  `version: latest`, builds the `player_web` target, copies
  `player_web.html` → `pages/index.html` plus `.js`/`.wasm`, uploads a Pages
  artifact, and runs `actions/deploy-pages@v4` under
  `environment: github-pages`.
- The `github-pages` environment's deployment branch policy permits only the
  default branch, so any `pages` job on another ref fails before its steps
  run. ADR 0023 records this and notes the (repository-setting) `v*` tag
  policy that was added for tag deploys.
- ADR 0023 decided to keep `pages` inside `ci.yml` and explicitly rejected a
  separate workflow ("more surface for no behavioral gain"). That calculus
  changed once `workflow_dispatch` became the agent verification entry point:
  the deploy job now fails on exactly the refs used to prove the gate.
- No source, CMake, or test changes are involved; this is workflow + docs.

## Goals / Non-Goals

**Goals:**

- A manual gate run on any ref can be green; the gate's status reflects
  verification only.
- Pages still deploys automatically (on `main`) and on demand, with the same
  built web player.
- One documented, version-controlled place owns Pages deployment.

**Non-Goals:**

- Changing the gate's triggers, jobs, artifacts, or the `release` job.
- Changing golden-image jobs/tolerance/determinism.
- Introducing CI-on-push/PR validation.
- Editing repository settings.

## Decisions

### D1: A dedicated `pages.yml`, triggered by default-branch push and manual dispatch

New `.github/workflows/pages.yml` owns the build-and-deploy steps currently in
`ci.yml`'s `pages` job. Triggers: `push.branches: [main]` and
`workflow_dispatch`. The job is gated with `if: github.ref ==
'refs/heads/main'`, so a manual dispatch from a feature branch skips the job
(a no-op) instead of hitting the protected environment — the failure mode that
motivated the change.

*Rejected:* keeping `pages` in `ci.yml` with a default-branch-only `if`. It
stops the false red but leaves a deployment job in the gate, so a Pages
outage or policy change still reddens a `main`/tag gate run; separation makes
the gate's status unambiguous.
*Rejected:* triggering `pages.yml` only on `v*` tags (preserving ADR 0023's
cadence). It keeps deployment coupled to the release run and means the public
demo does not track `main`; the chosen cadence makes the demo follow the
working branch, which is the point of a Pages site for this repo.

### D2: Remove the `pages` job from `ci.yml`

With D1, `ci.yml` keeps only verification jobs plus the tag-only `release`
job. A manual feature-branch dispatch then contains `native` ×3, `emscripten`,
`golden-web`, and `generate-goldens`, all of which can succeed on that ref, so
the run's conclusion reflects the gate.

### D3: Pages cadence moves to `main`; amend ADR 0023 in place

ADR 0023 states Pages deploys on "tag or manual dispatch" and lists a separate
workflow under Rejected alternatives. Both change. The record is amended in
place (status note + updated Pages clause + moved rejected alternative) rather
than superseded, because the cross-cutting decision it encodes — gate triggers
and downloadable artifacts — is untouched; only the Pages placement/cadence
changes. This matches the amendment precedent (ADR 0016).

### D4: Pin the Emscripten toolchain in `pages.yml`

The current `pages` job uses `setup-emsdk@v14` with `version: latest`. The
separate workflow pins `3.1.64`, the version used by every other CI job, so
the deployed web player is built reproducibly. This is not the golden gate, so
ADR 0020 does not require it, but the pin removes a silent drift source.

### D5: No unmanaged-resource implications

This change adds no script-facing API and no native handles, so the ADR
0011/0012 exposure rule (GC-finalized opaque classes vs fixed banks) does not
apply.

## Risks / Trade-offs

- **A Pages workflow run per push to `main`** → one lightweight job
  (Emscripten build + deploy); acceptable, and it is what makes the demo
  track `main`. If runner cost matters later, the trigger can be narrowed.
- **`workflow_dispatch` requires the workflow on the default branch** → until
  this change is merged, `pages.yml` cannot be dispatched from a feature
  branch; the push trigger also only fires once on `main`. Verification
  before merge uses `gh workflow view`/YAML inspection; after merge the
  triggers are live.
- **Protected environment still restricts non-default refs** → the job gate
  (D1) makes those dispatches no-ops rather than failures.
- **Duplicated Emscripten build** (`ci.yml` `emscripten` job and
  `pages.yml`) → accepted; the gate builds `player` for tests/artifacts, Pages
  builds `player_web` for deployment. Merging them would re-couple deploy to
  the gate, the thing this change removes.
- **The `github-pages` `v*` tag policy is no longer needed** → leaving it in
  place is harmless; it is a repository setting and not version-controlled.
- **Removing the job could strand Pages if `main` never moves** → `main` is
  the active branch, and manual dispatch remains available on it.

## Migration Plan

1. Merge the change to `main`: `pages.yml` becomes dispatchable and its push
   trigger goes live; the next `main` push deploys.
2. `ci.yml` loses its `pages` job; nothing else changes.
3. Rollback: re-add the `pages` job to `ci.yml` (and optionally delete
   `pages.yml`); no data or state migration exists.

## Open Questions

- Whether to eventually narrow the Pages trigger (e.g. path-filter to
  web-player inputs) is deferrable and does not change this change's specs or
  tasks.
