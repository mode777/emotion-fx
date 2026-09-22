# Design

## Context

CI is a single workflow, `.github/workflows/ci.yml`, with five jobs:
`native` (Windows/Linux/macOS matrix), `emscripten`, `golden-web`,
`generate-goldens` (manual bootstrap), and `pages`. The workflow is the
four-target gate runner (ADR 0009) and also owns the pinned golden-image
conditions (ADR 0020). Today it triggers on `push.branches: [main]`,
`pull_request`, and `workflow_dispatch`. The repo is single-branch and
agent-driven; per-push runs re-verify known state. See `proposal.md` —
Why for motivation; the `verification` delta is the behavior contract.

One existing invariant constrains the approach: ADR 0020 requires golden
comparisons to run under pinned conditions, and the golden jobs must
still run whenever the gate runs. The `.wasm`, `.js`, and `.data` output
names come from the Emscripten CMake target and must be read from the
build at apply time rather than assumed here.

## Goals / Non-Goals

**Goals:**

- Exactly two ways to start the full gate: a `v*` tag push, or a manual
  `workflow_dispatch` (`gh workflow run ci.yml`).
- Every run leaves four downloadable per-target archives; tag runs also
  attach them to the tag's GitHub Release.
- Keep the golden jobs, the pinned toolchains, and the four-target gate
  semantics untouched.
- Capture the policy durably (new ADR; ADR 0009's trigger paragraph
  superseded).

**Non-Goals:**

- Any change to sources, CMake, or the built player's contents.
- Signing, notarization, installers, or package registries.
- Changelog/version automation.
- Re-introducing PR validation (a later change if the workflow changes).

## Decisions

### D1 — Triggers are `v*` tags plus `workflow_dispatch`

The `on:` block becomes:

```yaml
on:
  push:
    tags: ['v*']
  workflow_dispatch:
```

`push.branches` and `pull_request` are removed. `workflow_dispatch` was
already present, so no new event is introduced; it just becomes one of
two ways in. Manual is the escape hatch for "prove the gate now" and for
non-release refs — essential because milestone verification (AGENTS.md's
Linux → Windows → macOS order) is performed on demand, not on pushes.

*Rejected:* any-tag triggering. Arbitrary/moving tags would cut releases
and spend runners on non-release refs; `v*` is the minimal version
convention. *Rejected:* keeping `pull_request` (or `paths`-filtered
push) as a safety net — no PR workflow exists, and a filtered push still
fires on the branch the team works on.

### D2 — Package in the build jobs, create the release in one place

Each producing job packages its own output and uploads it with
`actions/upload-artifact`, so every run — tag or manual — yields
downloads:

- `native` matrix: tar/zip the built `efx` player per OS.
- `emscripten`: zip the web player bundle produced by the Emscripten
  build (HTML loader, JS glue, wasm, data).

Tag runs additionally attach the same files to a release. Release
creation lives in a single `release` job (`needs: [native, emscripten]`,
`if: startsWith(github.ref, 'refs/tags/v')`) that `download-artifact`s
everything and then runs `gh release create "$GITHUB_REF_NAME"
--generate-notes` followed by uploads, behind
`permissions: contents: write`. One job avoids the matrix jobs racing to
create the same release and keeps the write token off the build jobs.

Asset naming uses a version token: the tag name on tag runs, and a
SHA-derived development token on manual runs, e.g.
`emotion-fx-<version>-<os>-<arch>.tar.gz|.zip` and
`emotion-fx-<version>-web.zip`. Exact names are finalized during apply.

*Rejected:* a third-party release action — `gh` is preinstalled on
runners and already used in this repo's tooling, so no new dependency.
*Rejected:* letting each matrix job upload to the release directly —
concurrent `gh release` creation races and needs duplicated tokens.

### D3 — Auxiliary jobs re-gated, not removed

`generate-goldens` keeps `if: github.event_name == 'workflow_dispatch'`.
`pages` changes from `if: github.ref == 'refs/heads/main'` to run on tag
or manual dispatch, otherwise removing the push trigger would silently
stop Pages deployment. `golden-web` and `emscripten` are unchanged and
run on both entry paths.

*Rejected:* moving Pages to its own workflow — more surface for no
behavioral gain. *Rejected:* dropping Pages auto-deploy — it is the
public web player and should follow releases.

### D4 — Documentation: new ADR 0023, mark 0009's trigger superseded

A new record, `docs/decisions/0023-tag-triggered-ci-and-releases.md`,
states the durable policy: the gate runs on `v*` tags and manual
dispatch, and every run publishes per-target archives with tag runs
attaching to the release. ADR 0009's status gains "Superseded by 0023"
with a pointer, since its "every push and every PR" trigger paragraph is
now wrong; the rest of 0009 (Actions is the gate runner) still holds.
The README index row and the CI notes in `AGENTS.md`/`README.md` are
updated in the same change.

*Rejected:* editing ADR 0009 in place — the directory convention is to
supersede rather than rewrite accepted records.

### D5 — No unmanaged-resource implications

This change adds no script-facing resources and no native handles, so
the ADR 0011/0012 exposure rule (GC-finalized opaque classes vs fixed
banks) does not apply.

## Risks / Trade-offs

- **No automatic checks on the working branch** → a broken `main` is
  noticed only when a tag/manual gate runs. Mitigation: run
  `gh workflow run ci.yml` before cutting a release; restore a PR or
  filtered trigger in a later change if the workflow changes.
- **`gh workflow run` needs the workflow on the default branch and a
  token with `actions: write`** → document in the ADR and `README`; the
  existing `workflow_dispatch` declaration already satisfies the
  default-branch requirement.
- **Release job needs `contents: write`** → scoped to the `release` job
  only; build jobs stay read-only. A tag pushed by a token that cannot
  create releases fails loudly at that job, after tests.
- **Emscripten/web bundle filenames may drift** → the packaging task
  reads the actual CMake output names during apply and fails if a
  required file is missing, rather than uploading a partial bundle.
- **Windows vs Unix archiving differs** → use per-OS shell steps
  (`Compress-Archive`/`tar`), keeping the job's existing per-OS
  conditionals.
- **Tag refs change `github.ref`** → the `pages` and `release` gating
  uses explicit `startsWith(github.ref, 'refs/tags/v')` rather than
  assuming a branch ref.

## Apply addendum

Two findings surfaced while proving the tag path against the real
repository (run `35720979004`):

- **Pages needs a tag policy on the `github-pages` environment.** The
  environment's deployment branch policy allowed only `main`, so the tag
  run's `pages` job failed immediately (no steps ran). A `v*` tag policy
  was added to the environment (a repository setting, not
  version-controlled) so D3's tag-or-manual Pages behavior works. A repo
  that skips this setting will see `pages` fail on tag runs.
- **The release job needs a checkout.** `gh release create
  --generate-notes` reads git history, which is absent in a job that only
  downloads artifacts; the job now begins with `actions/checkout@v4` and
  `fetch-depth: 0`.
- **The release downloads must be filtered.** `download-artifact` with no
  filter pulled the auxiliary `github-pages` artifact (`artifact.tar`)
  into the release upload; the step now uses `pattern: player-*` so only
  the four target archives are attached.
