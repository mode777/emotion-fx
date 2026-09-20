# 0011 — Dynamic-count resources are GC-finalized opaque classes; slots only for fixed banks

Status: Accepted (2026-09, change `js-api-reference`)

## Context

The initial `js-api` sketch used pre-allocated slot banks for all native
storage (`setMesh(0, data); useMesh(0)` — vision.md's leak-avoidance rule
read as "slots by default"). Review pushed back: slot discipline turns user
code into a mini-allocator (which slot is free? who owns index 3?), fixed
banks cannot scale to F6 asset packs with arbitrary content, and
index-collision errors are exactly the class of bug a GC'd scripting layer
should not export. quickjs (ADR 0002) fully supports the alternative:
`JS_NewClass` + `JSClassDef.finalizer` wraps a native handle in an opaque JS
object with real methods, the finalizer runs when the GC collects the
object, and `JS_FreeRuntime` finalizes everything at shutdown — the vendored
`vendor/quickjs-ng/examples/point.c` demonstrates the pattern.

## Decision

Resources are classified by count, and the JS shape follows the count:

- **Dynamic-count resources are opaque GC-finalized JS class instances**
  wrapping the native handle: type-checked arguments via `JS_GetOpaque2`
  and an explicit `destroy()` as the deterministic release path.
  `create*`/`load*` calls return them. (The definitive type list and the
  fully-opaque starting point — `destroy()` only at first — live in
  ADR 0013.)
- **The finalizer is a backstop, not the primary path**: `destroy()` is
  idempotent; using a destroyed resource throws; anything still alive at
  runtime teardown is finalized — a script cannot leak past process exit.
- **Slot banks remain only where the count is fixed by design**: the light
  bank (4 point + 1 directional). The camera is state, not a resource.
- **Materials stay plain JS objects** — pure data, nothing native until a
  draw call reads them.
- The display list pins recorded resource objects (`JS_DupValue` at record
  time, release after playback); native release from `destroy()` is
  deferred to frame end so playback never touches freed memory. The
  non-determinism that remains is bounded by the discipline in ADR 0012.

## Consequences

- User code drops slot bookkeeping entirely; resource count scales to asset
  packs, bounded by memory rather than by a compile-time constant.
- Display-list records hold traced `JSValue`s instead of `int` indices and
  state grouping compares opaque pointers — slightly heavier, negligible at
  PS2-era scene sizes.
- F2 (first native resource class) must build the class machinery once:
  class-ID registration, finalizers, argument validation — then each new
  class is cheap.
- Future `js-api` deltas must model new resource types as classes (or
  justify a genuinely fixed bank) per the `js-api` classification rule.

## Rejected alternatives

- **Slots for everything** (the original sketch): lost — user-side
  allocator burden, fixed banks cannot hold arbitrary asset packs, and
  index-collision bugs leak into scripts. Retained only for the light bank.
- **Plain integer handles + `destroy*` functions, no finalizer**: lost —
  forgetful scripts balloon native memory for the whole session; the
  finalizer backstop is strictly safer at negligible cost.
- **C-side refcounting shared with JS**: lost — two ownership systems for
  wrappers that are 1:1; refcounting only pays when handles are shared,
  which these resources are not.
