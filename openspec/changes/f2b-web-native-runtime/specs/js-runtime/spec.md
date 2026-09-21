# Spec Delta

## Purpose

Defines the JavaScript runtime contract per platform after the web split:
the bundled quickjs interpreter is the desktop runtime; on Emscripten the
browser's own JS engine drives the compiled core directly through the
native bridge, with the same `efx` namespace contract and error/exit-code
semantics.

## MODIFIED Requirements

### Requirement: Bundled ES6 execution
The player MUST execute scripts with its own bundled ES6 interpreter
(quickjs-ng) on desktop platforms. On Emscripten the browser's native JS
engine MUST drive the compiled core directly through the native bridge —
no quickjs, no embedded interpreter, and no embedded-GC machinery ships in
the wasm. Game scripts MUST NOT depend on browser or Node.js APIs, neither
directly nor transitively, so the same script text runs on both runtimes.

#### Scenario: Portable script runs identically everywhere
- **WHEN** a smoke script that uses only standard ES6 features and
  engine-exposed functions is run on all four targets
- **THEN** it produces the same observable output and exit code on each

#### Scenario: Engine-provided global namespace
- **WHEN** any script starts (desktop interpreter or browser engine)
- **THEN** the engine functions are available in a single well-known global
  namespace without any import or setup by the script

#### Scenario: No interpreter in the wasm
- **WHEN** the Emscripten player is built
- **THEN** quickjs object code is not linked and the runtime memory
  footprint contains no embedded interpreter

## ADDED Requirements

### Requirement: Native bridge binding contract
On Emscripten the player SHALL expose every C-implemented engine function
through the native bridge with the same name, signature, semantics and
error behavior as the desktop binding, so that browser code and game
scripts use one API. Resource objects (`createImageData`, `createTexture`,
`whiteTexture`) SHALL wrap native handles as JS objects with the same
`destroy()` semantics; the frame-end native release sweep SHALL continue
to run in C. The bridge SHALL NOT require the embedded interpreter, and
its presence MUST NOT reintroduce embedded-GC machinery into the web
build.

#### Scenario: Same call from browser code
- **WHEN** browser script code calls the bridge's `setClearColor` with a
  4-element color array and then `drawQuad` with a created texture
- **THEN** the core renders identically to the same sequence on desktop

#### Scenario: Resource lifecycle on web
- **WHEN** browser code creates a texture and calls `destroy()` on it
- **THEN** the native release follows the same deterministic/deferred
  contract as on desktop, with the browser GC managing only the JS side

#### Scenario: No embedded GC on web
- **WHEN** the web player runs its frame loop
- **THEN** no embedded-interpreter garbage collection executes (the
  segfault class is structurally absent)
