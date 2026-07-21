# automata architecture

automata is a live-coding audio synthesis engine. A patch is an ordinary C++
file compiled to a shared library; the host hot-reloads it on every edit, and
the running sound keeps its DSP state (filter memory, oscillator phase, delay
lines) across the edit. This document is the core design; each load-bearing
decision has an ADR in `docs/adr/` with the full rationale.

Designed from the property list in the rewrite brief, deliberately without
reference to the previous implementation.

## Properties that define the system

These are invariants, not features — everything below exists to serve them:

1. **State-preserving hot-swap.** A live edit never resets DSP memory that
   structurally survived the edit. Scalar-only edits are near-free; structural
   edits transfer state and crossfade.
2. **Real-time safety.** The audio thread never allocates, frees, locks,
   logs, or blocks. Not "looks safe" — auditable by construction: every
   audio-thread data structure is fixed-capacity, every cross-thread channel
   is a bounded SPSC queue.
3. **Adding a UGen is almost free.** A new node is a dependency-free kernel
   plus a one-line factory. No hand-written per-block loops, no manual input
   declarations.
4. **Bounded hot-reload footprint.** Patch-library generations are reclaimed
   once nothing references them. A day-long session holds a small constant
   number of generations resident, not one per edit.
5. **Offline rendering is first-class.** Render N seconds into a buffer or
   WAV deterministically, faster than real time, with no audio device — the
   same code path the live host uses, not a parallel one.

Decisions fixed by the brief: single-output nodes only; CMake + CPM for
dependencies; no patch DSL — patches are written in C++ and hot-reloaded,
full stop.

## The load-bearing inversion: descriptions in, execution stays home

The single deepest decision (ADR 0001): **a patch describes a graph; it does
not execute one.** The patch library's entry point builds a `GraphDef` — a
flat, immutable, plain-data description of nodes and their connections. All
code that runs on the audio thread belongs to the host: the engine and its
UGen vocabulary are compiled into the host binary, and a `Graph` (the
executable form of a def) is assembled entirely from host-owned code plus
data copied out of the def.

Everything hard about the old design gets easier under this inversion:

- **Offline rendering** is trivial: the engine consumes defs; it does not
  know or care whether a device, a test, or a CLI batch loop is driving it.
- **Testing** needs no DLLs: tests hand-construct defs and drive
  `Engine::render` directly. The hot-swap logic — the heart of the system —
  is exercised devicelessly and deterministically.
- **Generation reclamation** mostly dissolves: a patch library that only uses
  the host vocabulary can be unloaded the moment its def has been copied
  out. Only patches that register *custom kernels* (function pointers into
  their own code) pin their generation, via refcount, until every Graph
  using them has retired (ADR 0001).

The DLL hot-reload machinery is therefore peripheral, not core: one producer
of GraphDefs among several (live host, offline tool, tests).

Implementation language: C++23, same toolchain discipline as xhal. Patches
are written in the engine's language by requirement, and the proven kernels
are C++ headers; a different engine language would put an FFI in the middle
of the live-coding loop for no property gained.

## Layers and dependency rules

| Layer            | Contents                                                        | May depend on        |
| ---------------- | --------------------------------------------------------------- | -------------------- |
| `automata/core`  | assert, hash, `RingBuffer`, `LockfreeQueue`, `ControlBus`, `Transport` | nothing        |
| `automata/kernel`| pure DSP kernels (phasor, svf, delayline, envelopes, …)         | nothing              |
| `automata/graph` | `Signal`, `GraphBuilder`, `GraphDef`, structural hashing        | core                 |
| `automata/ugens` | UGen vocabulary: thin factories binding kernels into the graph  | graph, kernel        |
| `automata/engine`| `Graph`, reconciliation, `Engine`, OSC decode                   | core, graph          |
| `host/`          | live host: audio device, DLL generations, OSC, mouse            | engine, ugens, deps  |
| `tools/`         | offline renderer CLI                                            | engine, ugens        |
| patches          | user code; includes `automata/patch.hpp` (graph + ugens only)   | graph, ugens         |

Kernels know nothing about the graph — they are plain structs testable in
isolation (ADR 0005). Patches never see the engine.

## Data model

A **node** is one unit generator in the signal graph. Its description has
four kinds of ingredients, and keeping them distinct is what makes two-tier
hot-swap work (ADR 0002):

| Ingredient | Examples                                   | Hashed? | On matching edit    |
| ---------- | ------------------------------------------ | ------- | ------------------- |
| inputs     | which nodes feed this one                  | yes     | —                   |
| config     | delay capacity, seq max length             | yes     | — (change = swap)   |
| values     | a constant's value, seq steps, filter mode | no      | patched in place    |
| state      | filter memory, phase, delay contents       | no      | transferred across  |

- `Signal` — a value-type handle to one node's single output, created during
  describe. Arithmetic (`+ - * / unary-  < >=`) composes new nodes; scalars
  auto-lift to `Const` nodes whose value is patchable.
- `GraphDef` — the finalized, immutable description: flat node array in
  deterministic schedule order, each node carrying its structural hash.
- `Graph` — the executable instance: op schedule, pooled sample buffers,
  a values table, and one contiguous trivially-copyable state arena.

Sharing is what the patch author expresses: using one `Signal` in two places
is one node computed once per block. There is no hash-based common-
subexpression elimination — two separately written identical subtrees stay
two nodes (predictability over cleverness; ADR 0002).

## Identity and reconciliation (ADR 0002)

Identity is structural: `hash(type, config, input hashes)`, values excluded.
Equal-hash nodes within one graph are disambiguated by occurrence ordinal in
schedule order, so identity is the pair (hash, ordinal). Source location
appears nowhere — inserting a line or reformatting cannot reset state (the
old project proved location-based identity wrong; we never build it).

On a new def from any source, the control thread diffs against the running
def:

```
                 new GraphDef
                      │
        full-def hash equal to live?
             │                  │
            yes                 no
             │                  │
      ValuePatch msg      build Graph, match nodes by
      (new values table,  (hash, ordinal), compute
      Consts ramp ~10ms)  TransferPlan (memcpy list)
             │                  │
             └────► audio thread ◄────┘
                    inbox (SPSC)
```

- **Value patch** — the common case (tweak a constant, edit seq steps, flip
  an svf's LP/BP/HP mode value): the audio thread swaps in a new values
  table; `Const` nodes ramp linearly to their new value (~10 ms default) so
  a hot-reloaded gain change glides instead of stepping. No swap, no
  crossfade, no state touched.
- **Structural swap** — the audio thread, at the transport-gated boundary,
  executes the precomputed TransferPlan (bounded memcpys old→new state
  arena), then runs both graphs under a linear crossfade (50 ms default).
  The faded-out graph goes to the outbox for control-thread destruction.

Feedback uses `Tap`: a write node owning a short delay buffer and read nodes
with fractional (Hermite-interpolated) offsets. A named tap (`g.tap("x")`)
hashes by name — the explicit-identity escape hatch; an anonymous tap hashes
by creation ordinal and is documented as fragile under reordering edits.

## Execution model and time base (ADR 0004)

Sample rate and block size are compile-time constants (`automata/config.hpp`,
48 kHz / 128 by default). miniaudio resamples for devices that differ;
offline rendering uses the same constants.

A `Graph`'s schedule is the condensation of its nodes into strongly
connected components, in topological order (`graph/schedule.hpp`, ADR 0011).
Acyclic regions run block-mode (each node processes a full block into its
pooled buffer). A component that contains a feedback cycle whose minimum tap
delay is shorter than one block — or whose delay is modulated — runs as a
sample-serial island where `read(0)` is a true z⁻¹; cycles decoupled by a
Const of ≥ one block stay block-mode. A tap read outside any cycle schedules
after its write and is sample-exact at any delay ≥ 0. Only the parts that
need per-sample execution pay for it; a value edit that flips a cycle's mode
escalates to a structural swap, since the schedule is baked into the Graph.

`Engine::render(out, nframes)` is a plain function of (state, nframes):
drain the inbox, advance the schedule, push retirements to the outbox. The
`Transport` (bpm, beats-per-bar, beat position) advances by samples rendered
— never wall clock — which is what makes offline rendering land on the exact
same beats. Pending swaps are gated Immediate / NextBeat / NextBar and land
at the first block boundary past the musical boundary. Musical time reaches
the graph through `cycle(beats)` leaves that re-derive a phase ramp from the
transport's beat position every block — position is computed, never
accumulated, so rhythmic units stay on the grid across swaps and tempo
changes (ADR 0008).

## Threads and real-time discipline (ADR 0003)

| Thread     | Role                                                                | Allocates? |
| ---------- | ------------------------------------------------------------------- | ---------- |
| audio      | `Engine::render` only: inbox → render → outbox                      | never      |
| control    | watch DLL, describe, build Graph, diff, drain outbox (frees, logs)  | yes        |
| OSC (opt.) | UDP receive → write `ControlBus` slots                              | setup only |

All audio↔control traffic flows through two bounded SPSC queues (inbox:
value patches, swap requests, transport changes; outbox: retired graphs
and tables, log records). External control is a `ControlBus` — a fixed table
of atomic floats; `param("name")` nodes read slots, OSC and mouse polling
write them. Offline, the bus simply holds defaults — live sources are
pluggable by construction, so a patch referencing the mouse still renders.

At most two graphs render concurrently (current + fading) plus one pending
swap slot with last-writer-wins; a swap landing mid-fade snaps the older
fade. Everything on the audio thread is fixed-capacity; overflow drops and
counts, never blocks.

Lifetime rule (learned the hard way upstream): Graphs hold references to
the generations whose code they use, and the `GenerationPool` outlives the
`Engine` — declaration order in the host enforces destruction order.

## Patch authoring (ADR 0001, ADR 0005)

A patch is one file:

```cpp
#include "automata/patch.hpp"

using namespace automata;

AUTOMATA_PATCH(g) {
  auto lfo   = sine(0.25f);
  auto env   = ar(metro(2.0f), 0.005f, 0.12f);
  auto voice = svf_lp(saw(110.0f), 800.0f + lfo * 600.0f, 0.7f) * env;
  auto fb    = g.tap("fb");
  auto wet   = voice + fb.read(0.375f) * 0.35f;
  fb.write(wet);
  g.out(soft_clip(wet), soft_clip(wet));
}
```

`AUTOMATA_PATCH` exports an `extern "C"` describe entry point plus an ABI
stamp (engine version, compiler, config) the host checks before trusting the
library. Host and patch must be built by the same toolchain — accepted and
documented; this is a live-coding tool, not a plugin ABI.

The macro is export plumbing, not the unit of composition: it appears exactly
once per hot-reloadable library, as its root. A sub-patch is an ordinary
function taking and returning `Signal`s — nestable, spread across files,
shared between patches — composed by plain function calls.

Adding a UGen (ADR 0005): write a kernel — a self-contained, trivially
copyable DSP class with `process` and `reset` plus its own parameter
setters (`update_coeffs`, `set_*`, …), state and coefficients private,
reusable verbatim in a conventional host (JUCE plugin, CLI tool) — then a
one-statement factory:

```cpp
Signal svf_lp(Signal in, Signal cutoff, Signal q) {
  return make_node<Svf>(in)
      .control(&Svf::update_coeffs, cutoff, q)
      .output(&Svf::Out::lp);
}
```

`make_node` deduces the audio inputs from `process`'s signature (with the
MSVC `noexcept`-deduction workaround planned from day one); `.control`
binds each kernel-specific setter to its control signals by member pointer.
A kernel may compute several outputs in one pass — `Svf::Out{lp, bp, hp}` —
while the node emits only the member the factory selects; the selector is a
value, so editing `svf_lp` → `svf_bp` preserves filter state (ADR 0002).
State transfer is a memcpy of the whole kernel object. Block processing is
a loop over `process`; kernels may provide a `process_block` override when
it earns its keep. Patches may register custom kernels through the same
interface; doing so pins their generation until retirement (ADR 0001).

## Generation lifetime (ADR 0001)

1. Build produces `patch.dll`; the host shadow-copies it (so the compiler is
   never blocked by a file lock) and loads the copy.
2. Describe runs against a host-owned builder; the host rebinds known
   kernels to its own statics and canonical op bytes (ADR 0006), leaving
   only custom kernels pointing into the generation.
3. Vocabulary-only patch → the generation is unloaded immediately and its
   shadow copy deleted. Custom-kernel patch → the resulting Graph holds a
   generation reference; the control thread drops it when the Program is
   destroyed, and the last drop unloads and deletes.

Resident generations are bounded by live graphs (≤ 2 + in-flight retires),
typically one. No unbounded growth in memory or on disk.

## Repository layout

```
automata/            library (namespace automata; #include "automata/...")
  config.hpp         compile-time constants: sample rate, block size, capacities
  patch.hpp          the single header a patch includes
  core/  kernel/  graph/  ugens/  engine/      (*.test.cpp beside sources)
host/                live host executable
tools/render/        offline renderer CLI (patch → WAV)
patches/             example patches (also the smoke tests for authoring UX)
docs/  adr/          this document; decision records
reference/           submodules surveyed for DSP prior art (never linked)
```

Build: CMake + CPM. Targets `automata::automata` (static lib), `host`,
`render`, an `automata_add_patch()` helper for patch modules, and xhal-style
auto-globbed Catch2 tests (`*.test.cpp` → one target each). Dependencies:
miniaudio (device I/O *and* WAV encoding) and Catch2 (tests); OSC rides
plain UDP sockets with an in-house codec (ADR 0007).

## Build order

The inversion dictates the order — the engine is complete and fully tested
before a DLL or device ever appears:

- **0** core primitives (`RingBuffer` and `LockfreeQueue` ported from xhal
  with their tests — ADR 0003), hash, config, scaffolding (repo layout, CI).
- **1** kernels + GraphBuilder + GraphDef + Graph + block execution +
  offline render to WAV. The brief's invariant tests come alive here.
- **2** reconciliation: value patch, structural swap, state transfer,
  crossfade, taps/feedback, transport gating — all tested devicelessly.
- **3** live host: miniaudio device, DLL generations, file watcher.
- **4** external control: ControlBus, OSC, mouse.
- **5** vocabulary buildout to the full brief list (seq, euclid, clock
  division, swing, shapers, …).

## Deliberately deferred

- Buffer-pool liveness reuse (one buffer per node is fine at current scale).
- Sample-exact swap boundaries (block granularity first).
- Offline automation input (scripted ControlBus writes for batch renders).
