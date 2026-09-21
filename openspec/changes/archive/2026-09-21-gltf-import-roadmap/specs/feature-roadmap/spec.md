# Spec Delta: feature-roadmap

## MODIFIED Requirements

### Requirement: Fixed milestone order
The roadmap SHALL define exactly eight milestones in fixed order: F1 (player
skeleton), F2 (2D layer + display list + verification harness), F3 (3D core),
F4 (lighting + Phong, split F4a/F4b), F5 (render targets + post FX), F6
(resource packaging + glTF 2.0 asset import + REPL), F7 (skinning +
animation), F8 (high-level JS layer + text + demo pack). F6 asset import
SHALL cover glTF 2.0 payloads: meshes, images (textures), skins, and
animation clips. Each milestone SHALL build only on capabilities delivered by
its predecessors, and the milestone order MUST NOT be reordered without a
change to this capability.

#### Scenario: Locating a feature in the ladder
- **WHEN** a future feature proposal is drafted
- **THEN** the roadmap assigns it exactly one milestone, and that milestone's
  predecessor list is unambiguous

#### Scenario: Out-of-order proposal
- **WHEN** a proposal implements functionality that belongs to a milestone whose
  predecessors have not passed their verification gate
- **THEN** the proposal is out of roadmap order and MUST NOT proceed to
  implementation until the predecessor's gate passes

### Requirement: Early risk retirement
The roadmap SHALL order work so that the highest-risk foundations are
delivered first: the four-platform build matrix and the JS/C runtime boundary
in F1, the display-list architecture and golden-image verification harness in
F2, and the fixed-function canned-shader strategy (single mega-shader vs
build-time shader permutations) settled no later than F4. The glTF 2.0 import
format SHALL be pinned by this roadmap (F6 scope), with only the glTF profile
— container (.glb vs .gltf), allowed extensions, and image embedding —
deferred to the F6 change.

#### Scenario: Platform build failure halts the ladder
- **WHEN** any milestone cannot build on one of the four targets
- **THEN** subsequent milestones MUST NOT start until the gap is closed

## ADDED Requirements

### Requirement: Third-party dependency evaluation at proposal time
Any proposal that introduces a new third-party dependency (library, codec, or
data-format implementation) SHALL name the candidate libraries and settle the
selection in the proposal, before implementation starts. The evaluation SHALL
record, for the chosen library at minimum: its license, its fit with the
vendoring policy (pinned source snapshots), its coverage of all four targets
including Emscripten, and its compatibility with the C11 core. A third-party
dependency MUST NOT be vendored or linked before its evaluation is recorded
in an approved proposal (or an approved update to one).

#### Scenario: Proposal introduces a dependency
- **WHEN** a feature proposal names a new third-party dependency
- **THEN** the proposal lists the candidate libraries and the selection
  rationale (license, vendoring fit, four-target coverage, C11 fit), and
  implementation tasks for that dependency start only after approval

#### Scenario: Dependency need discovered during implementation
- **WHEN** implementation work uncovers the need for a dependency that the
  proposal did not evaluate
- **THEN** the dependency is not vendored or linked until the proposal (or
  design doc of the change) is updated with the evaluation and re-approved

#### Scenario: First evaluation customers
- **WHEN** the F2 verification-harness change (image read/write for golden
  images) and the F6 import change (glTF loader, image decoder, zip reader)
  are drafted
- **THEN** each proposal contains the required dependency evaluation for the
  libraries it introduces
