# ADR 0003: Two-thread model and real-time discipline

## Status

Proposed

## Context

The upstream project's real-time-safety audit found heap allocation, `delete`,
and `printf` reachable from the audio thread in a design that already "looked"
real-time safe — all in the graph-swap path, the code that runs precisely when
a performer is paying attention. The lesson the brief draws: don't audit
safety in afterward; make the unsafe states unrepresentable from the start.
The proven fixes were deferred deletion over SPSC queues, fixed-capacity
containers, and a log queue — the same toolbox xhal's ADR 0007 codifies.

The brief also requires offline rendering with no device, and live-only
modulation sources (mouse, OSC) that degrade gracefully when absent.

## Decision

- **Two owning threads, bounded SPSC channels between them, nothing else.**
  - *Audio thread* (device callback, or the offline loop's caller): runs
    `Engine::render(out, nframes)` and nothing more — drain inbox, render,
    push retirements/logs to outbox. Never allocates, frees, locks, logs,
    or blocks.
  - *Control thread* (host main loop): watches the patch library, loads
    generations, describes, builds Graphs, diffs, feeds the inbox; drains
    the outbox — destroying retired graphs and tables, dropping generation
    references, printing queued log records. All allocation lives here.
  - *Inbox* messages: value patch (new values table pointer), swap request
    (`Graph` pointer + TransferPlan + quantize + fade), transport change.
    *Outbox* messages: retired graph, retired values table, log record.
- **External control is a `ControlBus`, not messages**: a fixed-capacity
  table of atomic floats keyed by hashed name. `param("name")` nodes load
  slots; writers are pluggable — the OSC receive thread and the control
  thread's mouse polling live in the host, not the engine. Offline, nothing
  writes the bus and slots hold their defaults, so a patch referencing the
  mouse still renders. Relaxed atomic float load/store per block is the
  entire synchronization story.
- **The primitives are xhal's, copied not re-derived.** `RingBuffer`
  (single-context fixed-capacity container) and `LockfreeQueue` (the SPSC
  channel) come from xhal's `core/` with only the namespace and assert
  renamed, together with their tests — including the real producer/consumer
  thread stress test. One deliberate adaptation: xhal omits cache-line
  padding on the atomic indices because a single-core Cortex-M0+ has no
  false sharing to pay for (its ADR 0007 says so explicitly); automata's
  audio and control threads are separate cores hitting the same line on
  every push/pop, so the copy restores
  `alignas(std::hardware_destructive_interference_size)` on `head_`/`tail_`.
  The cached-index/fenced variants (xlib v6/v8) stay out for the same
  "right amount of complexity" reason xhal gives: a few messages per block
  is not the contention they exist to absorb. (Reading v8 while settling
  this found its slot-reuse edge apparently under-ordered — no release
  fence between the pop-side slot read and the `head_` store, no acquire on
  the push-side reload — masked on x86 by TSO; exactly the audit tax the
  simpler protocol avoids.)
- **Fixed capacity everywhere on the audio thread**, sized in
  `automata/config.hpp`: nodes per graph, inbox/outbox depth, log record
  size, ControlBus slots, tap buffer capacities. Overflow policy is drop and
  count (xhal's `dropped_bytes` precedent), never block, never grow.
- **Swap concurrency is capped at two rendering graphs** (current +
  fading) plus one pending request with last-writer-wins — an editor saving
  three times during one crossfade means the newest wins, which is what a
  live coder expects. A swap landing while a fade is active snaps the older
  fade closed first. Bounded CPU: worst case two graphs render per block.
- **State transfer executes on the audio thread**, at the gated boundary,
  as the precomputed TransferPlan's memcpys. Rationale: the plan must read
  the *live* old state at the moment of swap; copying is bounded and branch-
  free (hundreds of KB worst case ≈ tens of µs against a 2.67 ms block
  budget); doing it anywhere else would need a second state snapshot channel
  with worse properties.
- **Retirement is the only deallocation path**: the audio thread never
  destroys anything; it pushes the pointer to the outbox and forgets it.
  `Graph` destructors run on the control thread, which is also where
  generation references drop (ADR 0001).

## Consequences

- (+) The audit checklist becomes a type inventory: if a structure on the
  audio thread isn't `RingBuffer`/`LockfreeQueue`/array/arena, it's a bug by
  definition — reviewable without dynamic analysis.
- (+) Offline rendering needs zero special casing: the "audio thread" is
  whatever calls `render` in a loop; queues and bus behave identically.
- (+) Mouse/OSC cannot couple the engine to a window system or socket —
  they are bus writers in the host, exactly the pluggability the brief asks
  for.
- (−) Last-writer-wins on pending swaps means rapid successive edits skip
  intermediate states — accepted, that is the desired live behavior.
- (−) Snapping the older fade truncates its tail mid-crossfade, so two
  saves landing within one fade (50 ms) can step audibly. The upstream
  design kept a ring of up to 15 concurrently fading graphs instead;
  rejected here because it unbounds worst-case audio work. Revisit only if
  real usage saves that fast.
- (−) ControlBus values are floats only; structured control (strings,
  arrays) has no path. Accepted until a real need appears; seq steps already
  travel the value-patch path instead.
- (−) A genuinely enormous patch could make TransferPlan copies material
  against the block budget; the plan carries its total byte count so the
  control thread can log a warning when a swap approaches the budget, but
  the engine does not split transfers across blocks (deferred until real).
