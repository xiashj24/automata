# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Build and test

```bash
cmake --preset develop
cmake --build build/develop
ctest --test-dir build/develop --output-on-failure
```

Run a single test by name:
```bash
ctest --test-dir build/develop -R <test-name> --output-on-failure
```

Other useful presets: `ninja-multi-asan` (AddressSanitizer + UBSan), `ninja-multi-tsan` (ThreadSanitizer).

## Architecture

**Automata** is a header-only C++23 generative music library (`include/`) with two executable targets built in `src/`. The core is stream-based: every audio signal, trigger, and modulation is a `Stream`.

### Stream (`include/stream.hpp`)

`Stream` is the central type — a ref-counted, lazy `float()` generator. Key properties:
- Copies share the same generator (`shared_ptr<State>`); copying is O(1)
- Within a single sample tick, `next()` evaluates at most once (cached by `current_tick`), so a stream can be fanned out to multiple consumers without double-evaluation
- `render(span<float>) const` increments `current_tick` and fills a buffer — this is the audio callback entry point
- Both `next()` and `render()` are `const` — mutation goes through `shared_ptr<State>`, not the `Stream` object itself
- `current_tick == 0` means outside a render context: `next()` always recomputes and never writes to the cache, so stateful streams (phasors, LCGs) advance correctly when called directly in tests
- `State::gen` is `std::move_only_function<float()>` — do not regress to `std::function`

Oscillators and modulation functions (`osc`, `saw`, `sqr`, `sin`, `phasor`, `noise`, `lin_exp`) return `Stream` and compose with `+`, `-`, `*`, `/`.

### Timing (`include/clock.hpp`)

`Clock(bpm, rate)` where `rate` is subdivisions per whole note (4=quarter, 8=eighth, 16=sixteenth):
- `clock.ramp()` → 0→1 phasor per step
- `clock.trigger()` → 1.f for one sample at step boundary, 0.f otherwise
- `clock * 2` / `clock / 2` → clock multiplication/division
- `metro(bpm)` → shorthand quarter-note trigger

### Pattern and rhythm (`include/pattern.hpp`, `include/euclid.hpp`)

- `seq(trigger, {v1, v2, ...})` → advances index on trigger, cycles through values
- `choose`, `xchoose`, `wchoose` → random selection
- `euclid(hits, steps)` → stateless Euclidean rhythm as `vector<bool>`

### Graph and parameters (`include/graph.hpp`)

`Graph` is a named-port runtime container for a signal chain. It owns named output slots and controllable parameters, and is the unit of live swap. The DAG itself remains implicit — evaluation order and fan-out deduplication are handled by `Stream`'s pull-recursion and `cache_tick` mechanism. `Graph` adds named ports and hot-swap support on top of that, not explicit node/edge tracking.

```cpp
Graph g;
auto& cutoff = g.make_param("cutoff", 2000.f, 20.f, 20000.f);
g.add_output("main", svf_lp(saw(440_hz), cutoff, 0.3f) * 0.3f);
g.render(buf);       // sums all outputs into buf
g.render_multi(bufs); // one span per output, no summing
```

**`Param`** — a named, live-controllable `float` backed by `shared_ptr<atomic<float>>`. Implicitly converts to `Stream` so it is a drop-in wherever a `Stream` parameter is accepted. `set()` is safe to call from any thread; the audio thread reads with `memory_order_relaxed` (one-sample-late updates are acceptable).

**`Graph::lkg(i)`** — returns the last sample rendered by output `i`. Intended for two future uses:
1. **Live hot-reload crossfade**: when `live_graph` is atomically replaced, the incoming graph's first frame can be blended with `old_graph->lkg(i)` to avoid a single-buffer dropout.
2. **UI metering**: a control panel can poll `lkg(i)` to display per-channel output levels without touching the audio thread (acceptable tearing for a meter).

**Source layout**: `src/main.cpp` owns the audio device and graph runtime (`build_graph`, `live_graph`, `render`). `src/graph.cpp` contains only user signal-chain code — the single function `define_graph(Graph& g)`. Edit `graph.cpp` to change what plays.

Patch functions in `graph.cpp` return `Stream` and can be mixed freely in `define_graph`:

```cpp
void define_graph(Graph& g) {
  g.add_output("main", patch_basics() * 0.5f + patch_euclid() * 0.5f);
}
```

Live graph swap from a REPL/DSL thread: `live_graph.store(build_graph(), memory_order_release)`. Graph swap resets all lambda-captured state (filter memory, oscillator phases). `lkg` bridges the single-buffer gap (~5 µs at 48 kHz). Full state continuity (named state slots surviving a swap) is deferred until the DSL interpreter is begun.

### DSP modules

| Header | Contents |
|--------|----------|
| `filter.hpp` | `biquad`, state-variable filter (LP/BP/HP), `lag`, `slew`, `one_pole` |
| `envelope.hpp` | ASR envelope, `env_follow` (peak follower) |
| `stream.hpp` | Oscillators, arithmetic ops, `toggle`, `count` |
| `delay.hpp` | Single-sample feedback register (enables recursive/chaotic patches) |
| `note_number.hpp` | MIDI note ↔ Hz conversion |
| `units.hpp` | Literals: `440_hz`, `120_bpm`, `0.5_s` |

Additional UGens ported from uSEQ (adapted from control-rate to fixed 48 kHz):

| Function | Location | Description |
|----------|----------|-------------|
| `one_pole(x, cutoff_hz)` | `filter.hpp` | One-pole low-pass; wrapper around `lag` with Hz→coefficient conversion: `a = 1 - min(1, 2π·fc/SR)` |
| `env_follow(x, attack_hz, release_hz)` | `envelope.hpp` | Peak follower — tracks envelope of `x` with asymmetric attack/release rates in Hz |
| `toggle(trigger)` | `stream.hpp` | Flip-flop: output alternates 0↔1 on each rising edge of `trigger` |
| `count(trigger, reset)` | `stream.hpp` | Rising-edge counter; resets to 0 on rising edge of `reset` |

### Executables

**`automata-audio`** (`src/main.cpp` + `src/graph.cpp`): Real-time audio via miniaudio at 48 kHz, 256-frame callback. `graph.cpp` defines the audio graph — edit `define_graph` to change what plays.

### Reference implementations (`external/`)

When adding new features, study the submodules in `external/` for prior art and design inspiration:

- **`sapf/`** — Sound As Pure Form: functional audio DSP in a Forth-like language; reference for signal-flow primitives
- **`cane/`** — minimal algorithmic rhythm sequencer; reference for rhythm DSL design
- **`isobar/`** — Python music pattern library; reference for pattern and sequencing abstractions
- **`signalflow/`** — Python/C++ signal processing framework; reference for graph-based DSP
- **`strudel/`** — browser-based live coding (TidalCycles port); reference for pattern syntax and mini-notation
- **`uSEQ/`** — FRP-based live-coding Eurorack sequencer (RP2040, ModuLisp DSL); reference for explicit node-DAG compilation (CSE, constant folding, topological sort), declared cross-sample state (`defstate`/`integrate`/UGens), zero-allocation flat-array hot path, and live recompilation with last-known-good fallback.

### Tests

Uses Catch2 v3. Tests live in `test/` and favour `STATIC_REQUIRE` (constexpr checks). The `develop` preset enables cppcheck and cpplint.

| File | Covers |
|------|--------|
| `math.test.cpp` | Constexpr math utilities |
| `param.test.cpp` | `Param` construction, `set`/`get`, implicit `Stream` conversion, copy sharing |
| `graph.test.cpp` | `Graph` output/param counts, constant-output render, multi-output summing, `lkg`, `render_multi` |

## Project direction

### DSL backend

Automata is intended to become the backend of a generative music DSL. The DSL will be implemented as a **tree-walking interpreter** — not a full compiler. Evaluating a DSL expression produces `Stream` objects directly; no code generation step is needed. The C++ API in `graph.cpp` already reads like a DSL; the interpreter maps AST nodes onto the same factory calls.

### CV output

CV signals are modelled as additional `Stream` outputs alongside audio — same type, different sink. Microcontroller CV output is not a current target; the DSP backend runs on the host PC only.

### Live coding

The `Graph`/`Param` layer is implemented and ready. `live_graph` in `main.cpp` is `std::atomic<std::shared_ptr<Graph>>`; the audio thread loads a snapshot per callback and the interpreter/REPL thread stores a freshly-built graph atomically. `Param::set()` allows sub-graph parameter control without a full rebuild. `Graph::lkg(i)` provides the last-rendered sample per output for dropout-free crossfading on swap.

### Design decisions (do not revisit without measurement)

- **Block processing** (fill N-sample buffers per node, SignalFlow-style): not worth the complexity at current graph sizes. Per-sample pull with `cache_tick` deduplication is sufficient.
- **Graph introspection** (explicit `inputs` list on `State`): YAGNI until the DSL interpreter needs it.

## Code conventions

- `constexpr` whenever possible
- No single-letter variable names except conventional ones (`x`, `y`, `z`, `i`); do not abbreviate (e.g. no `rhy` for rhythm)
- No trailing underscores on private member variables
- All new DSP/signal code returns `Stream` and composes functionally — avoid stateful objects except where a delay register is semantically required

### `mutable` on lambdas

Only add `mutable` to a lambda when a captured-by-value variable is directly written to (e.g. a phase accumulator, a filter state, an LCG seed). Do **not** add `mutable` just to call `Stream::next()` — it is `const`. Lambdas that are pure mappings over streams (arithmetic operators, waveshapers, converters) should have no `mutable`.

### `constexpr` limits

`std::pow` and `std::log` are not `constexpr` until C++26, so `note_to_frequency` and `frequency_to_note` cannot be `constexpr` yet. Functions that construct `Stream` (which uses `shared_ptr` + `std::move_only_function`) cannot be `constexpr`.

### Realtime safety rules

The audio callback must never allocate or free heap memory. Rules by category:

**Bounded delay lines** (`delay_n`, `allpass_delay`, `karplus_strong`): use `std::array<float, MaxDelaySamples>` (4096 samples, ~85 ms at 48 kHz, defined in `samplerate.hpp`) inside a plain `State` struct. Initialise with `st = State{}` directly in the lambda capture — no pre-fill step, no `std::move` needed.

```cpp
struct State { std::array<float, MaxDelaySamples> buf{}; std::size_t pos = 0; };
return Stream([x, sz, st = State{}]() mutable -> float { ... });
```

**Variable-length delay lines** (`comb_n`): accept a `max_delay_s` construction parameter, pre-allocate a `std::vector` to that size outside the lambda, then move it in. The callback clamps the runtime delay to `max_samples - 1` and never calls `resize`.

```cpp
std::vector<float> buf(max_samples, 0.f);
return Stream([..., buf = std::move(buf)]() mutable -> float { ... });
```

**Long analysis windows** (`rms`): window size can reach tens of thousands of samples — a fixed array is impractical. Keep `std::vector`, pre-fill it before the lambda, and move it in with `std::move`. No resize inside the lambda.

```cpp
State st; st.buf.assign(n, 0.f);
return Stream([x, n, st = std::move(st)]() mutable -> float { ... });
```

**No extra `shared_ptr` on State**: `Stream` already owns the callable via `shared_ptr<State>` internally. Wrapping `State` in a second `shared_ptr` (e.g. `make_shared<State>()`) adds double-indirection with no benefit — capture `State` by value. Exception: `Delay` (`delay.hpp`) intentionally uses `shared_ptr<float>` so two lambda captures in a recursive patch share the same one-sample register.
