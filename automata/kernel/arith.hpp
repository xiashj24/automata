#pragma once

// Arithmetic kernels behind Signal's operators. Stateless, but shaped like
// every other kernel so the graph machinery has no special cases.

namespace automata {

class Add {
public:
  [[nodiscard]] float process(float a, float b) { return a + b; }
  void reset() {}
};

class Sub {
public:
  [[nodiscard]] float process(float a, float b) { return a - b; }
  void reset() {}
};

class Mul {
public:
  [[nodiscard]] float process(float a, float b) { return a * b; }
  void reset() {}
};

class Div {
public:
  [[nodiscard]] float process(float a, float b) { return a / b; }
  void reset() {}
};

class Neg {
public:
  [[nodiscard]] float process(float a) { return -a; }
  void reset() {}
};

class Less {
public:
  [[nodiscard]] float process(float a, float b) { return a < b ? 1.f : 0.f; }
  void reset() {}
};

class GreaterEqual {
public:
  [[nodiscard]] float process(float a, float b) { return a >= b ? 1.f : 0.f; }
  void reset() {}
};

}  // namespace automata
