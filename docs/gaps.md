# System-level gaps: a survey of six engines

Surveyed July 2026 against the `reference/` submodule snapshots: Csound,
Elementary, JSyn, klang, sapf, SuperCollider. The question: what machinery do
mature synthesis systems carry at the *engine* level — voice management,
events, buffers, I/O, observability — that automata does not? UGen
vocabulary is deliberately out of scope; this is about infrastructure.

For calibration, what automata already holds, which several surveyed systems
do not: per-sample DSP state preserved across live edits (JITLib restarts
synths and masks the reset with a crossfade; Csound versions definitions and
lets old instances play out), sample-serial feedback islands (every surveyed
system's graph-level feedback is block-delayed — SC `InFeedback`, Elementary
`tapIn`/`tapOut`, JSyn's pull-graph cycle break), and offline rendering that
shares the live path bit-for-bit (only Csound and SC/NRT match this).

## What each system contributes

**SuperCollider** (`reference/supercollider`) — the dynamic-graph
counterpoint to automata's fixed topology. Synths spawn at runtime into an
ordered node tree and *free themselves*: a UGen fires `DoneAction`, the
server runs a switch that unlinks the node after the current block
(`server/scsynth/SC_Node.cpp`). Above that sits the Pattern → Stream → Event
pipeline — declarative sequences pulled into timed events that spawn synths
on a `TempoClock`, every event quantized to the grid and stamped with an OSC
timetag so it lands on the exact sample, not the block boundary
(`SCClassLibrary/Common/Streams`, `Collections/Event.sc`, `Core/Clock.sc`).
Buses route anything to anything; buffers load asynchronously with
completion messages and stream from disk (`DiskIn`/`DiskOut`); `Recorder`
captures the live output; `/status` reports average and peak CPU;
`Stethoscope`/`ServerMeter`/`Poll` watch signals. JITLib's `NodeProxy` is
the closest prior art to the reconciler — per-proxy fade time, role-based
attachment (replace vs. filter vs. set) — but resets state where automata
preserves it.

**Csound** (`reference/csound`) — the event-and-instance model at its most
complete. An `i` event instantiates an instrument from a per-definition
instance pool, unbounded; instances get release tails (`xtratim`), tied
legato notes, per-instrument allocation caps and a CPU budget governor
(`Engine/insert.c`). Runtime recompiles (`compileOrc`) retire old
definitions to a deadpool only when their last instance ends — a different
answer to automata's state-preservation problem. Function tables are
number-keyed and global, so buffer identity survives recompiles
(`Engine/fgens.c`). The `fsig` type makes streaming FFT frames a first-class
signal rate (`include/pstream.h`). Ships built-in peak metering with
out-of-range counts, and an optional output limiter (`--limiter`,
`InOut/libsnd.c`). Full MIDI subsystem with note→instance routing.

**JSyn** (`reference/jsyn`) — the cleanest small answer to "how do live
edits and note events stay glitch-free": one time-stamped command queue.
Every mutation — param set, connect, start/stop, note-on, data queue — is a
`ScheduledCommand` executed *by the audio thread* at a block boundary
(`com/softsynth/shared/time/ScheduledQueue.java`,
`engine/SynthesisEngine.java`). `VoiceAllocator` is a fixed voice pool keyed
by integer tag (e.g. MIDI note): reuse the tag's voice, else steal —
free first, then oldest-released, then oldest-playing — with a hook to
reconfigure a stolen voice before it sounds (`util/VoiceAllocator.java`).
Idle voices auto-disable and cost nothing. Sample data is queued to reader
units with loop points and start/loop/finish callbacks
(`ports/UnitDataQueuePort.java`). `WaveRecorder` captures to WAV;
`LoadAnalyzer` reports CPU usage.

**klang** (`reference/klang`) — the C++-embedded voice architecture, the
most directly reusable design for automata. A `Synth` owns a pre-allocated
pool of `Note` objects; each note is a state machine
(`Onset/Sustain/Release/Off`) with `on()`/`off()`/`process()` overrides and
per-voice state as plain members; `Notes::assign()` steals
free-then-released-then-oldest (`klang.h:4220-4467`). Controls are declared
in-code as typed, self-describing objects (dial/slider/menu/meter, range,
smoothing) that auto-surface to a host UI, with factory presets
(`klang.h:1655-1981`). MIDI bytes decode straight into the voice allocator.
Debug layer: signal taps into a rolling scope buffer (`x >> debug`), a
thread-safe console, and inline function plotting. Templates compile a patch
into a shippable VST3/AU plugin.

**Elementary** (`reference/elementary`) — automata's nearest architectural
relative (hash-identity reconciliation), and proof of two designs automata
lacks. First, the engine→app event relay: any node can queue readouts from
the audio thread; the host drains them on a timer into typed events —
`meter`, `scope`, `snapshot`, `fft`, `capture` — each tagged with the node's
name for routing (`runtime/elem/Runtime.h`, `builtins/Analyzers.h`). Second,
the virtual file system: an insert-only map of name → refcounted audio
buffer, so sample nodes reference buffers by string key, identity survives
re-renders, and nothing is freed under the render thread
(`runtime/elem/SharedResource.h`). Also: two-tier parameter control (refs
that bypass reconciliation entirely, plus sample-accurate time-stamped param
events), a seekable `sampleTime`/`beatTime`/`bpm` transport shared by a
sequencer node family, MIDI events in the block stream with an MPE-style LRU
voice allocator (`builtins/MIDI.h`), and `el.capture` recording a gated
region and emitting it back to the app.

**sapf** (`reference/sapf`) — the outlier: audio, control, and events are
one lazy, memoized, possibly infinite list type, so patterns are just lists
and forking a signal is free. Its transferable ideas: APL-style automatic
multichannel expansion — pass `[300 301]` where a scalar is expected and the
patch becomes stereo, no channel-aware code (`src/MultichannelExpansion.cpp`);
polyphony as `ola` — overlap-add instantiation of a sound-function on a
schedule, wrapped into SC2-style overlap/crossfade textures
(`src/UGen.cpp`); tempo as a signal that envelopes integrate over; and
per-thread seeded RNG where every random UGen forks its seed from the
thread's, making a whole piece reproducible from one seed
(`include/rgen.hpp`).

## The gaps, ranked

Ranked for a solo live-coding instrument. Convergent designs — where
several systems independently landed on the same shape — are called out,
since they are the strongest signal of what automata's version should look
like.

### Tier 1 — capability gaps: things automata cannot express at all

1. **Polyphonic voices with lifecycle.** Every surveyed system has an
   answer; automata has none. The designs converge hard: a *fixed
   pre-allocated pool* of structurally identical voices (klang, JSyn,
   Elementary's allocator — not Csound/SC's unbounded spawning, which needs
   a dynamic graph), *tag-keyed allocation* with free → released → oldest
   stealing (JSyn, klang, Elementary independently), a per-voice *state
   machine* (klang's `Onset/Sustain/Release/Off`), and *auto-release of
   silent voices* so idle voices cost nothing (SC `doneAction`, JSyn
   auto-disable, Csound release tails). A fixed pool fits automata's
   fixed-topology, state-preserving model exactly: N identical subgraphs
   are N-way structural identity, and the pool size is just a def constant.

2. **An event/sequencing layer with musical time.** Step-sequencer ugens
   are not a score. SC's Pattern→Event pipeline, Csound's score plus
   `schedule` opcodes, Elementary's sparse sequences on a shared transport,
   sapf's patterns-as-lists — all give the performer "play these
   parameterized events on the grid," with lookahead and quantization.
   The timing substrate matters as much as the pattern surface: SC stamps
   events to the exact sample via timetags, JSyn timestamps every command,
   Elementary schedules param events at sample offsets. automata quantizes
   patch *swaps* to the beat but cannot schedule an *event* at all. Depends
   on voices (an event needs something to trigger).

3. **Buffers and sample playback.** Absent entirely, and it is engine
   infrastructure, not vocabulary: loading must happen off the audio thread
   (SC's async completion messages, Csound's deferred GEN01), and buffer
   *identity must survive edits* so a re-render doesn't reload or free
   anything the render thread holds. Elementary's insert-only,
   refcounted, name-keyed resource map is the design closest to automata's
   constraints; Csound's number-keyed global tables prove the
   identity-outlives-the-graph principle. Streaming from disk (SC
   `DiskIn`/`VDiskIn`) can wait; in-memory buffers cannot, if samples are
   ever to matter here.

4. **An engine→host observability channel.** automata's control flow is
   strictly inbound; the performer flies blind. Every mature system pushes
   data back out: Elementary's per-node RT-safe event queues drained into
   typed meter/scope/snapshot/fft events; SC's `SendReply`/`Poll`, CPU
   average/peak in `/status`, `ServerMeter`; Csound's automatic peak
   metering; klang's `>> debug` signal taps; JSyn's scope probes and
   `LoadAnalyzer`. The automata shape is clear: a bounded SPSC queue from
   render to host (the mirror of the existing ControlBus), probe nodes that
   post into it, and the host printing meters and DSP load (render time vs.
   block budget — newly urgent now that feedback islands buy correctness
   with CPU). Elementary's name-tagged event routing is the model.

5. **MIDI input.** OSC and mouse only, today. Every other system treats
   MIDI as table stakes; JSyn shows the minimal shape (a byte parser
   feeding typed callbacks — device I/O can stay in the host), klang shows
   it feeding a voice allocator directly, Elementary shows time-stamped
   MIDI events resolved at sample offsets within the block. CC → ControlBus
   channels is nearly free and useful alone; note events join the voice
   work.

### Tier 2 — robustness and reach

6. **Output safety.** Csound ships `--limiter` and 0dbfs clipping with
   out-of-range reporting; SC ships a master `Volume` fader and `Limiter`
   as convention. Notably, Elementary and JSyn *also* lack a true safety
   stage — this gap is common, not embarrassing, but a one-sample feedback
   loop with a gain typo now reaches full scale inside a single block, so
   automata's need is sharper than most: a host-side master stage with
   NaN/inf scrubbing, a soft limiter, and auto-mute on sustained garbage.

7. **Live capture.** Offline render exists; recording what the audience
   heard does not. Three proven shapes: SC's `DiskOut` tail synth, JSyn's
   `WaveRecorder` background writer thread, Elementary's `el.capture`
   (record a gated region, emit it back to the app — the most interesting
   for a live sampler workflow). A ring buffer draining to a writer thread
   fits automata's threading rules as-is.

8. **Multichannel beyond stereo.** Fixed stereo out, no channel
   abstraction. SC's language-level multichannel expansion (an array
   argument fans the whole expression out) and sapf's auto-mapping do this
   with zero per-channel code; both are surface-level designs that would
   sit naturally on automata's typed patch DSL. Spatialization (`PanAz`,
   ambisonics) only matters after the channel model exists.

9. **Declared, discoverable parameters.** `param("fm", 0.9f)` gives a
   name and a fallback, nothing else. klang's typed control declarations
   (range, curve, smoothing, UI kind, presets) and SC's SynthDesc metadata
   make patches *introspectable* — a host UI, a MIDI map, a preset system
   all hang off it. (Param clamping was previously declined as engine
   policy; declaration is a different thing — metadata, not enforcement.)

### Tier 3 — noted, not urged

- **Spectral framework.** Csound's `fsig` — FFT frames as a typed signal
  rate — is the gold standard; SC and Elementary route spectra through
  buffers/events. A large design; only worth opening with a concrete
  musical need.
- **Tempo as a signal / seekable transport.** sapf integrates a tempo
  signal into beat time; Elementary's transport seeks and scrubs. automata's
  transport is host-set BPM. Fine for now; revisit if patterns land.
- **Reproducible randomness.** sapf's per-thread seed forking makes a
  whole piece deterministic from one seed — cheap to adopt whenever random
  ugens appear, and it composes with offline-render parity.
- **Plugin export.** klang compiles a patch into a VST3/AU. A different
  product; noted only because the patch-as-C++ design keeps the door open.

### Deliberate differences, not gaps

- **Dynamic topology** (SC's node tree, JSyn's runtime patching): automata's
  fixed topology *is* the design — it is what makes state-preserving swap,
  structural identity, and RT-safety-by-construction tractable (ADR 0001/
  0002). Voice pools recover the musical capability without giving that up.
- **Client/server split and wire protocols** (SC, Elementary's instruction
  stream): automata is deliberately in-process; the DLL boundary plays the
  role of the wire.
- **Multicore render** (supernova's parallel groups): previously declined;
  a solo instrument saturates one core long after these gaps are closed.
- **A patch language** (sapf, Csound orchestra, SC lang): patches are C++,
  full stop, per the brief. The survey's language-side ideas (multichannel
  expansion, patterns) port as library surface, not as a DSL.

## Capability matrix

| Capability            | SC  | Csound | JSyn | klang | sapf | Elementary | automata |
| --------------------- | --- | ------ | ---- | ----- | ---- | ---------- | -------- |
| Polyphonic voices     | ✓   | ✓      | ✓    | ✓     | ✓    | partial    | —        |
| Event scheduling      | ✓   | ✓      | ✓    | —     | ✓    | partial    | —        |
| Buffers/samples       | ✓   | ✓      | ✓    | ✓     | ✓    | ✓          | —        |
| Engine→host telemetry | ✓   | ✓      | ✓    | ✓     | —    | ✓          | —        |
| MIDI in               | ✓   | ✓      | ✓    | ✓     | ✓    | ✓          | —        |
| Output safety stage   | ✓   | ✓      | —    | —     | —    | —          | —        |
| Live capture          | ✓   | ✓      | ✓    | —     | ✓    | ✓          | —        |
| Multichannel          | ✓   | ✓      | ✓    | —     | ✓    | partial    | —        |
| Sub-block event time  | ✓   | ✓      | ✓    | —     | —    | ✓          | —        |
| State-preserving edit | —   | partial| —    | —     | —    | ✓          | ✓        |
| Sample-tight feedback | —   | —      | —    | ✓     | —    | —          | ✓        |
| Offline = live path   | ✓   | ✓      | ✓    | —     | —    | ✓          | ✓        |

(klang's "sample-tight feedback" is statement-order recursion inside one
`process()`, not a scheduled graph; Elementary's "partial" voices are its
MIDI allocator node; its "partial" events are sequence nodes without a
pattern layer.)

## Reading of the whole

The vocabulary can grow indefinitely, but these six systems agree on what an
*instrument* needs around the graph: something to play (voices), a way to
tell it when (events on musical time), material beyond synthesis (buffers),
eyes on the signal (telemetry), hands on the controls (MIDI), and a floor
under mistakes (safety). automata's distinctive strengths — state-preserving
edits, sample-tight feedback, offline parity — are exactly the ones the
others lack, so closing the shared-baseline gaps does not mean converging on
someone else's architecture. The natural first move is the voice pool: it is
the prerequisite for events and MIDI notes, the designs across klang, JSyn,
and Elementary converge on a shape that fits the fixed-topology model, and
it unlocks the largest musical territory per line of engine code.
