# Spec Delta

## Purpose

Defines the project's ordered milestone ladder that decomposes `vision.md` into
stacked, independently verifiable feature milestones (F1–F8), so every future
OpenSpec change has a defined position, predecessor, and verification gate.

## ADDED Requirements

### Requirement: Fixed milestone order
The roadmap SHALL define exactly eight milestones in fixed order: F1 (player
skeleton), F2 (2D layer + display list + verification harness), F3 (3D core),
F4 (lighting + Phong, split F4a/F4b), F5 (render targets + post FX), F6
(resource packaging + asset import + REPL), F7 (skinning + animation), F8
(high-level JS layer + text + demo pack). Each milestone SHALL build only on
capabilities delivered by its predecessors, and the milestone order MUST NOT be
reordered without a change to this capability.

#### Scenario: Locating a feature in the ladder
- **WHEN** a future feature proposal is drafted
- **THEN** the roadmap assigns it exactly one milestone, and that milestone's
  predecessor list is unambiguous

#### Scenario: Out-of-order proposal
- **WHEN** a proposal implements functionality that belongs to a milestone whose
  predecessors have not passed their verification gate
- **THEN** the proposal is out of roadmap order and MUST NOT proceed to
  implementation until the predecessor's gate passes

### Requirement: Verification gate per milestone
Every milestone SHALL define a verification strategy that must pass on all four
target platforms (Windows, Linux, macOS, Emscripten) before the next milestone
starts. F1 verification SHALL be based on the build matrix plus script-mode
smoke tests with exit-code checks. From F2 onward, rendering milestones SHALL
be verified with a golden-image pixel-diff harness introduced as a first-class
F2 deliverable, complemented by unit tests for non-visual logic; F4 lighting
math and F7 skinning math SHALL additionally be verified against CPU reference
implementations.

#### Scenario: F1 gate
- **WHEN** F1 completes
- **THEN** the player binary builds on all four targets and a scripted smoke
  test that crosses the JS/C boundary passes with a zero exit code on each

#### Scenario: Rendering milestone gate
- **WHEN** a rendering milestone (F2 or later) completes
- **THEN** its golden-image comparisons pass within the defined pixel tolerance
  on all four targets, and its unit tests pass

### Requirement: Early risk retirement
The roadmap SHALL order work so that the highest-risk foundations are delivered
first: the four-platform build matrix and the JS/C runtime boundary in F1, the
display-list architecture and golden-image verification harness in F2, and the
fixed-function canned-shader strategy (single mega-shader vs build-time shader
permutations) settled no later than F4.

#### Scenario: Platform build failure halts the ladder
- **WHEN** any milestone cannot build on one of the four targets
- **THEN** subsequent milestones MUST NOT start until the gap is closed

### Requirement: Roadmap documented in AGENTS.md
The roadmap (milestone order, scope, and verification gates) SHALL be documented
in the repository's `AGENTS.md` so any session can determine where a feature
belongs without reading this change.

#### Scenario: New session needs placement
- **WHEN** an agent reads `AGENTS.md` only
- **THEN** it can determine the milestone order, each milestone's scope summary,
  and which milestones are still open

### Requirement: Proposals declare roadmap position
Every future feature proposal SHALL state which roadmap milestone it implements
and SHALL be rejected or deferred if that milestone's predecessor gate has not
passed.

#### Scenario: Proposal with missing position
- **WHEN** a feature proposal does not name its roadmap milestone
- **THEN** the proposal is incomplete and cannot be approved for apply
