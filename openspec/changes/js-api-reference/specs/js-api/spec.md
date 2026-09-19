# Spec Delta

## Purpose

Defines the contract for the engine's script-facing JavaScript API: one global
namespace, the two-layer (C-implemented / pure-JS) structure, the resource
memory model with fixed limits, and the normative developer-facing reference
document that catalogs every public API function by delivery milestone.

## ADDED Requirements

### Requirement: Single global API namespace
All engine-provided script functions SHALL be exposed as members of one
well-known global namespace object (the `efx` object established by F1),
available to every script without imports or setup. Scripts SHALL access
engine functionality only through this namespace and standard ES6 built-ins;
the reference document SHALL state this rule. This covers both C-implemented
functions and engine-provided high-level JS functions.

#### Scenario: Namespace available without setup
- **WHEN** a script calls `efx.log` without any import or setup code
- **THEN** the call succeeds on every target platform

#### Scenario: No scattered engine globals
- **WHEN** a reviewer checks how a script reaches an engine function
- **THEN** every engine-provided function is reachable as a member of the
  single namespace, not as an additional free global

### Requirement: Two-layer API with strict layering
The script API SHALL consist of exactly two layers: low/mid-level functions
implemented in C/C++ and registered through the engine binding, and
high-level convenience functions implemented in pure ES6. High-level
functions MUST be implemented using only the public low/mid-level API and
standard ES6 built-ins — they MUST NOT use private bindings or host
facilities that are not part of the public API. Every API function in the
reference SHALL be tagged with its layer.

#### Scenario: High-level function built on public API
- **WHEN** a high-level convenience function (e.g. a model or text drawer) is
  implemented
- **THEN** it calls only documented public API functions and standard ES6

#### Scenario: Layer tag present
- **WHEN** a function entry is read in the API reference
- **THEN** its entry marks it as either C-implemented or pure-JS

### Requirement: Resource classification and fixed limits
Every engine resource type that scripts can create or reference SHALL be
classified in the reference as exactly one of: JS-managed (plain script
objects, garbage collected), slot-based (a fixed pre-allocated set of indexed
resources, e.g. `setMesh(slot, data)`), or handle-based (explicit creation
and destruction). Every resource requiring native storage MUST be either
slot-based or handle-based with a documented release path. The reference
SHALL document the engine's fixed limits, including 4 point lights, 1
directional light, and 1 camera, and SHALL document slot counts as explicit
constants.

#### Scenario: Fixed limits stated
- **WHEN** the reference document's limits section is read
- **THEN** it states 4 point lights, 1 directional light, and 1 camera,
  matching vision.md

#### Scenario: Resource without a classification
- **WHEN** a change proposes exposing a new resource type to scripts without
  classifying it as JS-managed, slot-based, or handle-based
- **THEN** the change is incomplete and MUST NOT update the API reference

### Requirement: Normative API reference document
The project SHALL maintain `docs/js-api.md` as the normative, developer-facing
reference of the entire script API. It SHALL contain an entry for every public
API function with a signature sketch, a description, its layer tag, and the
roadmap milestone (F1–F8) that delivers it. Entries for functions whose
milestone has not passed its verification gate SHALL be explicitly marked
provisional. The document SHALL also document the lifecycle hooks the entry
script defines (`update`, `render`) and how API errors surface (exceptions,
exit codes). Any change that adds, modifies, or removes a public API function
MUST update the document in the same change.

#### Scenario: Callable-today vs planned is distinguishable
- **WHEN** a reader opens the reference
- **THEN** the F1 functions (`efx.log`, `efx.quit`, `efx.args`) are presented
  as current behavior, and later-milestone entries are marked provisional

#### Scenario: Milestone change updates the reference
- **WHEN** a feature change adds or changes an API function
- **THEN** the same change contains the matching `docs/js-api.md` update with
  the function's signature, layer, and milestone tags

#### Scenario: Catalog derived from vision
- **WHEN** the document's function catalog is checked against vision.md
- **THEN** every capability vision.md names for the consumer API (2D quads,
  meshes, vertex colors, cameras, lights, Phong materials with maps, alpha
  masks, blending modes, render targets, post FX, resource loading,
  skinning/animation, high-level model and text drawing) has a corresponding
  catalog entry or an explicitly noted open question
