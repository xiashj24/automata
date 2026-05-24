#pragma once
#include <memory>
#include <stream.hpp>

namespace automata {

// A one-sample feedback register: read() returns the previous tick's value,
// write(src) passes src through while latching it for the next tick.
// Each Register instance is independent, so multiple loops can coexist.
class Delay {
  std::shared_ptr<float> reg;

public:
  Delay() : reg(std::make_shared<float>(0.f)) {}

  [[nodiscard]] Stream read() const {
    return Stream([reg = reg]() -> float { return *reg; });
  }

  [[nodiscard]] Stream write(Stream src) const {
    return Stream([src, reg = reg]() -> float {
      float v = src.next();
      *reg = v;
      return v;
    });
  }
};

}  // namespace automata
