#pragma once
#include <concepts>
#include <functional>
#include <string>
#include <string_view>
#include <cstdint>
#include <euclid.hpp>

namespace automata {

// clang-format off
// NOLINTBEGIN(clang-analyzer-core.StackAddressEscape, clang-analyzer-cplusplus.NewDeleteLeaks)
// clang-format on
// std::function owns its heap allocation via RAII; clang-analyzer does not
// model this correctly.

class Signal {
public:
  using Func = std::function<bool(int)>;

  explicit Signal(Func func) : fn_(std::move(func)) {}

  [[nodiscard]] bool operator()(int step) const { return fn_(step); }

  // Time operations
  [[nodiscard]] Signal operator>>(int offset) const {
    return Signal([fn = fn_, offset](int step) { return fn(step - offset); });
  }

  [[nodiscard]] Signal operator<<(int offset) const {
    return Signal([fn = fn_, offset](int step) { return fn(step + offset); });
  }

  // no use
  [[nodiscard]] Signal tile(int period) const {
    return Signal([fn = fn_, period](int step) {
      return fn(((step % period) + period) % period);
    });
  }

  // stretch: place each hit at step*factor, gaps become false
  [[nodiscard]] Signal stretch(int factor) const {
    return Signal([fn = fn_, factor](int step) {
      return step % factor == 0 && fn(step / factor);
    });
  }

  // compress: take every factor-th step of the source
  [[nodiscard]] Signal compress(int factor) const {
    return Signal([fn = fn_, factor](int step) { return fn(step * factor); });
  }

  // no good
  // concat: first first_length steps from this, then other (offset to start at
  // 0)
  [[nodiscard]] Signal concat(const Signal& other, int first_length) const {
    return Signal([lhs = fn_, rhs = other.fn_, first_length](int step) {
      return step < first_length ? lhs(step) : rhs(step - first_length);
    });
  }

  // Logic operations

  [[nodiscard]] Signal operator+(const Signal& rhs) const {
    return Signal([lhs = fn_, rhs = rhs.fn_](int step) {
      return lhs(step) || rhs(step);
    });
  }

  [[nodiscard]] Signal operator*(const Signal& rhs) const {
    return Signal([lhs = fn_, rhs = rhs.fn_](int step) {
      return lhs(step) && rhs(step);
    });
  }

  [[nodiscard]] Signal operator~() const {
    return Signal([fn = fn_](int step) { return !fn(step); });
  }

  [[nodiscard]] Signal operator-(const Signal& rhs) const {
    return Signal([lhs = fn_, rhs = rhs.fn_](int step) {
      return lhs(step) && !rhs(step);
    });
  }

  [[nodiscard]] Signal operator^(const Signal& rhs) const {
    return Signal(
        [lhs = fn_, rhs = rhs.fn_](int step) { return lhs(step) ^ rhs(step); });
  }

  // trigger: rising edge — true only on the first step of a gate burst
  [[nodiscard]] Signal trigger() const {
    return Signal([fn = fn_](int step) { return fn(step) && !fn(step - 1); });
  }

  // count: fire on every k-th hit; recomputes from step 0 for referential
  // transparency
  [[nodiscard]] Signal count(int max) const {
    return Signal([fn = fn_, max](int step) {
      int counter = 0;
      for (int i = 0; i <= step; ++i)
        if (fn(i) && ++counter == max) {
          counter = 0;
          if (i == step)
            return true;
        }
      return false;
    });
  }

  // Generators

  [[nodiscard]] static Signal euclid(int pulses, int steps) {
    return Signal([pulses, steps](int step) {
      int index = ((step % steps) + steps) % steps;
      return euclid_simple(static_cast<uint32_t>(pulses),
                           static_cast<uint32_t>(steps),
                           static_cast<uint32_t>(index));
    });
  }

  [[nodiscard]] static Signal fill() {
    return Signal([](int) { return true; });
  }

  [[nodiscard]] static Signal rest() {
    return Signal([](int) { return false; });
  }

  // from_pattern: periodic gate from "x.x." notation ('x'/'X'/'1' = hit)
  [[nodiscard]] static Signal from_pattern(std::string_view sv) {
    return Signal([pattern = std::string(sv)](int step) {
      int period = static_cast<int>(pattern.size());
      int index = ((step % period) + period) % period;
      char ch = pattern[static_cast<std::size_t>(index)];
      return ch == 'x' || ch == 'X' || ch == '1';
    });
  }

  std::string to_pattern(int length) {
    std::string result(static_cast<std::size_t>(length), '.');
    for (int i = 0; i < length; ++i)
      if (fn_(i))
        result[static_cast<std::size_t>(i)] = 'x';

    return result;
  }

private:
  Func fn_;
};
// clang-format off
// NOLINTEND(clang-analyzer-core.StackAddressEscape, clang-analyzer-cplusplus.NewDeleteLeaks)
// clang-format on

using Gate = Signal;

}  // namespace automata
