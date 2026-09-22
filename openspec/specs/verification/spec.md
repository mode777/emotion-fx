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
