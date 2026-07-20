# ADR 0006: Defs are adopted by rebinding kernel infos and bound ops

## Status

Proposed

## Context

ADR 0001 promises that a vocabulary-only patch references no generation code
and unloads immediately after describe. Implementing the host exposed what a
def actually carries out of a patch library: every node's `KernelInfo*`
points at that library's own instantiation of the info statics (an inline
template instantiated on both sides of the boundary), and `make_node`'s op
bytes are `SetterPack` member-function pointers into that library's code.
Left alone, either one keeps the generation pinned — or crashes after it is
unloaded — even for a patch that uses nothing but the host vocabulary.

The def's plain data (nodes, values, configs, tap ids) has no such problem:
it lives in host-owned vectors on a shared CRT heap.

## Decision

- **Adoption is a rewrite, not a copy.** After describe returns, the host
  rebinds the def in place: for every node whose kernel the host knows, the
  `KernelInfo*` is replaced with the host's own static, and — for kernels
  whose op bytes are a setter binding — the op bytes are overwritten with
  the host's canonical bytes for that binding. Plain-data op bytes (tap
  pairing ids) are kept verbatim.
- **The key is the info's own `type_hash`, read through the still-loaded
  library.** Node `type_hash` won't do: tap nodes fold their tap id into it.
  `node.kernel->type_hash` is stable across generations by construction
  (ADR 0005's stable-type-name identity), and reading it is safe because
  rebinding happens before the generation can be released.
- **The registry is harvested from a host-side probe describe.** The
  vocabulary registry maps `type_hash → (host info, canonical op bytes)`:
  the graph-owned data kernels (Const, tap read/write) are registered
  explicitly; every `make_node` kernel comes from one probe graph that
  invokes each factory in `ugens.hpp`, so the canonical pointers and op
  bytes are compiled-in host artifacts, never hand-maintained tables.
- **Unknown kernels stay foreign and pin.** A node whose hash the registry
  lacks keeps its pointers; `rebind_kernels` returns the count, and the host
  attaches the generation as the def's owner (a `shared_ptr` token the
  Reconciler holds per built Graph, released on the control thread when that
  Graph retires). Zero foreign nodes → no owner → the generation unloads at
  the next pool collect. Correctness never depends on the registry knowing a
  kernel — an out-of-date host plays a newer patch's nodes from the patch's
  own code, just without immediate unload.

## Consequences

- (+) ADR 0001's lifetime story holds mechanically: vocabulary-only
  generations unload right after describe; custom-kernel generations live
  exactly as long as a Graph using them.
- (+) The probe keeps registration honest: a new factory is covered by
  writing the factory itself into the probe, and a full-vocabulary rebind
  test fails if one is forgotten.
- (−) Two same-arity setter bindings of one kernel share a `type_hash`
  (binding identity hashes arities, not member names), so the registry
  cannot distinguish their canonical ops; registration asserts byte-equal
  duplicates to surface a collision the day one appears.
- (−) A patch-defined kernel whose stable type name and binding shape
  collide with a vocabulary kernel is rebound to the host's implementation —
  the same identity rule ADR 0005 already accepts across generations.
- (−) Forgetting a factory in the probe is silent at runtime (correct sound,
  pinned generation); only the coverage test reports it.
