# ADR 0007: External control — ControlBus params, OSC over plain sockets

## Status

Proposed

## Context

Phase 4 makes the running sound controllable from outside the patch: OSC
from controllers on the network, the mouse, and any future live source.
ADR 0003 fixed the shape — a `ControlBus` of atomic floats written by
control-side threads and read by the audio thread, never messages — but
left open where the bus lives, how a patch names a slot, how values are
smoothed, and which transport carries OSC (the one dependency decision the
architecture deferred to this phase: SDL3_net vs. plain sockets).

Prior art surveyed (SuperCollider control buses / Lag / VarLag, Faust
`si.smoo`, Csound `port`, sst-basic-blocks `OnePoleLag`, oscpack and
scsynth's OSC parsers, and xlib's OSC codec): parameter smoothing is
one-pole or linear over ~10–100 ms, first value snaps rather than glides,
strict bounds-checked parsing (oscpack) coexists with fast trusting parsing
(scsynth), live servers treat immediate timetags as "now", and nobody
sanitizes NaN — which is sticky once it enters smoother state.

## Decision

- **The bus is a core primitive; the engine owns the instance.** A fixed
  `ControlBusCapacity` table of (name hash → atomic float + written flag).
  Registration is mutex-guarded find-or-create — control and OSC threads
  only; the audio thread reads through slot pointers resolved at Graph
  build, stable for the bus's lifetime because names never unbind. Writes
  to a full bus drop and count (ADR 0003's overflow policy). Living in
  `core` (not `engine`) lets the graph layer's param state name the slot
  type without a layering cycle.
- **`param("name", fallback)` is a graph-vocabulary leaf like Const.** The
  name folds into `type_hash` — identity that survives any edit, exactly
  the named-tap rule (ADR 0002) — and rides the op bytes for slot binding
  at Graph build; the fallback is a patchable value. Unwired (offline, or a
  full bus) a param plays its fallback; once the slot is written, the live
  value wins for the life of the process. Output goes through the same
  linear ~10 ms ramp Consts use — first value snaps, later changes glide —
  so every control path in the engine shares one glide semantic.
- **OSC arrives over plain sockets, not SDL3_net.** The need is one UDP
  receive socket on a background thread; SDL3_net would add all of SDL3 for
  that. The platform seam (winsock vs. BSD) is a handful of divergent lines
  confined to one host TU, the same shape as generation loading. Revisit
  only if a windowing/visual feature brings SDL into the tree anyway. The
  listener binds all interfaces (tablet controllers on the LAN are the use
  case) — automata trusts its network; the blast radius of a hostile packet
  is a float write into a bounded table.
- **The codec is in-house, oscpack-strict, ported from xlib.** Decode only,
  straight into the bus: address minus its leading '/' is the slot name;
  the first numeric argument (i f d h, T/F as 1/0) writes `name`, later
  numeric ones write `name/1`, `name/2`, … — hash-chained so `param
  ("pad/1")` addresses an xy pad's second float. Every read is
  bounds-checked; unknown type tags, truncation, or bad framing drop the
  whole message before anything is written. Bundles unpack recursively and
  timetags are ignored — a live-control surface is always "now". Non-finite
  floats are dropped (keeping their argument ordinal) so NaN can never
  poison a ramp.
- **The mouse is just reserved names.** The host polls the cursor each
  control tick into `mouse/x` / `mouse/y`, normalized 0..1 over the virtual
  desktop — y = 1 at the top, the theremin convention, not the screen's;
  `mouse_x()` / `mouse_y()` are `param` sugar with centered fallbacks. Platforms without an implementation are a no-op
  and patches still render — the pluggable-sources property.
- **Reserved names also flow the other way: `bpm` steers the transport.**
  The host's control tick reads the `bpm` slot and forwards changes
  (clamped 20–300) to the engine as `SetBpm` — the OSC thread never
  touches the engine inbox, whose single producer is the control loop.
  Every accepted OSC write is echoed to the console; the log is the
  confirmation a controller reached the bus.

## Consequences

- (+) Every def source keeps working identically: offline renders play
  fallbacks (no bus), tests inject a bus directly, the live host wires the
  engine's bus — one render path, per the properties list.
- (+) A controller can write slots before any patch reads them; the values
  are waiting when a `param` lands in a later edit.
- (−) Slots are floats and never unbind; a long session writing many
  distinct names can fill the bus (writes then drop and count). Capacity is
  the knob.
- (−) OSC address pattern matching (wildcards) and timetag scheduling are
  deliberately unsupported; senders must use literal addresses, now.
- (−) The first bus write shadows a param's fallback permanently (no
  "release back to default" gesture); that gesture, and scripted bus
  automation for offline renders, remain deferred.
- (−) Multi-argument messages beyond 32 numeric args stop writing (parse
  still validates); no real controller sends that.
