# ADR 0011: Feedback islands — sample-serial execution of tight tap cycles

## Status

Proposed

## Context

ADR 0004 specified the execution model — topological order of the SCC
condensation, with sub-block feedback cycles running as sample-serial
islands — but the first executor shipped as a flat def-order block loop, and
the tap read floored its look-back at one block. Any loop tighter than
128 samples (2.67 ms) was silently stretched: feedback FM, Karplus-Strong,
flangers, and one-pole-style loops were impossible. The old engine solved
this with a whole-graph sample-by-sample fallback (one `has_feedback` flag,
virtual dispatch per node per sample); Elementary never solved it at the
graph level (its tapIn/tapOut feedback is delayed by the engine block size,
tight recursion only inside built-in nodes). This ADR records the decisions
made while building the deferred half of ADR 0004.

## Decision

- **The scheduler lives in `automata/graph/schedule.hpp`** and runs at Graph
  build (control thread). Dependencies are the def's input edges plus one
  synthetic write→read edge per tap — that edge is what turns a feedback
  loop into a visible cycle for Tarjan. Multi-node SCCs are feedback cycles.
  The condensation is ordered by Kahn keyed on the smallest member index, so
  a pure DAG reproduces def order exactly. **The def's node order is never
  rewritten**: identity, structural hashing, and state transfer (ADR 0002)
  are untouched; the schedule is derived data inside the Graph.
- **`Graph` executes a precomputed op table** (resolved process fn, state,
  input pointers, values, output — everything fixed at construction) in
  schedule-order segments: block segments run whole blocks as before;
  island segments run their ops once per sample with input/output pointers
  offset to the current sample. Kernels need no changes — every process
  function is already nframes-generic, and per-sample bound setters are
  cheap under ADR 0010's cached derivation.
- **A cycle stays block-mode only when every in-cycle delay is a Const of at
  least one block** (ADR 0004's rule). A modulated delay could dip under a
  block, so it islands conservatively; range analysis is deferred.
- **Three tap-read process variants**, chosen per node by the scheduler:
  - *pre-write* (block-mode cycle): floor one block — exactly the old
    behavior, so long Const echoes cost nothing new;
  - *post-write* (no cycle): the read schedules after its write, making any
    delay ≥ 0 sample-exact in block mode — sub-block chorus/flanger without
    feedback is free, and `read(0)` is a passthrough;
  - *island*: floor one sample, `read(0)` is a true z⁻¹.
  Within an island sample, tap writes emit last, so every read sees the
  previous sample — safe while writes are sinks with unconsumed outputs.
- **A read inside an island whose write lives outside folds the write into
  the island** (the write's head must advance in lockstep). If it cannot
  fold — two islands sharing one tap, or the write inside another cycle —
  the whole graph runs as one island: the always-correct fallback the old
  engine used everywhere, kept only for this rare coupling. The host prints
  what the schedule decided.
- **A value edit that flips a cycle's execution mode escalates to a
  structural swap** (`schedule_equivalent` in the reconciler): the schedule
  is baked into the Graph, and def_hash cannot see values. State transfers
  in full, so the escalation is audible only as the crossfade.
- **Tap interpolation is 4-point Hermite** (the modulated-delay standard),
  falling back to linear inside the newest interval where the fourth point
  does not exist yet. Integer delays remain exact, so existing echoes are
  bit-identical.

## Consequences

- (+) `fb.read()` in a cycle is a true z⁻¹; sub-block and audio-rate
  modulated feedback delays work. The feedback vocabulary (FM feedback,
  Karplus-Strong, flangers) is expressible.
- (+) Only cycle nodes pay the per-sample dispatch (~a function-pointer call
  per island node per sample); feeders and consumers stay block-mode. The
  whole graph never degrades for one tight loop — except the documented
  cross-island tap coupling fallback.
- (+) Acyclic taps became strictly better: sample-exact at any delay ≥ 0.
- (−) An island's execution cost scales with cycle size × block size; a
  deliberately huge loop body is the author's choice, surfaced by the host's
  island note.
- (−) Editing a Const delay across the one-block threshold swaps instead of
  value-patching (crossfade instead of a 10 ms glide). Rare, and the tap
  buffer survives the swap.
- (−) `schedule_equivalent` runs two Tarjan passes per ownerless same-hash
  update — negligible at ≤ 1024 nodes.
