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

### DSP modules

| Header | Contents |
|--------|----------|
| `filter.hpp` | `biquad`, state-variable filter (LP/BP/HP), `lag`, `slew` |
| `envelope.hpp` | ASR envelope — `attack`, `sustain`, `release` durations |
| `delay.hpp` | Single-sample feedback register (enables recursive/chaotic patches) |
| `note_number.hpp` | MIDI note ↔ Hz conversion |
| `units.hpp` | Literals: `440_hz`, `120_bpm`, `0.5_s` |

### Executables

**`automata-audio`** (`src/main.cpp` + `src/graph.cpp`): Real-time audio via miniaudio at 48 kHz, 256-frame callback. `graph.cpp` defines the audio graph — edit it to change what plays.

### Reference implementations (`external/`)

When adding new features, study the submodules in `external/` for prior art and design inspiration:

- **`sapf/`** — Sound As Pure Form: functional audio DSP in a Forth-like language; reference for signal-flow primitives
- **`cane/`** — minimal algorithmic rhythm sequencer; reference for rhythm DSL design
- **`isobar/`** — Python music pattern library; reference for pattern and sequencing abstractions
- **`signalflow/`** — Python/C++ signal processing framework; reference for graph-based DSP
- **`strudel/`** — browser-based live coding (TidalCycles port); reference for pattern syntax and mini-notation

### Tests

Uses Catch2 v3. Tests live in `test/` and favour `STATIC_REQUIRE` (constexpr checks). The `develop` preset enables cppcheck and cpplint.

## Project direction

### DSL backend

Automata is intended to become the backend of a generative music DSL. The DSL will be implemented as a **tree-walking interpreter** — not a full compiler. Evaluating a DSL expression produces `Stream` objects directly; no code generation step is needed. The C++ API in `graph.cpp` already reads like a DSL; the interpreter maps AST nodes onto the same factory calls.

### CV output

CV signals are modelled as additional `Stream` outputs alongside audio — same type, different sink. The PC sends CV as USB audio channels; the target device receives samples and clocks them to an external DAC.

Target hardware options, in order of preference:

| Device | USB Audio | MCU | Notes |
|---|---|---|---|
| **Teensy 4.1** | UAC 2.0 (HS) | i.MX RT1062, 600 MHz M7 | Easiest path — Teensy Audio Library has USB audio device out of the box |
| **Electrosmith Daisy Seed** | UAC 2.0 (HS) | STM32H750, 480 MHz M7 | Purpose-built audio platform; LibDaisy audio callback mirrors automata's render loop. Option to run light DSP locally in future. |
| **Raspberry Pi Pico 2 W** | UAC 1.0 (FS) | RP2350, 150 MHz M33 | Cheapest; 48 kHz 16-bit sufficient for control-rate CV, but no audio-rate modulation |

UAC 2.0 (requires USB High Speed) enables audio-rate CV (192 kHz update rate, 32-bit resolution) — useful for oscillator FM and waveshaping via CV. UAC 1.0 is limited to control-rate signals but is adequate for pitch, gates, and slow modulation.

Running the DSP backend on any of these MCUs is **not** the target: `shared_ptr`, `move_only_function`, `vector`, and `thread_local` all require hosted-environment rework. These devices are CV peripherals, not DSP engines.

### Live coding

The current architecture supports live graph swapping without structural changes. The root stream (`out`) can be wrapped in `std::atomic<std::shared_ptr<State>>` so the interpreter thread installs a new graph with one atomic store while the audio thread holds a snapshot for the duration of each render call. Controllable parameters are exposed as `std::atomic<float>` slots read by a stream closure (`memory_order_relaxed` is correct — one-sample-late updates are acceptable).

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
