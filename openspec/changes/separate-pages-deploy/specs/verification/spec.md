# Spec Delta: verification

## ADDED Requirements

### Requirement: Separate Pages deployment workflow
The public web player's GitHub Pages build and deployment SHALL be a
separate workflow (`.github/workflows/pages.yml`), not a job inside the gate
workflow. That workflow SHALL run on pushes to the default branch and on
`workflow_dispatch`, and its deployment job SHALL be gated to the default
branch so a manual dispatch from another ref is a no-op rather than a
protected-environment failure. It SHALL build the same Emscripten web-player
target the gate builds and deploy it with the GitHub Pages actions, and its
Emscripten toolchain SHALL be pinned consistently with the rest of CI
(ADR 0020).

#### Scenario: Default-branch push deploys the web player
- **WHEN** a commit is pushed to the default branch
- **THEN** the Pages workflow builds the web player and deploys it to
  GitHub Pages

#### Scenario: Manual dispatch from the default branch deploys
- **WHEN** a user manually dispatches the Pages workflow from the default
  branch
- **THEN** the web player is built and deployed to GitHub Pages

#### Scenario: Manual dispatch from another ref is a no-op
- **WHEN** the Pages workflow is manually dispatched from a non-default ref
- **THEN** the deployment job is skipped (no protected-environment failure)
  and the workflow does not deploy

#### Scenario: Pages deployment is not part of the gate
- **WHEN** a gate run completes, whether triggered by a tag or manually
- **THEN** the gate workflow contains no Pages deployment job, so the run's
  status reflects only the verification (and tag-release) jobs

## MODIFIED Requirements

### Requirement: Version-tag and manual CI triggers
The continuous-integration gate workflow SHALL run when a version tag
matching `v*` is pushed, and when a run is requested manually through the
`workflow_dispatch` event (e.g. `gh workflow run ci.yml`). It MUST NOT
run automatically on ordinary branch pushes or on pull requests. A manual
run SHALL execute the same full gate — native build matrix, Emscripten
build, and golden-image jobs — as a tag run, against the selected ref. The
gate workflow SHALL NOT contain the GitHub Pages deployment job, so a gate
run completes on the selected ref without attempting a protected
deployment; Pages deployment is owned by a separate workflow (see the
Separate Pages deployment workflow requirement).

#### Scenario: Version tag starts the gate
- **WHEN** a tag matching `v*` is pushed
- **THEN** the full gate workflow starts for that tag

#### Scenario: Manual run via the GitHub CLI
- **WHEN** a user invokes `gh workflow run ci.yml` against a ref
- **THEN** the full gate workflow starts for that ref and runs every job
  the tag run would run

#### Scenario: Branch push does not start the gate
- **WHEN** a commit is pushed to a normal branch or a pull request is
  opened
- **THEN** the gate workflow does not start automatically

#### Scenario: Manual gate run does not attempt deployment
- **WHEN** a user manually dispatches the gate workflow against a
  non-default ref
- **THEN** the run contains no Pages deployment job and completes without a
  protected-environment failure
