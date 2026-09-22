# Proposal

**Roadmap position:** Cross-cutting CI infrastructure — no feature
milestone. It revises the trigger policy of the F1-era gate runner
(ADR 0009) and applies to every gate from F2 onward; it does not start or
reopen a ladder milestone.

## Why

`.github/workflows/ci.yml` currently runs the full four-target matrix on
every push to `main` and on every pull request. The project is
single-branch and agent-driven, so those runs re-verify already-known
state and burn runner minutes without producing a new signal. At the same
time, a green run produces no downloadable player: the built binaries
exist only inside the ephemeral runner. The gate should run when a
version is cut (a tag) or when explicitly asked for, and every run should
leave behind retrievable binaries.

## What Changes

- **Trigger policy (BREAKING for anyone relying on per-push checks).**
  The workflow runs on pushes of version tags (`v*`) and on
  `workflow_dispatch` only. The `push.branches: [main]` and
  `pull_request` triggers are removed, so pushes and PRs no longer start
  the matrix automatically.
- **Manual runs via the GitHub CLI.** `workflow_dispatch` (already
  declared) becomes a first-class entry point: `gh workflow run ci.yml`
  starts the full gate on the selected ref, including the native matrix,
  the Emscripten build, and the web golden jobs.
- **Downloadable binaries on every run.** Each target's build is packaged
  into a named archive and attached to the run:
  - Linux, Windows, macOS native `efx` player executables;
  - the Emscripten web bundle (`player_web.html` + `.js` + `.wasm` +
    `.data`).
  Every run uploads these as workflow artifacts. On tag runs, the same
  archives are attached to the GitHub Release for that tag, so a release
  page carries all four downloadable targets.
- **Re-gate the auxiliary jobs.** `generate-goldens` stays manual-only.
  The `pages` job currently deploys only on `refs/heads/main`; with the
  push trigger gone it is re-gated to tag and manual runs so Pages
  deployment is not silently stranded.
- **Docs and ADR.** A new decision record
  `docs/decisions/0023-tag-triggered-ci-and-releases.md` captures the
  trigger policy and artifact/release rules and marks ADR 0009's trigger
  paragraph superseded; the index and the CI notes in `AGENTS.md`/`README`
  are updated. **New ADR required — docs/decisions/0023.**

**Assumption recorded:** "a new tag" is taken to mean a version tag
matching `v*`. Tags outside that pattern do not start the gate. If no
versioning convention exists yet, this is the least surprising default
and is easy to widen later.

**Non-goals:**

- No source, header, or CMake changes; the build itself is unchanged.
- No code signing, notarization, or installer generation.
- No package-manager or artifact-registry distribution.
- No automated changelog or version bumping.
- No change to golden-image tolerance, golden jobs, or the four-target
  gate's meaning — only to when it runs and what it leaves behind.
- Restoring PR validation for a future contributor workflow is out of
  scope; it can be a later change if needed.

## Capabilities

### New Capabilities

None.

### Modified Capabilities

- `verification`: adds an explicit CI trigger policy (version tags plus
  manual dispatch; never per-push/PR) and a downloadable-artifact
  requirement (per-target archives on every run; attached to the tag's
  release on tag runs) to the existing CI gate behavior.

## Impact

- **Workflow:** `.github/workflows/ci.yml` — triggers, per-job gating,
  packaging and upload steps, and a release-attachment step.
- **Docs:** new `docs/decisions/0023-tag-triggered-ci-and-releases.md`,
  its row in `docs/decisions/README.md`, and a status note in
  `docs/decisions/0009-github-actions-gate-runner.md`; CI wording in
  `AGENTS.md` and `README.md`.
- **APIs / source:** none.
- **CI economics:** runner usage drops to tag/manual cadence, and each
  run produces retrievable artifacts.
