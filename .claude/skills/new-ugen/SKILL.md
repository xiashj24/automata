---
name: new-ugen
description: Kick off a new UGen for automata — prior-art survey, choosing the kernel shape, unit and cached-setter idioms, registration, tests, live verification. Use whenever adding a node or factory to the vocabulary.
---

# New UGen workflow

Work through the phases in order. The ADRs referenced are in `docs/adr/`;
read the relevant one before deviating from anything here.

## 1. Survey prior art (before any code)

Read at least two or three implementations from `reference/` and synthesize
an approach — don't transplant the first one found:

- **Broad coverage**: supercollider (`server/plugins/`), csound
  (`Opcodes/`), faustlibraries (`*.lib` — concise, math-first), Gamma,
  DaisySP, Soundpipe, stk, Maximilian, jsyn, klang, sapf, signalflow.
- **Specialists**: q (band-limited oscillators, polyBLAMP/BLEP),
  elliptic-blep, signalsmith-dsp (interpolation, delay, spectral),
  sst-filters / sst-waveshapers / sst-effects (production filter and
  shaper math), vital (wavetables, modulation), airwindows (character
  effects), music-dsp (the mailing-list archive of classic algorithms).
- The first attempt (the xlib repo, when available) often has a proven
  automata-shaped version — worth checking, and its kernels follow the
  same conventions.

Reference code is never linked or copied wholesale (ADR 0005): kernels are
rewritten dependency-free in project convention. Note where the algorithm
came from in one comment line (e.g. "cycfi::q's poly_blamp").

## 2. Choose the shape

- **Stateless pure map** → not a class. A plain function wrapped by an
  `fn` factory (ADR 0009). One-liners inline the lambda in the factory
  (`frac`, `soft_clip` in `automata/ugens/ugens.hpp`); anything longer is
  a named function in `automata/kernel/shapers.hpp` (`tri_from_phase_aa`,
  `swing_shape`). The fn *name* is the identity — body edits keep
  downstream state; renaming is structural.
- **Stateful DSP** → a kernel class in `automata/kernel/` (ADR 0005):
  trivially copyable, no base class, no allocation, no engine types;
  `process`/`reset` fixed names (never `tick`), kernel-specific setters.
  Exemplars: `svf.hpp` (cached coeffs, multi-output), `envelopes.hpp`
  (cached time params), `rhythm.hpp` (small trigger consumers).
- **Engine-coupled or values-driven leaf** (reads the Transport, the
  ControlBus, or a patchable float array) → a graph-layer data kernel:
  hand-written `KernelInfo` + process function. Exemplars:
  `automata/graph/clock.hpp`, `sequence.hpp`, `param.hpp`.
- **Multi-output** → `process` returns a flat all-float aggregate; the
  factory picks the member with `.output(&K::Out::m)`. The selector is a
  value, so lp→bp edits are value patches, and siblings come free from
  one pass.
- Variable-size state (delay lines) is a capacity template parameter,
  never a runtime tail — capacity change is a structural swap.

## 3. Write the kernel

- **Physical units in setters** (ADR 0010): setters take Hz and seconds
  and convert inside — factories pass user arguments straight through.
  Hz → increment divides by `SampleRateF` (correctly rounded; round
  frequencies stay exact). `automata/config.hpp` constants are the only
  non-std include a kernel may add.
- **Cache expensive derivation** (ADR 0010): any setter that derives
  (`tan`, `exp`, a division) caches its raw inputs behind a NaN sentinel
  and early-outs unchanged:

  ```cpp
  void set_tau(float seconds) {
    if (seconds == tau_in_) {
      return;
    }
    tau_in_ = seconds;
    ...derive...
  }
  // members:
  static constexpr float Uncached = std::numeric_limits<float>::quiet_NaN();
  float tau_in_ = Uncached;  // NaN: the first setter call always derives
  ```

  Setters that only convert (a single multiply/divide) skip the cache.
- **Real-time safety** (ADR 0003): nothing in `process`, `reset`, or a
  setter allocates, locks, logs, or blocks. Setters run per sample.
- **State survives edits by factoring** (ADR 0002/0005): keep state in
  the fewest kernels possible — `sine` is a stateless shaper over
  `Phasor`, so waveform edits keep the phase. Rhythmic position is
  derived from transport phase, never counted, unless the unit is
  genuinely trigger-driven (`step`, `Latch`).
- **Conventions**: filter resonance is normalized 0..1 (0 critically
  damped, 1 self-oscillation) — never raw Q. Times are seconds, T60
  where exponential. Clamp inputs defensively in the kernel.
- Style: `docs/style.md`; run clang-format on every file you touch.

## 4. Factory and registration

- One `[[nodiscard]]` statement in `automata/ugens/ugens.hpp`:
  `make_node<K>(audio...).control(&K::setter, sigs...)` or
  `fn("name", f, inputs...)`. Doc comment states the caller's contract
  (units, ranges) — nothing else. Default arguments over overloads.
- **Join the probe** in `automata/ugens/vocabulary.hpp` — every factory
  must be instantiated there. A forgotten one still plays but pins the
  generation of every patch using it. Data kernels also register via
  `add_data_kernel`.
- **Extend the coverage guard**: add the factory to
  `full_vocabulary_def()` in `automata/graph/rebind.test.cpp` — it fails
  rebinding to zero foreign nodes if the probe missed anything.

## 5. Tests

- Kernel test beside the kernel (`foo.test.cpp`, auto-globbed):
  `static_assert(std::is_trivially_copyable_v<K>)`, behavior probes, and
  reset-equals-fresh where state matters.
- Seconds round-trip through `SampleRateF` is inexact: probe stage
  boundaries with one sample of slack (see `envelopes.test.cpp`).
- Rhythm/transport semantics get an engine-level test in
  `automata/engine/clock.test.cpp`: offline render, probe one sample
  inside step boundaries, ±1-sample windows on onsets.
- If identity is subtle (value patch vs structural), pin it with a
  def_hash test (see "editing a seq step is a value patch").

## 6. Verify and finish

- Full build + `ctest` — the whole suite, not a filter.
- Live smoke test: run `build/host.exe` on a patch using the factory;
  expect `swap` with no unexpected "generation pinned" note (fn-based
  vocabulary rebinds; only patch-local kernels pin). Clean up
  `$LOCALAPPDATA/Temp/automata-shadow-*` if the host was hard-killed.
- If the UGen settled an open design question, record it: amend the
  relevant ADR or add a new one (`docs/adr/`).
- Commit when finished.
