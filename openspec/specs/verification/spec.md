# verification

## Purpose

Defines the verification harness for milestone gates: scripted smoke tests
that cross the JS/C boundary and assert exit codes, which must pass on all four
targets. This is the F1 gate and the foundation that the F2 golden-image
harness will extend.

## Requirements

### Requirement: Smoke-test suite
The repository SHALL contain a smoke-test suite consisting of scripted tests
run via the player's `--script` mode. Each test SHALL assert its outcome via
exit code and SHALL exercise the JS/C boundary (calling engine-exposed
functions from JS).

#### Scenario: Suite run on a built target
- **WHEN** the smoke-test suite is run against a player build on any supported
  target
- **THEN** every test completes with exit code 0

### Requirement: Four-target gate
The full smoke-test suite MUST pass on Windows, Linux, macOS, and Emscripten
before the next roadmap milestone starts.

#### Scenario: Gate evaluation for F1
- **WHEN** the F1 gate is evaluated
- **THEN** the build matrix succeeds on all four targets and the smoke suite
  passes on all four targets with zero failures

### Requirement: Test failure reporting
A failing smoke test SHALL produce a non-zero exit status and a diagnostic that
identifies the failing test by name.

#### Scenario: Identifying a failure
- **WHEN** a smoke test fails on any target
- **THEN** the suite reports the failing test's name and exits non-zero

### Requirement: Golden-image harness
The repository SHALL contain a golden-image harness as a first-class
deliverable alongside the smoke suite. The harness SHALL render scripted
frames at a fixed, documented virtual frame size, capture the rendered
frame into an image file, and compare it against a committed golden image
under a settled tolerance policy. The player SHALL provide a
non-interactive capture run mode that renders a resource root's `main.js`
for a given number of frames, writes the final rendered frame to a PNG, and
exits with status 0 — no user interaction, no window shown when the platform
allows. Tolerance policy (settled per the roadmap's F2 assignment): a pixel
passes when every channel differs from the golden by at most 2 of 255; a
frame passes when at least 99.5% of its pixels pass. A failing comparison
SHALL exit non-zero and produce a diagnostic naming the test plus a diff
artifact (the captured frame and a visual diff image) for triage.

#### Scenario: Golden test passes within tolerance
- **WHEN** a golden-image test runs against a build on any supported target
- **THEN** the captured frame matches its committed golden under the
  tolerance policy and the test exits 0

#### Scenario: Mismatch beyond tolerance fails with artifacts
- **WHEN** a rendering change alters the output so that more than the
  allowed fraction of pixels exceeds the channel tolerance
- **THEN** the comparison exits non-zero, names the failing test, and writes
  the captured and diff images

#### Scenario: Capture run mode is deterministic
- **WHEN** the same resource root is rendered twice through the capture run
  mode on the same target
- **THEN** both captured frames are identical

### Requirement: CI rendering determinism
Golden-image comparisons MUST run in continuous integration on all four
targets under pinned rendering conditions: software rasterization for
desktop targets, an exact pinned Emscripten version, and a pinned browser
for the Emscripten job. Runner or toolchain upgrades MUST NOT require
regenerating committed goldens; goldens change only when intended output
changes, via an explicit regeneration step.

#### Scenario: Runner upgrade does not churn goldens
- **WHEN** CI runners or pinned toolchains are updated to newer versions
- **THEN** the golden-image suite still passes without regenerating any
  golden image

### Requirement: Display-list unit tests
The display list's record and playback behavior SHALL be unit-testable
without a window or GPU — a headless test SHALL be able to record a frame's
draw commands and assert on the recorded set (order, keys, values) per the
frame-transient record semantics (ADR 0019). These tests SHALL run on all
four targets as part of the standard test suite.

#### Scenario: Headless record-and-assert
- **WHEN** a unit test records a known sequence of 2D draw commands and
  asserts on the recorded commands without initializing any graphics device
- **THEN** the test passes with exit code 0 on all four targets

### Requirement: Rendering milestone gate
From F2 onward, a rendering milestone's verification gate SHALL be: every
golden-image comparison for that milestone within tolerance AND every unit
test (including display-list tests) passing — on Windows, Linux, macOS, and
Emscripten — before the next milestone starts, complementing the existing
smoke-suite gate.

#### Scenario: F2 gate evaluation
- **WHEN** the F2 gate is evaluated
- **THEN** the 2D golden images match on all four targets, the display-list
  and unit tests pass on all four targets, and the F1 smoke suite still
  passes

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
