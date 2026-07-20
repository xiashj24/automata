# ADR 0005: Kernel/UGen authoring model

## Status

Proposed

## Context

The brief is explicit that the best part of the old design came from
relentlessly shrinking per-UGen boilerplate: deriving a node's input count
from its processing function's signature collapsed "declare inputs + write a
block loop" into "write a kernel + a one-line factory". It asks the rewrite
to optimize for "adding a UGen is almost free" from day one.

The old monorepo's kernels have a proven shape: a self-contained stateful
object — state and coefficients as data members, a `process`/`reset` core
plus per-kernel parameter setters (`set_*`, `update_coeffs`), no base
class, no allocation, normalized frequency. That shape earned its keep twice over: it collapsed the node
boilerplate, and the same files dropped unchanged into a conventional host
(the JUCE plugin sharing the monorepo's kernel layer). An earlier draft of
this ADR split each kernel into `Config`/`Values`/`State` structs with a
static `tick`; it was rejected because it breaks verbatim kernel reuse in
both directions — old kernels would need rewriting to port in, and automata
kernels would stop being drop-in usable elsewhere.

Two mechanical hazards are known in advance: MSVC treats `noexcept` as part
of a callable's deduced type, so signature-sniffing templates need dual
specializations; and ADR 0002 requires state transfer to be a memcpy.

## Decision

- **A kernel is a self-contained DSP object** in `automata/kernel/`:
  default-constructible, trivially copyable, no base class, no allocation,
  no graph knowledge, std-only includes. State and coefficients are data
  members — private, with in-class initializers as the reset state — so the
  named member functions are the only mutation paths. Two names are fixed
  vocabulary across every kernel (never `tick`):
  - `process(float...)` → `float` or an output aggregate — one sample in,
    one sample out; required.
  - `reset()` — return to initial state; required.

  Parameter setters are kernel-specific — `update_coeffs(cutoff,
  resonance)` on the SVF, `set_*` elsewhere — zero or more per kernel,
  named for their own domain; the factory binds them (below).

  ```cpp
  class Svf {
  public:
    struct Out {
      float lp, bp, hp;
    };

    void update_coeffs(float cutoff, float resonance);
    [[nodiscard]] Out process(float in);
    void reset();

  private:
    float s1_ = 0.f, s2_ = 0.f;            // state
    float gv_ = 0.f, r2_ = 0.f, h_ = 0.f;  // coefficients
  };
  ```

  Copying the object is copying the state: ADR 0002's transfer is a memcpy
  of the whole kernel, enforced by
  `static_assert(std::is_trivially_copyable_v<K>)` at registration. The
  same file drops unchanged into a JUCE plugin or a CLI experiment — the
  reuse property is kept by construction, not by discipline.
- **Audio inputs are deduced; control inputs are bound.** `process`'s
  parameters are the node's audio-rate inputs, deduced by signature
  sniffing (`phasor(w)` needs nothing else). Setter names can't be sniffed
  — they are kernel-specific — so the factory binds each setter to its
  control signals by member pointer, whose signature type-checks the
  binding at compile time. Node input order is `process`'s parameters,
  then control bindings in factory order. The function-traits template
  that sniffs `process` and each bound setter carries the dual `noexcept`
  specializations for MSVC, with a cross-platform static_assert suite
  compiled from day one.
- **Per-sample execution runs bound setters, then `process`** — each
  setter receives its control samples, then `process` its audio samples —
  so audio-rate modulation of a filter's cutoff is correct by default.
  Planned optimization, not day-one: a binding whose signals are all
  `Const`-fed is hoisted out of the loop and re-run only when a value
  patch lands. An optional `process_block` (span arguments) override
  remains the sanctioned vectorization path; correctness never requires
  it.
- **Multi-output kernels stay behind single-output nodes.** `process` may
  return a flat all-float aggregate (`Svf::Out{lp, bp, hp}`) — sibling
  outputs are byproducts of one pass inside an SVF. The node still emits
  exactly one stream: an *output selector* on the node — a value in ADR
  0002's sense, never hashed — picks the member, set by the factory as a
  member pointer. Because the selector is a value, `svf_lp` → `svf_bp`
  hashes identically: filter state survives and the edit lands as a value
  patch. A patch that wants two taps of the same filter writes two nodes
  and recomputes — the brief's stated preference, and consistent with ADR
  0002's no-CSE rule. Output aggregates must be standard-layout, all
  `float`; enforced at registration.
- **State lives in the fewest kernels possible.** When a UGen separates
  into a stateful core and a stateless map, write two kernels and compose
  in the factory: `sine(f)` is `SineShaper(phasor(f))`, not a monolithic
  stateful `Sine`. The phase accumulator then exists in exactly one kernel
  type, so a waveform edit (sine → saw) rehashes only the stateless shaper
  and ADR 0002's subtree matching carries the phase across the swap —
  monolithic per-waveform kernels have identical state layouts but no
  identity relation, so the phase would reset. The upstream design proved
  this factoring (`osc = sine(phasor(f) + phase)`); it also yields phase
  modulation for free.
- **Variable-size state is a template parameter, not a runtime tail.** A
  delay line is `Delayline<MaxSamples>`: the buffer lives inside the
  object, which stays trivially copyable, and transfer stays whole-object
  memcpy. Each instantiation registers as its own node type (the stable
  type name includes the capacity), so changing a capacity is a structural
  swap — the same rule ADR 0002 states for config. Patches write
  compile-time capacities; patches are C++, where that is natural.
- **A UGen is a one-statement factory** in `automata/ugens/`:

  ```cpp
  Signal svf_lp(Signal in, Signal cutoff, Signal q) {
    return make_node<Svf>(in)
        .control(&Svf::update_coeffs, cutoff, q)
        .output(&Svf::Out::lp);
  }

  Signal ar(Signal trig, Signal attack, Signal release) {
    return make_node<Ar>(trig)
        .control(&Ar::set_attack, attack)
        .control(&Ar::set_release, release);
  }
  ```

  `make_node<K>` takes the audio inputs, registers the node type on first
  use (type id = hash of a stable type name, not a function pointer —
  pointers differ across generations), lifts scalar arguments to `Const`
  nodes, and returns the node's single output `Signal`; `.control` binds
  setters, `.output` picks the emitted member of a multi-output kernel.
- **Describe runs against a scoped active graph.** Factories and `Signal`
  operators append to the graph installed by the patch entry point for the
  duration of describe (single-threaded, asserted). Chosen deliberately over
  threading a `GraphBuilder&` through every call: patch ergonomics is the
  product, and `svf_lp(saw(110.0f), cutoff, q)` must not become
  `g.svf_lp(g.saw(...), ...)` across every line of every patch. The context
  is scoped and asserted, not a mutable global in the xhal-style sense.
- **Custom kernels in patches use the identical machinery.** `make_node` on
  a kernel type defined inside the patch registers its member functions
  (pointers into the patch library), which pins the generation per ADR
  0001. One authoring model everywhere; host-vocabulary status is about
  where the code lives, not how it is written.

## Consequences

- (+) The old monorepo's kernels port by copy-paste — this is their exact
  shape (modulo making members private) — and automata kernels stay
  drop-in reusable in conventional hosts, with no engine types in their
  signatures.
- (+) A new UGen is a kernel class plus one factory line; input plumbing,
  block loops, hashing, value patching, and state transfer all come from
  the shared machinery.
- (+) Encapsulation without giving up relocation: private members make the
  three named functions the only mutation paths, while trivial copyability
  keeps transfer a memcpy.
- (+) `svf_lp`/`svf_bp`/`svf_hp` edits preserve state and land as value
  patches — the old system's cheapest common edit, kept under the
  single-output node rule.
- (−) Whole-object copy transfers derived coefficients alongside state —
  a few redundant bytes per node, overwritten by the next `update_coeffs`.
- (−) Setters run per sample by default — correct but costly (a `tan()`
  per filter per sample); the Const-fed hoist is the planned fix and
  `process_block` the escape hatch.
- (−) The scoped-active-graph context is invisible state during describe;
  misuse (constructing Signals outside a patch entry) is caught by assert
  at runtime, not by the compiler. Accepted for the authoring ergonomics.
- (−) Signature-sniffing templates are compiler-sensitive; the dual
  `noexcept` specializations and a cross-platform static_assert suite are
  part of Phase 1, not a portability fix deferred to "when MSVC breaks".
