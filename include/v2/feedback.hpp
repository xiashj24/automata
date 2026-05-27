#pragma once

#include <memory>

#include <v2/node.hpp>

namespace automata::v2 {

// FeedbackReadNode: returns the register value written in the previous tick.
// Starts at 0; reset() zeroes it.
class FeedbackReadNode : public Node {
public:
  explicit FeedbackReadNode(std::shared_ptr<float> r) : reg(std::move(r)) {}
  void reset() override { *reg = 0.f; }
  float process() override { return *reg; }

private:
  std::shared_ptr<float> reg;
};

// FeedbackWriteNode: evaluates src, stores its value in the register, and
// passes it through. Must be on the render path each tick for the register to
// be updated.
class FeedbackWriteNode : public Node {
public:
  FeedbackWriteNode(Signal src, std::shared_ptr<float> r)
      : src(std::move(src)), reg(std::move(r)) {}

  void reset() override {
    *reg = 0.f;
    src.reset();
  }

  float process() override {
    float v = src.next();
    *reg = v;
    return v;
  }

private:
  Signal src;
  std::shared_ptr<float> reg;
};

// ─── FeedbackRegister
// ─────────────────────────────────────────────────────────
//
// Named single-sample delay register. Replaces the shared_ptr<float> exception
// in delay.hpp with an explicit, topology-visible construct.
//
// Usage:
//   FeedbackRegister fb;
//   Signal out = some_processing(fb.read());
//   return fb.write(out);   // write() must be the graph output
//
// The write() return value must flow to the graph output so that
// FeedbackWriteNode::process() is called each tick. fb.read() returns last
// tick's written value (0 on the first tick).

class FeedbackRegister {
public:
  FeedbackRegister() : reg(std::make_shared<float>(0.f)) {}

  [[nodiscard]] Signal read() const {
    return Signal(std::make_shared<FeedbackReadNode>(reg));
  }

  [[nodiscard]] Signal write(Signal src) const {
    return Signal(std::make_shared<FeedbackWriteNode>(std::move(src), reg));
  }

private:
  std::shared_ptr<float> reg;
};

}  // namespace automata::v2
