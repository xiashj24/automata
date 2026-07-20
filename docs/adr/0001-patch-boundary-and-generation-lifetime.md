# ADR 0001: Patches describe graphs; the host executes them

## Status

Proposed

## Context

The old engine compiled each patch to a shared library whose code *ran on
the audio thread*: a live graph instance held pointers into its DLL
generation, so a generation could never be unloaded while any instance from
it might still be invoked (e.g. mid-crossfade). The old answer — never
unload anything — leaked resident code and on-disk copies without bound
across a live-coding session. The rewrite brief promotes fixing this to a
hard requirement, and separately requires first-class offline rendering and
executable-spec tests of the hot-swap invariants.

The brief leaves the patch boundary and even the engine language open, with
three fixed points: patches are written in the engine's implementation
language (no DSL), single-output nodes, CMake + CPM.

## Decision

- **Engine language is C++23**, following the xhal style baseline. Patches
  must be written in the engine's language; the proven DSP kernels are C++
  headers; any other engine language would put an FFI inside the live-coding
  loop and forfeit kernel reuse for no property gained. (A Rust port exists
  elsewhere as a separate learning project; this repo does not share that
  goal.)
- **The patch boundary is a description, not code.** A patch library exports
  one entry point (via the `AUTOMATA_PATCH` macro) that builds a `GraphDef`
  — flat, immutable, plain data. The host deep-copies the def; the audio
  thread executes only host-owned code. The DLL machinery becomes one
  producer of defs among several — tests hand-construct defs, the offline
  tool may link a patch statically — which is what makes hot-swap logic
  testable and offline rendering the same code path as live.
- **ABI stance: same toolchain, checked stamp.** The entry point carries a
  stamp (engine version, compiler id, `config.hpp` hash) verified before the
  def is trusted. Host and patches are built by the same compiler and CRT;
  this is a live-coding tool, not a stable plugin ABI, and pretending
  otherwise would force a C boundary that ruins the authoring ergonomics.
- **Generation lifetime is refcounted, and usually trivial.** The host
  shadow-copies `patch.dll` before loading (the build is never blocked by a
  file lock) and tracks each load as a `Generation` in a `GenerationPool`.
  A patch that only uses the host vocabulary references no generation code:
  its generation is unloaded, and its shadow copy deleted, immediately after
  describe returns. A patch that registers *custom kernels* (function
  pointers into its own code) pins its generation: the `Graph` built from it
  holds a generation reference, dropped on the control thread when the
  `Graph` is destroyed; the last drop unloads and deletes.
- **Destruction order is structural.** Graphs are destroyed on the control
  thread (never the audio thread), and the `GenerationPool` is declared
  before — thus destroyed after — the `Engine` in the host, so a generation
  is always mapped while anything built from it is torn down. This encodes
  the upstream ASan destruction-order crash as a rule instead of a bugfix.

## Consequences

- (+) The unbounded-bloat requirement is met by construction: resident
  generations are bounded by live graphs (≤ 2 rendering + in-flight
  retires), typically one; disk copies are cleaned at unload.
- (+) Offline rendering, deterministic tests, and the live host share one
  engine path; no test-only backdoors.
- (+) A crash in patch *description* code happens on the control thread at
  load time, not on the audio thread mid-performance.
- (−) New built-in vocabulary requires rebuilding the host (a restart, losing
  live state). Escape hatch: custom kernels defined in the patch itself
  hot-reload like any edit, at the cost of pinning their generation.
- (−) Same-toolchain requirement is a documented constraint; mixed-compiler
  setups are unsupported by design.
- (−) Describe crossing the DLL boundary with C++ types requires host and
  patch to share the dynamic CRT (`/MD` on Windows); enforced by the stamp.
