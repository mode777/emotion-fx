# Proposal

**Roadmap position:** Cross-cutting CI infrastructure — no feature
milestone. It refines the trigger/deploy policy established by ADR 0023
(change `ci-tag-releases`) and applies to every gate from F2 onward; it
does not start or reopen a ladder milestone.

## Why

`pages` is a job inside `.github/workflows/ci.yml`. ADR 0023 re-gated it to
run on version tags **or** `workflow_dispatch` so removing the branch-push
trigger would not strand Pages deployment. That `workflow_dispatch` branch
is also the entry point agents use to prove the gate on a feature branch —
and the `github-pages` environment's deployment policy only permits the
default branch. The result: a manual verification run from a feature branch
runs the full four-target matrix successfully, then fails its `pages` job
immediately, so the whole run is reported as **failure** even though the
gate itself passed (observed on run `35737462655`, verifying
`explicit-hook-registration`). A deploy job that cannot succeed on the refs
used for verification does not belong in the verification workflow.

## What Changes

- **New `.github/workflows/pages.yml`** — the public web player's build and
  GitHub Pages deployment move here, with the steps currently in the `pages`
  job (checkout, Emscripten build of `player_web`, collect Pages content,
  upload pages artifact, `actions/deploy-pages`).
  - Triggers: `push` to the default branch (`main`) and `workflow_dispatch`.
  - The deploy job is gated to the default branch (`github.ref ==
    'refs/heads/main'`), so a manual dispatch from any other ref is a no-op
    instead of a protected-environment failure.
  - `setup-emsdk` is pinned to the same `3.1.64` used by the rest of CI,
    instead of the current `latest`.
- **Remove the `pages` job from `ci.yml`.** The gate run's status then
  reflects only the verification jobs (native matrix, Emscripten, golden
  jobs) plus the tag-only `release` job, so a manual feature-branch run can
  be green.
- **Pages cadence shifts to the default branch.** Pages now deploys when
  `main` moves (and on demand) rather than on version tags. This is a
  deliberate behavior change from ADR 0023's "tag or manual dispatch"
  clause: the public demo tracks `main`, and deployment is decoupled from
  the gate/release run.
- **Amend ADR 0023.** Update its Pages clause and move "moving Pages to its
  own workflow" out of Rejected alternatives; the trigger policy and
  artifact/release rules are unchanged. **No new ADR — amend
  `docs/decisions/0023-tag-triggered-ci-and-releases.md`**, because the
  cross-cutting trigger/artifact decision still stands and only the Pages
  placement/cadence changes.

**Assumption recorded:** the default branch is `main` (it is the branch
this repo works on); the Pages workflow's push trigger and job gate use
`main`. If a different default branch is adopted, both move with it.

**Non-goals:**

- No change to the four-target matrix, its jobs, or the gate's meaning.
- No change to `ci.yml`'s `v*`-tag/`workflow_dispatch` trigger policy.
- No change to the `release` job or the downloadable-artifact policy.
- No change to golden-image jobs, tolerance, or determinism (ADR 0020).
- No change to the web player's content or build; the same `player_web`
  target is deployed.
- No PR/CI-on-push validation is introduced; the single-branch, on-demand
  gate policy stands.
- No change to repository settings (the now-unneeded `github-pages` `v*`
  tag policy may be left in place; it is not version-controlled).

## Capabilities

### New Capabilities

None.

### Modified Capabilities

- `verification`: states that the gate workflow contains no deployment job
  (a manual run on any ref completes without attempting a protected
  deployment), and adds the separate Pages deployment workflow and its
  trigger/cadence.

## Impact

- **Workflows:** new `.github/workflows/pages.yml`; the `pages` job is
  removed from `.github/workflows/ci.yml`.
- **Docs:** `docs/decisions/0023-tag-triggered-ci-and-releases.md` is
  amended (Pages clause + rejected alternatives); its
  `docs/decisions/README.md` index row stays accurate and needs no change.
- **APIs / source / tests:** none — no `src/`, `tests/`, or CMake changes.
- **CI economics:** one lightweight Pages run per push to `main` (the demo
  tracks `main`); gate runs no longer include a deploy job.
