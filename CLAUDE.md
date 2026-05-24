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

## Code conventions

- `constexpr` whenever possible
- No single-letter variable names except conventional ones (`x`, `y`, `z`, `i`); do not abbreviate (e.g. no `rhy` for rhythm)
- No trailing underscores on private member variables
- All new DSP/signal code returns `Stream` and composes functionally — avoid stateful objects except where a delay register is semantically required

### `mutable` on lambdas

Only add `mutable` to a lambda when a captured-by-value variable is directly written to (e.g. a phase accumulator, a filter state, an LCG seed). Do **not** add `mutable` just to call `Stream::next()` — it is `const`. Lambdas that are pure mappings over streams (arithmetic operators, waveshapers, converters) should have no `mutable`.

### `constexpr` limits

`std::pow` and `std::log` are not `constexpr` until C++26, so `note_to_frequency` and `frequency_to_note` cannot be `constexpr` yet. Functions that construct `Stream` (which uses `shared_ptr` + `std::move_only_function`) cannot be `constexpr`.
