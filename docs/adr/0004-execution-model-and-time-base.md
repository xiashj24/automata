# ADR 0004: Execution model, fixed time base, and offline rendering

## Status

Proposed

## Context

The old engine ran the whole graph block-parallel until any feedback cycle
existed, then dropped the entire graph to sample-by-sample execution. It
fixed sample rate and block size as compile-time constants (48 kHz / 128),
which the brief flags as a real constraint worth an explicit decision rather
than an accident. The brief adds offline rendering as a first-class
requirement and names its load-bearing preconditions: the render step must
be a plain function of (state, sample count), and the musical transport must
advance by samples rendered, never wall clock.

## Decision

- **`GraphDef` compiles to a `Graph`**: a flat op schedule, pooled sample
  buffers allocated once at build (one block-sized buffer per node — a
  liveness-based reuse pass is deferred until node counts make it matter), a
  values table, and one contiguous state arena laid out per node. Build
  happens on the control thread; after build, a `Graph`'s memory shape never
  changes.
- **Schedule = topological order of the SCC condensation.** Acyclic regions
  execute block-mode: each node consumes input buffer spans and fills its
  own, once per block regardless of fan-out. A strongly connected component
  containing a feedback cycle executes as a sample-serial island *only if*
  the cycle's minimum tap delay is shorter than one block; a cycle decoupled
  by ≥ one block of delay stays block-mode, because the tap buffer already
  carries the dependency across the block boundary. The whole graph never
  degrades for one tight loop — refinement over the old all-or-nothing rule.
- **Sample rate and block size stay compile-time constants** in
  `automata/config.hpp` (defaults 48 kHz / 128). Explicitly re-chosen, not
  inherited: it keeps every kernel loop, buffer size, and transport
  increment constant-folded, and the cost is covered — miniaudio resamples
  when a device disagrees, kernels already take normalized frequency, and
  offline rendering uses the same constants. Changing rates is a rebuild.
- **The transport is sample-driven arithmetic**: bpm, beats-per-bar, and a
  beat position advanced by exactly the samples rendered inside
  `Engine::render`. `beat()`/`bar()` nodes read it; pending swaps gate on it
  (Immediate / NextBeat / NextBar), landing at the first block boundary at or
  past the musical boundary. Block granularity (~2.7 ms) is accepted as
  swap-timing jitter; sample-exact block splitting is deferred.
- **Offline rendering is the same call in a tight loop.** `Engine::render`
  touches no clock, no device, no I/O; a CLI tool (`tools/render`) loads or
  links a patch, loops render, and writes a WAV via miniaudio's encoder —
  deterministic and faster than real time by construction. Determinism
  contract: no wall-clock reads anywhere in the engine, and any stochastic
  node takes an explicit seed (as config).

## Consequences

- (+) Offline and live rendering cannot drift apart — there is one render
  path, and the tests exercise it directly.
- (+) Tempo is exact offline: beat positions depend only on rendered sample
  count.
- (+) Tight feedback costs per-sample dispatch only inside its own island;
  a patch with one FM feedback pair keeps the rest of the graph block-mode.
- (−) A device that can't do 48 kHz runs through miniaudio's resampler —
  latency and a little CPU. Accepted for the simplification bought.
- (−) Compile-time block size means the engine cannot follow a host-imposed
  callback size; the device layer accumulates/splits as needed.
- (−) Swap timing quantizes to block boundaries; a musically noticeable case
  would motivate the deferred sample-exact split, none is expected at 128.
