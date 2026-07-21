# ADR 0010: Kernels speak physical units; setters cache their derivation

## Status

Proposed

## Context

ADR 0005 gave kernels normalized units (cycles per sample, samples) with
factories converting Hz and seconds through signal math, and accepted two
costs: every converted parameter spawns a Const and a Mul node per UGen —
buffers and per-sample multiplies whose product never changes — and bound
setters run per sample, so a derivation like the SVF's `tan()` or an
envelope pole's `exp()` reruns 48 000 times a second on an unchanged
value. ADR 0005 planned an engine-side fix (hoisting Const-fed bindings
out of the loop); phase 5 made the problem acute by adding more
`exp`-deriving setters (`are`, `smooth` — the first attempt's Smooth
carried a TODO about exactly this), and graphs grow large fast.

## Decision

- **Kernel setters take physical units — Hz and seconds.** Unit conversion
  happens inside the setter, next to the derivation it feeds; factories
  pass user arguments straight through. Kernels may include
  `automata/config.hpp` (compile-time constants only — `SampleRateF`,
  `InvSampleRate` live there now). Hz→increment uses division by
  `SampleRateF`, not multiplication by the inverse: correctly rounded, so
  a round frequency lands on an exact increment (12000 Hz is exactly 0.25
  cycles per sample).
- **Setters with a real derivation cache their inputs.** Raw inputs are
  members initialized to NaN — unequal to everything, so the first call
  always derives — and the setter early-outs when nothing changed:

  ```cpp
  void set_tau(float seconds) {
    if (seconds == tau_in_) {
      return;
    }
    tau_in_ = seconds;
    ...
  }
  ```

  A Const-fed binding then costs one compare per sample; modulation pays
  the derivation only while the value actually changes, and a value
  patch's glide derives per sample for its few milliseconds — which a
  click-free coefficient sweep requires anyway. The cache rides ADR
  0002's state transfer, so after a swap the derivation reruns only if
  the patched value differs; `reset()` restores the NaN sentinels.
  Setters that only convert (Phasor, Metro: one division) skip the cache.
- **This retires ADR 0005's planned Const-fed hoist.** The cache achieves
  the same collapse inside the kernel with no engine machinery, and it
  also covers cases the hoist could not (a modulated input that happens
  to sit still).

## Consequences

- (+) Every converted parameter sheds a Const and a Mul node from every
  patch; `ar(trig, a, r)` drops from seven nodes to five.
- (+) `tan`/`exp` run on change, not per sample; the cost model is honest
  — unmodulated parameters are near-free, modulated ones pay for what
  they use.
- (−) Kernels are no longer sample-rate-agnostic: the rate is baked in at
  compile time via config.hpp, and ADR 0005's "std-only includes" is
  relaxed to std plus config constants. Reuse in a runtime-rate host
  means porting the conversion lines, no longer a verbatim drop-in.
- (−) The seconds→samples round trip is inexact, so a stage boundary can
  land a sample off the nominal count; kernel tests probe with one-sample
  slack.
- (−) Cached inputs add a few bytes of state per node, transferred and
  harmlessly overwritten like any derived coefficient.
