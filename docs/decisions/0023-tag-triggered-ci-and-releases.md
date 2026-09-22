# 0023 — CI runs on version tags and manual dispatch, and every run publishes binaries

Status: Accepted (cross-cutting; change `ci-tag-releases`; amended 2026-09,
change `separate-pages-deploy`: Pages deployment moved to its own workflow)

## Context

ADR 0009 established GitHub Actions as the four-target gate runner and
had `.github/workflows/ci.yml` run on every push to `main` and on every
pull request. The repository is single-branch and agent-driven, so those
runs re-verified already-known state and spent runner minutes without a
new signal. A green run also produced nothing retrievable: the built
player existed only inside the ephemeral runner. Milestone gates are
evaluated on demand following the Linux → Windows → macOS iteration
order, which needs a manual entry point anyway. Full process record:
`openspec/changes/ci-tag-releases`.

## Decision

`.github/workflows/ci.yml` SHALL trigger only on `push.tags: ['v*']` and
`workflow_dispatch` (`gh workflow run ci.yml`). It MUST NOT run
automatically on ordinary branch pushes or pull requests.

Every run packages each supported target into a named, downloadable
archive and attaches it to the run with `actions/upload-artifact`:

- a native `efx` player archive for Linux, Windows, and macOS;
- the Emscripten web-player bundle (HTML loader, JS glue, wasm, data).

The version token is the tag name on tag runs and `dev-<sha>` on manual
runs. On tag runs a single `release` job (`needs: [native, emscripten]`,
`permissions: contents: write`) downloads every archive and attaches them
as assets of the GitHub Release for that tag using the preinstalled `gh`
CLI.

`generate-goldens` remains manual-only. Pages deployment lives in a
separate workflow (`.github/workflows/pages.yml`) that runs on pushes to
the default branch and on manual dispatch; the gate workflow contains no
deployment job, so a manual gate run on any ref reflects verification
only. The golden jobs and their pinned conditions (ADR 0020) are
unchanged — only when the gate runs and what it leaves behind changed.

## Consequences

- Runner use drops to release/on-demand cadence. A broken `main` is no
  longer caught automatically; run `gh workflow run ci.yml` before cutting
  a release, or restore a PR/filtered trigger in a later change.
- A release is the distribution surface: four downloadable target
  archives per release, built by the same gate that verifies them.
- `workflow_dispatch` requires the workflow to exist on the default
  branch; it does, so manual runs against any ref that contains the
  workflow are possible. `gh` needs a token with `actions: write` to
  start a run; release publishing needs `contents: write`, scoped to the
  `release` job only.
- Pages deploys from the default branch, which the `github-pages`
  environment permits by default, so no tag policy is required. A manual
  dispatch of the Pages workflow from any other ref skips its deploy job
  (the protected environment would otherwise fail it). The `v*` tag
  policy added for the earlier tag-triggered deploy is now unused and can
  be left in place. This reverses the original choice to keep `pages`
  inside the gate: `workflow_dispatch` is the agent verification entry
  point, and a deploy job that cannot succeed on a feature branch made
  successful verification runs report as failures.

## Rejected alternatives

- **Any-tag triggering.** Arbitrary or moving tags would cut releases and
  spend runners on non-release refs; `v*` is the minimal version
  convention.
- **Keeping `pull_request` (or a paths-filtered push).** No PR workflow
  exists, and a filtered push still fires on the one branch the team
  works on.
- **A third-party release action.** `gh` is preinstalled on runners and
  already used by the project's tooling; no new dependency.
- **Each matrix job uploading to the release directly.** Concurrent
  release creation races, and it duplicates the write token across jobs.
- **Keeping Pages inside the gate with a default-branch-only guard.**
  Considered when separating it (change `separate-pages-deploy`); it
  stops the false failure but leaves a deploy job in the gate, so a Pages
  outage or policy change would still redden a default-branch/tag gate
  run. A separate workflow keeps the gate's status unambiguous.
