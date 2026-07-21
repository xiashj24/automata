# ADR 0008: Musical time — beat position on the transport, phase-ramp clocks in the graph

## Status

Proposed

## Context

Phase 5's vocabulary (seq, euclid, clock division, swing) needs musical
time inside nodes, and ADR 0004 promised it — "`beat()`/`bar()` nodes read
it" — without fixing the interface. Two constraints box the answer in:
kernels are self-contained objects with no engine types in their signatures
(ADR 0005), so a kernel cannot see the `Transport`; and a running sequence
must keep its place across a hot-swap (the project's first non-negotiable).
The transport itself is sample-driven arithmetic mutated only on the audio
thread, with one known flaw: `next_boundary` derives the beat grid from
absolute sample position assuming bpm was constant since sample 0, so a
mid-run tempo change shifts the whole grid.

Prior art surveyed (SuperCollider server ugens, Csound metro family, Faust
basics.lib, Gamma, DaisySP, Soundpipe, sst-basic-blocks, signalflow): the
currency between rhythmic units is either a trigger pulse or a phase ramp,
and nearly every trigger is secretly a wrap event of a ramp (SC's `Impulse`
says so in a comment). Units that *accumulate* phase track tempo but drift
from absolute beat; the one design that exposes absolute position every
sample — Ableton Link via SC's `LinkPhase` — lets units re-derive position
instead. Long-running phase is kept in `double` where it matters (SC,
Csound, sst — "to avoid drift for 5-minute voices"). Swing appears either
as a second phasor with an offset wrap threshold (Csound `metro2`) or as
duration data; sequencers step either on trigger edges (Soundpipe `tseq`,
SC `Stepper`) or by consuming a position directly (Csound `timedseq`).
Only signalflow ships Euclidean rhythms (recursive Bjorklund, recomputed
on parameter change).

The first automata attempt (in the xlib repo) had already converged on the
same core: a `double` beat position advanced per block, with stateless
beat/bar nodes re-deriving phase from block-start position. It reached the
transport through a mutable global injected into each DLL, divided clocks
with a wrap-counting node whose divisor could not be patched, and stepped
sequences on trigger edges with a stored index — the details this ADR
weighs and partly replaces.

## Decision

- **The transport carries continuous beat position.** `Transport` gains
  `double beat_pos`, advanced each block by `nframes / samples_per_beat()`.
  A tempo change only alters future increments, so the grid is continuous
  across it — the TempoClock/Link semantic — and `next_boundary` computes
  swap boundaries from `beat_pos`, fixing the constant-bpm assumption.
  Double accumulation keeps sub-sample beat precision for days and is
  deterministic offline (same arithmetic, same sample count).
- **Musical time enters the graph through one leaf: `clock(beats)`.** A
  data-kernel node like `param` (ADR 0007), bound to a `const Transport*`
  at Graph build. Each block it re-derives phase from absolute position —
  `fract(beat_pos / beats)` advanced per sample at the current tempo, all
  in double, emitted as a float 0..1 ramp. Never integrated: no drift, a
  tempo change re-derives cleanly, and every clock in every graph is
  phase-locked to every other by construction — sync needs no wiring, and
  clock *division* is just a longer cycle (`clock(4)` against `clock(1)`).
  `beats` is a patchable value, so retiming a clock is a value patch.
  Unbound (a bare `Graph` with no transport) a clock emits 0.
- **Position is derived, never stored.** Rhythmic units consume the phase
  ramp and compute their place arithmetically — a sequencer's step is
  `floor(phase * N)`, not a counter advanced by edges. This is what makes
  the hot-swap invariant free: there is no position state to transfer, so
  a structural swap lands on the grid automatically, and during a crossfade
  both generations read the same transport and play the same step. Step
  content rides ordinary inputs (Consts lift from literals), so editing a
  step is a value patch with the standard glide.
- **Triggers are derived from phase by wrap detection.** The currency is a
  single-sample impulse (1.0); `trig(phase)` fires when the ramp drops by
  more than half a cycle — a threshold, not `<`, so warped ramps can't
  false-trigger — keeping one previous-sample float initialized to 1 so a
  fresh trig fires the downbeat at cycle start while state transfer keeps a
  hot-swap from re-firing it (both proven by the first attempt's
  `ClockTrig`). Stateful consumers fire on a rising edge through zero, so
  gates work as triggers — and gates fall out of comparison
  (`phase < width`), stateless. `trig` accepts any ramp: a free-running
  metro is `trig(phasor(f))`, no transport involved.
- **Swing is a stateless phase warp.** `swing(phase, amount)` piecewise-
  linearly stretches the first half of the cycle and compresses the second;
  everything downstream — trig, seq, euclid — lands swung with no unit
  aware of it. Chosen over Csound's dual-phasor because it composes: one
  warp swings an entire derived chain.
- **Euclid is arithmetic, not Bjorklund recursion.** Hit at step `i` iff
  `((k·(i+r)) mod n) + k ≥ n` — sndkit's closed form, carried over from the
  first attempt; algebraically the floor-difference construction and
  rotation-equivalent to the Bjorklund necklaces. Integer-only per query,
  no pattern buffer to rebuild on a parameter change, real-time safe by
  construction.

## Consequences

- (+) Every rhythmic unit is on the grid after any edit: swap, tempo
  change, or crossfade — position re-derives from the transport instead of
  surviving in per-node counters.
- (+) `beat()`/`bar()` from ADR 0004 become sugar over `clock`.
- (+) Kernels stay engine-free: only the `clock` leaf touches the
  transport, the same boundary `param` draws around the ControlBus.
- (−) Phase is a float32 signal: a cycle's per-sample increment falls
  below float resolution past roughly 700 beats at 120 bpm, so very long
  cycles quantize. Accepted; loops are short.
- (−) No absolute beat/bar *number* reaches the graph (a monotonic count
  would exhaust float32 in hours). Anything needing "bar 17" waits for a
  future decision.
- (−) A derived step index cannot express "advance on each trigger" — a
  melody stepping on euclid hits, or on audio-derived pulses. The companion
  is an ordinary trigger-counting kernel (the first attempt's `Seq` is the
  shape, stale-index guard and all); planned vocabulary, not an engine
  change.
- (−) A `beats` value patch re-derives phase against the new cycle length
  immediately — correct grid, but the phase value itself jumps; downstream
  trig can fire an extra pulse at the moment of the edit.
