# Spec Delta

## ADDED Requirements

### Requirement: Version-tag and manual CI triggers
The continuous-integration gate workflow SHALL run when a version tag
matching `v*` is pushed, and when a run is requested manually through the
`workflow_dispatch` event (e.g. `gh workflow run ci.yml`). It MUST NOT
run automatically on ordinary branch pushes or on pull requests. A manual
run SHALL execute the same full gate — native build matrix, Emscripten
build, and golden-image jobs — as a tag run, against the selected ref.

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

### Requirement: Downloadable per-target build artifacts
Every gate run SHALL package the built player for each supported target
into a named archive and publish it as a downloadable workflow artifact:
a native executable archive for Windows, Linux, and macOS, and a bundle
archive containing the Emscripten web player (HTML loader, JS glue, wasm,
and data) for Emscripten. On a tag run, the same archives SHALL
additionally be attached as downloadable assets of the GitHub Release for
that tag.

#### Scenario: Every run publishes downloadable archives
- **WHEN** a gate run completes, whether triggered by a tag or manually
- **THEN** a downloadable workflow artifact exists for each of the four
  targets

#### Scenario: Tag run attaches archives to the release
- **WHEN** the gate runs for a version tag
- **THEN** the four target archives are attached as assets of the GitHub
  Release for that tag
