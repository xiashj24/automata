# Design: Node / Signal Type System

## Context

The current `Stream` design uses `shared_ptr<State>` where `State::gen` is a
`std::move_only_function<float()>`. This gives uniform composition but hides all node types
inside lambdas — nothing is introspectable or named.

The goal is a **"shader language for audio"**: expressive and compositional at the authoring
level, while the underlying execution is a flat, cache-friendly, optimizable sequence of
operations. The analogy to GPU shaders is imprecise in one important way — shaders are
stateless, but audio DSP is inherently stateful (filter memory, oscillator phase). The
correct reference is **compute shaders**, which do have explicit state (read-write buffers)
and can express feedback (ping-pong passes). Cmajor (`external/cmajor`) shows this concretely:
processor state is declared as member variables; feedback is an explicit delay-annotated
connection in the graph topology. The "shader" property isn't statelessness — it's that
**state is declared and separated from computation logic**, and **feedback is explicit and
visible in the topology**.

### Relationship to reference designs

| | fundsp static | fundsp dynamic (`Net`) | pdsp | This design (`Signal`) |
|---|---|---|---|---|
| Node identity | Encoded in type | `Box<dyn AudioUnit>` | Encoded in type | `shared_ptr<Node>` |
| Connections | Operator → new type | Runtime `connect()` | Operator → new type | Pull via captured `Signal` |
| Fan-out | Recomputes each branch | Recomputes each branch | Explicit `split()` | `cache_tick` dedup (Eurorack) |
| Hot-swap | Impossible | ✓ | Impossible | ✓ |
| Feedback | No | No | No | `FeedbackRegister` |
| Introspection | None | Runtime edges | None | `dynamic_cast` |
| Execution | Inlined chain | Per-sample virtual | Per-sample virtual | Lowered flat list |

**Our `Signal` is fundsp's `Net` model** — the right choice for a live-coding DSL where
graphs are built and swapped at runtime. Key deliberate differences:

- `cache_tick` means a `Signal` shared to two inlets is computed once per tick — correct
  Eurorack semantics (one cable, one signal). None of the reference designs have this.
- `FeedbackRegister` makes feedback a first-class, topology-visible construct. The current
  `Stream` design hides feedback in a `shared_ptr<float>` exception in `delay.hpp`.
- A graph lowering pass converts the pull-DSP graph into a flat execution list before the
  audio callback — eliminating recursive virtual dispatch at render time.

**Not adopted from Cmajor:**
- Audio-rate / control-rate type distinction (`.ar`/`.kr`): the Eurorack and sapf model
  deliberately rejects this. Every outlet is `Signal`; every inlet accepts `Signal`.
- Static compilation (LLVM backend): requires a full language toolchain, out of scope.
- Generator/Processor split: justified in fundsp/pdsp because they encode connection arity
  in types at compile time. In a runtime-dispatch model, "no primary audio input" is
  ill-defined when every inlet — `freq`, `cutoff`, `phase_mod` — is already a `Signal`.

---

## Core Design

### Type hierarchy

```
Node       — abstract base: cache mechanism, reset(), process()
Signal     — handle wrapping shared_ptr<Node>; operators produce new Nodes
Graph      — render driver: owns named outputs, increments tick, sums into buffer
```

No `Generator` base class. No `Sink` class. Every concrete DSP unit inherits `Node` directly.
A node with no `Signal` member variables is a source by construction; no marker needed.
`Graph` plays the Sink role — it owns the named outputs and drives `render()`.

---

### `Node` — abstract base, owns the cache mechanism

```cpp
// include/node.hpp
class Node {
public:
    virtual ~Node() = default;

    // NVI: handles cache dedup per-tick, delegates to process().
    float next() {
        if (current_tick == 0) return process();
        if (cache_tick == current_tick) return cache_val;
        cache_val = process();
        cache_tick = current_tick;
        return cache_val;
    }

    // Reset all internal state (phases, filter memory, LCG seeds).
    // Each override calls reset() on its own captured Signal inputs.
    virtual void reset() {}

    static thread_local inline uint64_t current_tick;

protected:
    virtual float process() = 0;

private:
    float    cache_val  = 0.f;
    uint64_t cache_tick = ~0ull;
};
```

Arithmetic operators produce named `Node` subclasses:

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

---

### `Signal` — the user-facing handle

Operator overloads live here, not on `Node`, so expressions like `freq * fm_index` return
`Signal` rather than raw pointers. Copying a `Signal` is O(1).

```cpp
class Signal {
public:
    Signal() = default;
    Signal(float constant);                         // wraps a ConstantNode
    Signal(std::shared_ptr<Node> n) : impl(std::move(n)) {}

    float next() const { return impl->next(); }
    void  reset() const { if (impl) impl->reset(); }
    explicit operator bool() const { return impl != nullptr; }

    Signal operator+(Signal) const;
    Signal operator-(Signal) const;
    Signal operator*(Signal) const;
    Signal operator/(Signal) const;

private:
    std::shared_ptr<Node> impl;
};

Signal Signal::operator*(Signal other) const { return std::make_shared<MulNode>(*this, other); }
Signal Signal::operator+(Signal other) const { return std::make_shared<AddNode>(*this, other); }
```

---

### Concrete Node examples

**Source — `SineOscillator`:**

```cpp
class SineOscillator : public Node {
public:
    explicit SineOscillator(Signal freq, Signal phase_mod = {})
        : freq(freq), phase_mod(phase_mod) {}

    void reset() override {
        phase = static_cast<float>(node_id & 0xFFFF) / 65536.f; // deterministic per-node phase
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
    Signal freq, phase_mod;
    float    phase   = 0.f;
    uint64_t node_id = next_node_id();
};

Signal osc(Signal freq)                   { return std::make_shared<SineOscillator>(freq); }
Signal osc(Signal freq, Signal phase_mod) { return std::make_shared<SineOscillator>(freq, phase_mod); }
```

**Filter — `OnePoleLP`:**

```cpp
class OnePoleLP : public Node {
public:
    OnePoleLP(Signal input, Signal cutoff_hz) : input(input), cutoff(cutoff_hz) {}

protected:
    float process() override {
        float a = 1.f - std::min(1.f, 2.f * std::numbers::pi_v<float> * cutoff.next() / SampleRate);
        z = a * z + (1.f - a) * input.next();
        return z;
    }
private:
    Signal input, cutoff;
    float z = 0.f;
};

Signal one_pole(Signal input, Signal cutoff_hz) { return std::make_shared<OnePoleLP>(input, cutoff_hz); }
```

**Live parameter — `ConstantNode`** (the `Param` bridge):

```cpp
class ConstantNode : public Node {
public:
    explicit ConstantNode(float v) : value(v) {}
    void set(float v) { value.store(v, std::memory_order_relaxed); }
protected:
    float process() override { return value.load(std::memory_order_relaxed); }
private:
    std::atomic<float> value;
};
```

`Param` wraps a `ConstantNode` whose `set()` is safe to call from the control thread.

---

### `FeedbackRegister` — explicit single-sample delay for closing loops

The current `Stream` design hides feedback in a `shared_ptr<float>` register noted as an
exception in `delay.hpp`. `FeedbackRegister` makes it a named, first-class construct that
is visible to graph inspection and the lowering pass.

```cpp
// Two halves sharing one float register:
//   read-side  — a Signal that returns last tick's written value (starts at 0)
//   write-side — wired to a source Signal by the patch author

class FeedbackRegister {
public:
    FeedbackRegister();

    Signal  read()              const;  // returns a Signal backed by the register
    void    connect(Signal src);        // wires src to update the register after each tick
};
```

Usage — SC-style self-oscillating feedback loop (previously inexpressible):

```cpp
FeedbackRegister fb;
Signal filtered = bpf(fb.read() * 7.5f + saw(32.f), lfo_freq, 0.1f).distort();
Signal out      = comb(filtered, 2.f, 40.f);
fb.connect(out);
```

Usage — FM with phase feedback (equivalent to `sinoscfb` in sapf):

```cpp
FeedbackRegister fb;
Signal osc_out = osc(440.f, fb.read() * 0.5f);
fb.connect(osc_out);
```

The lowering pass treats `FeedbackRegister` as a **cut point** in the DAG: it breaks the
cycle so topological sort is possible, and schedules a register-write step after the node
connected via `connect()` executes each tick.

---

### Graph lowering pass

**Problem:** In the current pull-DSP model the audio callback calls `Node::next()` on the
output, which recurses through the graph following `shared_ptr` pointers with virtual dispatch
— N virtual calls and N pointer dereferences per sample.

**Solution:** After building the `Signal` graph at patch-define time (not in the audio
callback), run a one-time lowering pass:

1. Walk the graph from the output node, following `Signal` edges
2. Treat `FeedbackRegister` read-sides as leaves (cycle cuts)
3. Produce a topologically-sorted `std::vector<Node*>` — sources first, output last
4. The audio callback iterates this flat list linearly: no recursion, no pointer chasing,
   branch-predictor-friendly, cache-friendly

Virtual dispatch is still present per node, but the traversal overhead is eliminated.

```cpp
// Graph::lower() builds the execution list once after define_graph() returns.
// Graph::render() uses the list instead of recursive next().
std::vector<Node*> execution_order = lower(output_signal);
```

This is the same optimization Cmajor achieves at compile time (inlining graph connections);
here it's a runtime lowering step that still allows hot-swap (rebuild + re-lower on swap).

---

## Type role summary

| Class | Role | Inherits | Has Signal inputs? |
|-------|------|----------|--------------------|
| `Node` | Abstract base: cache + reset | — | — |
| `Signal` | Handle / value type (`shared_ptr<Node>`) | — | — |
| `SineOscillator` | Sine oscillator with optional phase mod | `Node` | `freq`, `phase_mod` |
| `OnePoleLP` | One-pole low-pass filter | `Node` | `input`, `cutoff` |
| `MulNode`, `AddNode`, … | Arithmetic ops | `Node` | two inputs |
| `ConstantNode` | Thread-safe live parameter | `Node` | none |
| `FeedbackRegister` | Explicit 1-sample loop-closer | not a Node | write: one Signal |
| `Graph` | Render driver + named outputs | — | one Signal per output |

---

## Required features

### 1. Fan-out deduplication

Shared `Signal` → shared `shared_ptr<Node>`. `cache_tick` in `Node::next()` ensures the
first consumer fills `cache_val`; subsequent consumers in the same tick get the cached value.

```cpp
Signal freq  = constant(440.f);
Signal voice = osc(freq) + osc(freq * 2.f);
// freq evaluates exactly once per tick even though two nodes call freq.next().
```

### 2. Intuitive composition syntax

Operator overloads on `Signal` plus implicit `Signal(float)` construction.

```cpp
Signal patch() {
    return (osc(440.f) + osc(880.f) * 0.5f) * adsr(gate);
}
```

### 3. Stateful stochastic nodes

State lives in named member variables on the concrete subclass — not in lambda captures.

```cpp
class WhiteNoise : public Node {
protected:
    float process() override {
        seed = seed * 6364136223846793005ULL + 1442695040888963407ULL;
        return static_cast<float>(seed >> 33) / static_cast<float>(1u << 31) - 1.f;
    }
private:
    uint64_t seed = 12345;
};
```

### 4. Every outlet connects to any inlet (Eurorack)

`Signal` is the only signal type. No `AudioSignal`, `CVSignal`, `TriggerSignal`. An LFO,
envelope, oscillator, or trigger are all `Signal` and patch freely into any inlet.

```cpp
Signal lfo     = osc(0.5f) * 50.f + 440.f;
Signal voice   = osc(lfo);                               // LFO → VCO pitch
Signal trigger = clock(120.f);
Signal melody  = osc(rand_choose(trigger, {220.f, 330.f, 440.f, 550.f}));
```

### 5. Explicit feedback (new)

`FeedbackRegister` is the only mechanism for closing a signal loop. There are no other
shared mutable registers. The graph remains a DAG at every point except at `FeedbackRegister`
cut points, which the lowering pass handles.

---

## Trade-offs vs current Stream design

| Aspect | Stream (current) | Node/Signal (new) |
|--------|-----------------|-------------------|
| Node identity | Anonymous lambdas | Named concrete classes |
| Dispatch | `move_only_function` indirect | Virtual call (vtable) |
| Audio callback traversal | Recursive virtual dispatch | Flat list (lowered) |
| Feedback | `shared_ptr<float>` exception | `FeedbackRegister` (explicit) |
| Introspection | None | `dynamic_cast`, type name |
| Cache tick | `State` member | `Node` member (same logic) |
| Composition | Operators on `Stream` | Operators on `Signal` |

---

## Not in scope

- Multi-channel output — mono-first; stereo uses two separate `Signal` graphs
- Compile-time arity checking — fundsp/pdsp have it; ours is runtime
- Audio/control rate distinction — deliberately rejected; conflicts with the single-type
  Eurorack model and the sapf design philosophy
- Signal analysis (frequency response, latency tracking)
- Block-level `process(span<float>)` — phase 2; enables SIMD and per-node cache efficiency

---

## Files to create / modify

- `include/node.hpp` — `Node`, `Signal`, `ConstantNode`, `FeedbackRegister`, `MulNode`, `AddNode`, `SubNode`, `DivNode`
- `include/osc.hpp` — `SineOscillator : Node`
- `include/graph.hpp` — add `lower()` step; `Graph` becomes the render driver
- `include/delay.hpp` — `FeedbackRegister` replaces the `shared_ptr<float>` exception
- `src/graph.cpp` — update patches to use `Signal` instead of `Stream`
