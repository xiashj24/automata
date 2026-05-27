#pragma once

#include <atomic>
#include <cstdint>
#include <memory>

#include <stream.hpp>

namespace automata::v2 {

// ─── Node
// ─────────────────────────────────────────────────────────────────────
//
// Abstract base. Owns the cache-dedup mechanism: process() is called at most
// once per render tick. Uses automata::current_tick so v2 Nodes integrate
// with the existing Graph render loop without a separate tick counter.

class Node {
public:
  virtual ~Node() = default;

  float next() {
    const uint64_t tick = automata::current_tick;
    if (tick == 0)
      return process();
    if (cache_tick == tick)
      return cache_val;
    cache_val = process();
    cache_tick = tick;
    return cache_val;
  }

  virtual void reset() {}

protected:
  virtual float process() = 0;

private:
  float cache_val = 0.f;
  uint64_t cache_tick = ~0ull;
};

// ─── Signal
// ───────────────────────────────────────────────────────────────────
//
// shared_ptr<Node> handle. Copying is O(1) and shares the same Node — the
// cache_tick dedup in Node::next() ensures a fanned-out Signal evaluates once
// per tick regardless of how many consumers call next().

class Signal {
public:
  Signal() = default;
  // cppcheck-suppress noExplicitConstructor
  Signal(float constant);  // defined below
  explicit Signal(std::shared_ptr<Node> n) : impl(std::move(n)) {}

  float next() const { return impl->next(); }
  void reset() const {
    if (impl)
      impl->reset();
  }
  explicit operator bool() const { return impl != nullptr; }

  // Wrap this Signal in a Stream so it can be added to an existing Graph.
  [[nodiscard]] Stream to_stream() const {
    Signal self = *this;
    return Stream([self]() { return self.next(); });
  }

  std::shared_ptr<Node> node() const { return impl; }

private:
  std::shared_ptr<Node> impl;
};

// ─── Arithmetic nodes
// ─────────────────────────────────────────────────────────

class MulNode : public Node {
public:
  MulNode(Signal a, Signal b) : a(a), b(b) {}
  void reset() override {
    a.reset();
    b.reset();
  }
  float process() override { return a.next() * b.next(); }

private:
  Signal a, b;
};

class AddNode : public Node {
public:
  AddNode(Signal a, Signal b) : a(a), b(b) {}
  void reset() override {
    a.reset();
    b.reset();
  }
  float process() override { return a.next() + b.next(); }

private:
  Signal a, b;
};

class SubNode : public Node {
public:
  SubNode(Signal a, Signal b) : a(a), b(b) {}
  void reset() override {
    a.reset();
    b.reset();
  }
  float process() override { return a.next() - b.next(); }

private:
  Signal a, b;
};

// is this actually useful?
class DivNode : public Node {
public:
  DivNode(Signal a, Signal b) : a(a), b(b) {}
  void reset() override {
    a.reset();
    b.reset();
  }
  float process() override { return a.next() / b.next(); }

private:
  Signal a, b;
};

// ─── ConstantNode
// ─────────────────────────────────────────────────────────────
//
// Backing node for Signal(float) and the Param bridge. set() is safe to call
// from the control thread; the audio thread reads with relaxed ordering.

class ConstantNode : public Node {
public:
  explicit ConstantNode(float v) : value(v) {}
  void set(float v) { value.store(v, std::memory_order_relaxed); }
  float get() const { return value.load(std::memory_order_relaxed); }

protected:
  float process() override { return value.load(std::memory_order_relaxed); }

private:
  std::atomic<float> value;
};

// ─── Signal member definitions (after node types are complete)
// ─────────────────

inline Signal::Signal(float constant)
    : impl(std::make_shared<ConstantNode>(constant)) {}

inline Signal operator+(Signal a, Signal b) {
  return Signal(std::make_shared<AddNode>(a, b));
}
inline Signal operator-(Signal a, Signal b) {
  return Signal(std::make_shared<SubNode>(a, b));
}
inline Signal operator*(Signal a, Signal b) {
  return Signal(std::make_shared<MulNode>(a, b));
}
inline Signal operator/(Signal a, Signal b) {
  return Signal(std::make_shared<DivNode>(a, b));
}

// ─── Bridge: wrap an old Stream as a v2 Signal
// ────────────────────────────────

class StreamNode : public Node {
public:
  explicit StreamNode(Stream s) : src(std::move(s)) {}
  float process() override { return src.next(); }

private:
  Stream src;
};

inline Signal from_stream(Stream s) {
  return Signal(std::make_shared<StreamNode>(std::move(s)));
}

}  // namespace automata::v2
