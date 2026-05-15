#pragma once
#include <algorithm>
#include <functional>
#include <span>

#include <signal.hpp>

namespace automata {

// signal for stateful processing — no random access like Signal
// what is the right copy semantics? forbid it?

class SequentialSignal {
  using Func = std::function<float()>;
  // Stored by value, not shared_ptr: the generator has mutable state
  Func gen;

public:
  explicit SequentialSignal(Func gen) : gen(std::move(gen)) {}

  // cppcheck-suppress noExplicitConstructor
  SequentialSignal(float v) : gen([v]() { return v; }) {}
  // cppcheck-suppress noExplicitConstructor
  SequentialSignal(const Signal& sig)
      : gen([sig, n = 0]() mutable { return sig[n++]; }) {}

  // note: for audio signal
  // overflow will happen after 2^31 / 48000 Hz ≈ 12.4 hours

  float next() { return gen(); }

  void render(std::span<float> buf) {
    std::generate(buf.begin(), buf.end(), [this] { return next(); });
  }
};

}  // namespace automata
