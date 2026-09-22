# js-runtime

## Purpose

Defines the embedded ES6 script runtime and the JavaScript/C boundary: script
execution with a bundled interpreter, the mechanism by which engine functions
implemented in C are callable from JS, the dependency restrictions on scripts,
and error propagation.

## Requirements

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

### Requirement: JS-to-C function calls
The engine SHALL expose its C-implemented functions to scripts through the
global namespace, such that a script call crosses into C, executes, and returns
a result or effect to the script. F1 SHALL expose at least: a log/print
function, a script-requested quit function taking an exit code, and a means to
read script arguments provided by the host.

#### Scenario: Log crossing
- **WHEN** a script calls the engine log function with a string
- **THEN** the string appears on the player's stdout

#### Scenario: Quit crossing
- **WHEN** a script calls the engine quit function with code 3
- **THEN** the C core terminates the run with exit code 3

#### Scenario: Host arguments readable
- **WHEN** the host launches a `--script` run with extra arguments
- **THEN** the script can read those arguments through the engine API

### Requirement: Error propagation to exit codes
An uncaught script exception MUST surface as a non-zero player exit code with
the error message on stderr, in both `--script` mode and the frame-loop hooks.
A stack or error trace SHALL be included in the diagnostic output.

#### Scenario: Exception in script mode
- **WHEN** a `--script` run throws an uncaught exception
- **THEN** the player exits non-zero and stderr contains the error message

#### Scenario: Exception in a frame hook
- **WHEN** `update` or `render` throws an uncaught exception during the frame
  loop
- **THEN** the player stops the loop, prints the error to stderr, and exits
  non-zero

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
