# ADR 0002: Structural identity and two-tier reconciliation

## Status

Proposed

## Context

State-blind hot-swap is the problem the whole system exists to solve:
rebuilding the graph on every edit resets filter memory, phase, and delay
lines, which is musically useless. The upstream project proved two things
the hard way: source-location identity (file, line) breaks on exactly the
edits live coding makes (inserting a line reformats everything below it),
and content-addressed structural identity — Elementary Audio's model —
fixes it by construction. It also proved that a node's tunable knobs and its
transferable DSP memory must be treated as different things, patched in
opposite directions.

The brief fixes single-output nodes, which removes output-channel selection
from identity — but the old design used multi-output selection for the
common live edit of flipping an SVF between LP/BP/HP, and that edit must
stay on the cheap path.

## Decision

- **Every node's description separates four ingredient kinds**, and the
  split is the API, not a convention:
  - *inputs* — which nodes feed this one; structure.
  - *config* — scalars that shape allocation or meaning (delay capacity,
    seq max length); structure.
  - *values* — scalars a running graph can absorb in place (a constant's
    value, seq steps, an svf's mode); never identity.
  - *state* — DSP memory (trivially copyable bytes); never identity,
    transferable across generations.
- **Identity is `hash(type, config, input hashes)`** — 64-bit, values
  excluded, no source locations anywhere. Cross-generation matching pairs
  equal-hash nodes positionally: a descent from the outputs (and sinks)
  pairs matching subtrees input-by-input, so reordered statements cannot
  cross-wire the many equal-hash leaves (every `Const` hashes alike); a
  second pass pairs still-unmatched *non-leaf* subtrees globally by hash,
  parents first, which is what keeps state when an edit merely wraps or
  moves a subtree. Bare leaves never pair on their own — only through a
  parent — because they carry no distinguishing context.
- **Sharing is author-expressed, not inferred.** Reusing one `Signal` in two
  places is one node computed once per block (fan-out dedup). There is no
  hash-based CSE: two separately written identical subtrees remain two
  nodes. Values are excluded from hashes, so hash-equality cannot prove
  value-equality — and a live coder who writes two oscillators means two
  oscillators.
- **Two-tier update, chosen by full-def hash comparison** on the control
  thread:
  1. *Value patch* (hashes identical): build a new values table off-thread;
     the audio thread pointer-swaps it in. `Const` nodes ramp linearly to
     new values (~10 ms default) so a gain edit glides; other values (seq
     steps, offsets, modes) snap. No swap, no crossfade, no state touched.
  2. *Structural swap* (hashes differ): build the new `Graph` off-thread,
     match nodes by (hash, ordinal), and precompute a `TransferPlan` — a
     flat list of (old offset, new offset, size) state-arena copies. At the
     transport-gated boundary the audio thread executes the plan (bounded
     memcpys; state is trivially copyable by kernel contract, so relocation
     is a copy), then crossfades old→new linearly (50 ms default). State is
     copied, not moved: the fading old graph keeps evolving its own copy.
     Unmatched new nodes start from reset — structure genuinely changed.
- **An output selector keeps multi-output kernels behind single-output
  nodes.** A kernel may compute sibling outputs in one pass (the SVF's
  lp/bp/hp); the node emits exactly one of them, chosen by a selector that
  is a value — never hashed. `svf_lp`/`svf_bp`/`svf_hp` are one node type
  differing only in selector, so flipping among them is a value patch that
  preserves filter state — the old system's cheapest common edit, kept
  without reintroducing multi-output nodes (mechanism in ADR 0005).
- **Taps are the feedback primitive**: a write node owning a short delay
  buffer (capacity is config), read nodes taking fractional interpolated
  offsets (offset is a value or a signal). A named tap hashes by its name —
  the explicit-identity escape hatch that survives edits hash matching
  cannot disambiguate. An anonymous tap hashes by creation ordinal and is
  documented as fragile under reordering; naming is the fix.

## Consequences

- (+) Reformatting, reordering, and inserting lines cannot reset state —
  the failure mode that killed location identity is unrepresentable.
- (+) The common live edits (constants, seq steps, filter mode, delay read
  offset) are near-free: no rebuild of the running graph, no fade.
- (+) (hash, ordinal) matching gives stable pairing even when a patch
  contains several structurally identical nodes.
- (−) An edit that changes structure *and* expects unrelated identical
  nodes to keep state pairs them by ordinal, which can mismatch if the edit
  also reorders them. Accepted: the escape hatch is naming (taps today; a
  general `id("name")` wrapper can follow the same rule if practice demands
  it).
- (−) Config changes (e.g. delay capacity) force a structural swap even
  though the edit "feels" scalar. Deliberate: capacity shapes allocation,
  and allocation never happens on the audio thread.
- (−) 64-bit hash collisions are theoretically possible; with per-graph node
  counts in the hundreds this is ignored by design (no collision handling).
