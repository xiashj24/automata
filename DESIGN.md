# Design Sketch: Node / Generator / Signal Type System

## Context

The current `Stream` design uses `shared_ptr<State>` where `State::gen` is a `std::move_only_function<float()>`. This gives uniform composition but hides all node types inside lambdas — nothing is introspectable or named. The goal here is to sketch a concrete-type + virtual-dispatch alternative, where every DSP node is a named class, operator overloading on a handle type (`Signal`) preserves composability, and `Generator` marks pure sources while everything else inherits `Node` directly.

### Relationship to fundsp and pdsp

| | fundsp static (`An<Pipe<X,Y>>`) | fundsp dynamic (`Net`) | pdsp (`AudioNode<Chain<A,B>>`) | This design (`Signal`) |
|---|---|---|---|---|
| Node identity | Encoded in type | `Box<dyn AudioUnit>` | Encoded in type (Copy) | `shared_ptr<Node>` |
| Connections | Operator → new type | Runtime `connect()` | Operator → new type | Pull via captured `Signal` |
| Fan-out | Recomputes each branch | Recomputes each branch | Explicit `split()` operator | `cache_tick` dedup (Eurorack) |
| Hot-swap | Impossible | ✓ | Impossible | ✓ (same as current `live_graph`) |
| Introspection | None | Runtime edges | None | `dynamic_cast` |

**Our `Signal` is fundsp's `Net` model** — the right choice for a live-coding DSL where graphs are built and swapped at runtime. The key deliberate difference: unlike all three reference models, `cache_tick` means a `Signal` shared to two inlets is computed once per tick — which is the correct Eurorack semantics (one cable, one signal).

**pdsp** independently arrived at the same `Generator` (no input) / `Processor` (has input) split — validating that `Generator` as a named semantic marker for pure sources is a sound distinction.

---

## Core Design

### Inheritance hierarchy

```
Node       — abstract base: cache mechanism, reset(), process()
Generator  — pure sources: no primary audio input (osc, noise, constant)

Sink       — consumer, not a Node
Signal     — handle wrapping shared_ptr<Node>
```

Concrete DSP classes (filters, envelopes, waveshapers) inherit `Node` directly. `Generator` marks the source role; everything else is just a named `Node` subclass.

---

### `Node` — abstract base, owns the cache mechanism

```cpp
// include/node.hpp
class Node {
public:
    virtual ~Node() = default;

    // Non-virtual NVI: handles cache dedup, then delegates to process().
    float next() {
        if (current_tick == 0) return process();
        if (cache_tick == current_tick) return cache_val;
        cache_val = process();
        cache_tick = current_tick;
        return cache_val;
    }

    // Reset all internal state (phases, filter memory, LCG seeds).
    // Called by Sink on hot-swap and before first render.
    virtual void reset() {}

    static thread_local inline uint64_t current_tick;

protected:
    virtual float process() = 0;

private:
    float cache_val = 0.f;
    uint64_t cache_tick = ~0ull;
};

// Pure source — no required audio input; parameters are Signals (freq, index, etc.)
class Generator : public Node {};
```

Arithmetic operators produce named `Node` subclasses — one per operation, no shared base:

```cpp
class MulNode : public Node {
public:
    MulNode(Signal a, Signal b) : a(a), b(b) {}
    float process() override { return a.next() * b.next(); }
private:
    Signal a, b;
};

class AddNode : public Node {
public:
    AddNode(Signal a, Signal b) : a(a), b(b) {}
    float process() override { return a.next() + b.next(); }
private:
    Signal a, b;
};
// SubNode, DivNode follow the same pattern
```

### `Signal` — the user-facing handle (shared_ptr wrapper)

Operator overloads live here, not on `Node`, so expressions like `freq * fm_index` return `Signal` rather than raw pointers. Copying a `Signal` is O(1) — no `std::move` needed.

`cache_tick` in `Node::next()` guarantees each upstream node evaluates at most once per tick, so `a.next() * b.next()` is safe even when `a` fans out to multiple consumers.

```cpp
class Signal {
public:
    Signal() = default;
    Signal(float constant);                        // wraps a ConstantGenerator
    Signal(std::shared_ptr<Node> n) : impl(std::move(n)) {}

    float next() const { return impl->next(); }
    void reset() const { if (impl) impl->reset(); }
    explicit operator bool() const { return impl != nullptr; }

    Signal operator+(Signal other) const;
    Signal operator-(Signal other) const;
    Signal operator*(Signal other) const;
    Signal operator/(Signal other) const;

private:
    std::shared_ptr<Node> impl;
};

Signal Signal::operator*(Signal other) const { return std::make_shared<MulNode>(*this, other); }
Signal Signal::operator+(Signal other) const { return std::make_shared<AddNode>(*this, other); }
// ... same pattern for -, /
```

---

### Filter — concrete `Node` example

```cpp
class OnePoleLP : public Node {
public:
    OnePoleLP(Signal input, Signal cutoff_hz)
        : input(input), cutoff(cutoff_hz) {}

protected:
    float process() override {
        float a = 1.f - std::min(1.f, 2.f * std::numbers::pi_v<float>
                                       * cutoff.next() / SampleRate);
        z = a * z + (1.f - a) * input.next();
        return z;
    }
private:
    Signal input, cutoff;
    float z = 0.f;
};

Signal one_pole(Signal input, Signal cutoff_hz) {
    return std::make_shared<OnePoleLP>(input, cutoff_hz);
}
```

---

### `Sink` — consumes a signal, not a Node itself

```cpp
class Sink {
public:
    explicit Sink(Signal source) : source(source) {}
    virtual ~Sink() = default;

    void reset() { source.reset(); }

    void render(std::span<float> buf) {
        for (float& s : buf) {
            ++Node::current_tick;
            s = source.next();
        }
    }

protected:
    Signal source;
};
```

`Signal::reset()` forwards into the graph recursively — each node's `reset()` override calls it on its own captured `Signal` inputs. A concrete audio output sink would inherit from `Sink` and drive a device callback.

---

### `SineOscillator` — concrete Generator

```cpp
class SineOscillator : public Generator {
public:
    explicit SineOscillator(Signal freq, Signal phase_mod = {})
        : freq(freq), phase_mod(phase_mod) {}

    void reset() override {
        // Deterministic per-node initial phase so two osc(440) nodes
        // in the same patch start at different phases (fundsp pattern).
        phase = static_cast<float>(node_hash & 0xFFFF) / 65536.f;
        freq.reset();
        if (phase_mod) phase_mod.reset();
    }

protected:
    float process() override {
        float pm = phase_mod ? phase_mod.next() : 0.f;
        phase += freq.next() / SampleRate + pm;
        if (phase > 1.f) phase -= 1.f;
        return std::sin(phase * 2.f * std::numbers::pi_v<float>);
    }

private:
    Signal freq;
    Signal phase_mod;
    float phase = 0.f;
    uint64_t node_hash = next_node_id();   // global counter, one per construction
};

Signal osc(Signal freq)                { return std::make_shared<SineOscillator>(freq); }
Signal osc(Signal freq, Signal phase_mod) { return std::make_shared<SineOscillator>(freq, phase_mod); }
```

---

### FM patch — identical call-site to the current version

```cpp
Signal simple_fm(Signal freq, Signal fm_index) {
    auto modulator = osc(freq * fm_index);   // freq * fm_index -> MulNode (Signal)
    return osc(freq, modulator);             // SineOscillator with phase mod (Signal)
}
```

---

## Type role summary

| Class | Role | fundsp analogue | Inherits | Has input? |
|-------|------|----------------|----------|------------|
| `Node` | Abstract base: cache + reset | `AudioUnit` (dyn-erased) | — | — |
| `Generator` | Pure source (semantic marker) | `Sine<F>`, `Noise` | `Node` | params only (freq, index…) |
| `Signal` | Handle / value type | `Net` node reference | — | — |
| `SineOscillator` | Concrete source | `Sine<F>` | `Generator` | freq + opt. phase mod |
| `OnePoleLP` | Concrete filter | `OnePole<F>` | `Node` | input + cutoff |
| `MulNode`, `AddNode` … | Arithmetic ops | `Binop<FrameMul,X,Y>` | `Node` | two `Signal` inputs |
| `Sink` | Consumer, not a Node | `AudioUnit` sink | — | one `Signal` source |

### `ConstantGenerator`

The concrete `Generator` behind `Signal(float)` (pdsp calls this `Parameter`):

```cpp
class ConstantGenerator : public Generator {
public:
    explicit ConstantGenerator(float v) : value(v) {}
    void set(float v) { value.store(v, std::memory_order_relaxed); }
protected:
    float process() override { return value.load(std::memory_order_relaxed); }
private:
    std::atomic<float> value;
};
```

Bridge to the existing `Param` system: a `Param` wraps a `ConstantGenerator` whose `set()` is safe to call from the control thread.

### Not in scope (yet)

- Multi-channel output — mono-first; stereo uses two separate `Signal` graphs
- Compile-time arity checking — fundsp/pdsp have it; ours is runtime
- Signal analysis (frequency response, latency tracking)
- Separate audio/event/control buses — not needed until MIDI/OSC

---

## Required features

### 1. Fan-out deduplication

When the same `Signal` is passed to two consumers, they share the same `shared_ptr<Node>`. The first to call `next()` in a tick fills `cache_val`; the second gets the cached result — identical to the current `Stream::next()` mechanism, moved into `Node::next()`.

```cpp
Signal freq = constant(440.f);
Signal voice = osc(freq) + osc(freq * 2.f);
// freq's Node::process() runs exactly once per tick.
// The osc(freq*2) MulNode calls freq.next() -> cache hit.
```

### 2. Intuitive composition syntax

Operator overloads on `Signal` plus implicit `Signal(float)` construction — no raw pointers or explicit `connect()` calls.

```cpp
Signal patch() {
    return (osc(440.f) + osc(880.f) * 0.5f) * adsr(gate);
}
```

### 3. Stateful stochastic nodes

State is member variables on the concrete subclass — named, introspectable, no lambda capture.

```cpp
class WhiteNoise : public Generator {
protected:
    float process() override {
        seed = seed * 6364136223846793005ULL + 1442695040888963407ULL;
        return static_cast<float>(seed >> 33) / static_cast<float>(1u << 31) - 1.f;
    }
private:
    uint64_t seed = 12345;
};

class RandChoose : public Generator {
public:
    RandChoose(Signal trigger, std::vector<float> values)
        : trigger(trigger), values(std::move(values)) {}
protected:
    float process() override {
        float t = trigger.next();
        if (t > 0.f && !prev_high) idx = rng() % values.size();
        prev_high = t > 0.f;
        return values[idx];
    }
private:
    Signal trigger;
    std::vector<float> values;
    std::size_t idx = 0;
    bool prev_high = false;
    std::mt19937 rng{std::random_device{}()};
};
```

### 4. Every outlet connects to any inlet (Eurorack)

`Signal` is the only signal type — no `AudioSignal`, `CVSignal`, or `TriggerSignal`. Every inlet on every node is typed `Signal`; every outlet is `Signal`. An LFO, oscillator, envelope, or trigger are all the same type and patch freely into any inlet.

```cpp
Signal lfo     = osc(Signal(0.5f)) * 50.f + 440.f;
Signal voice   = osc(lfo);                             // LFO into VCO pitch

Signal mod     = osc(Signal(3.f)) * 500.f + 1500.f;
Signal out     = one_pole(osc(Signal(220.f)), mod);    // VCO into filter cutoff

Signal trigger = clock(120.f);
Signal melody  = osc(rand_choose(trigger, {220.f, 330.f, 440.f, 550.f}));
```

---

## Trade-offs vs current Stream design

| Aspect | Stream (current) | Node/Signal (new) |
|--------|-----------------|-------------------|
| Node identity | Anonymous lambdas | Named concrete classes |
| Dispatch | `move_only_function` indirect call | Virtual call (vtable) |
| Introspection | None | `dynamic_cast`, type name |
| Role distinction | No distinction | `Generator` marks sources; rest inherit `Node` directly |
| Cache tick | `State` member | `Node` member (same logic) |
| Composition | Operator overloads on `Stream` | Operator overloads on `Signal` |
| Allocation | One `shared_ptr<State>` per op | One `shared_ptr<Node>` per node |

---

## Files to create / modify

- `include/node.hpp` — `Node`, `Generator`, `Signal`, `Sink`, `ConstantGenerator`, `MulNode`, `AddNode`, `SubNode`, `DivNode`
- `include/osc.hpp` — replace stub `OSC` class with `SineOscillator : Generator`
- `src/graph.cpp` — update patches to use `Signal` instead of `Stream`
