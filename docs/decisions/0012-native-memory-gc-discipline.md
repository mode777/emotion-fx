# 0012 — Native memory counts toward GC pressure; destroy-first, finalizer-backstop discipline

Status: Accepted (2026-09, change `js-api-reference`); applies from F2
(first native-backed resource class)

## Context

ADR 0011 makes dynamic-count resources GC-finalized opaque classes. That
introduces a real hazard: the quickjs collector triggers on **JS heap**
pressure and never sees GPU/RAM bytes, so a script allocating native-heavy
resources in a loop (procedural atlases, per-frame effects) can balloon
native memory long before any collection runs. Shipping the class model
without a mitigation would trade user-side slot bookkeeping for an
unbounded native-memory leak window — not acceptable for a lightweight
engine meant to run unattended.

## Decision

Four mechanisms, all required, owned by the runtime module:

1. **Native bytes count toward GC pressure.** Resources created through the
   engine route their native cost (texture bytes, buffer sizes) into the
   runtime's memory accounting via the allocator hooks (`JS_NewRuntime2`
   custom malloc functions), so collection decisions approximate the true
   footprint, not just the JS heap.
2. **Frame-end collection.** The player runs `JS_RunGC` at frame end
   (every frame, or throttled to every N frames gated on the native
   allocation delta) — cheap on this engine's heap sizes, and it bounds
   unreferenced native waste to roughly one frame of script garbage.
3. **`destroy()` is the primary path.** Every native-backed class exposes
   an idempotent `destroy()`; using a destroyed resource throws. The
   reference documents it as *the* release path — the finalizer is the
   safety net, never the plan.
4. **Finalizer backstop.** Remaining objects are finalized by GC and
   unconditionally at `JS_FreeRuntime` teardown.

Supporting rule: the display list pins recorded resources (`JS_DupValue`
at record, release after playback), and `destroy()` during a frame defers
the native release until playback completes — the list can never touch
freed memory.

## Consequences

- Unbounded native growth from script loops is structurally prevented;
  worst case is ~one frame of garbage, independent of script discipline.
- The allocator-accounting hook is a one-time runtime build (F2) that must
  be maintained as new resource kinds add native cost.
- Frame-end GC adds a small bounded per-frame cost; throttling is available
  if it ever measures.
- Scripts that care get determinism (`destroy()`); scripts that don't stay
  safe — ease of use is preserved, which was the point of ADR 0011.

## Rejected alternatives

- **Default GC only**: native memory is invisible to pressure heuristics —
  ballooning before collection; the exact hazard this ADR closes.
- **Collect on JS-heap watermarks only**: same blindness, later.
- **Expose `gc()` and document it as the user's job**: violates the
  ease-of-use goal; memory discipline is engine responsibility.
