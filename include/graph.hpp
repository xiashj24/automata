#pragma once

#include <atomic>
#include <memory>
#include <span>
#include <string>
#include <vector>

#include <stream.hpp>

namespace automata {

// TODO: improve parameter system

class Param {
public:
  Param(std::string n, float init, float lo, float hi)
      : slot(std::make_shared<std::atomic<float>>(init)),
        name(std::move(n)),
        min_val(lo),
        max_val(hi) {}

  void set(float v) { slot->store(v, std::memory_order_relaxed); }
  float get() const { return slot->load(std::memory_order_relaxed); }
  // cppcheck-suppress noExplicitConstructor
  operator Stream() const {
    return Stream(
        [s = slot]() -> float { return s->load(std::memory_order_relaxed); });
  }

private:
  std::shared_ptr<std::atomic<float>> slot;

  std::string name;
  float min_val, max_val;
};

class Graph {
public:
  Param& make_param(std::string name, float init, float lo, float hi) {
    params.push_back(std::make_unique<Param>(std::move(name), init, lo, hi));
    return *params.back();
  }

  void add_output(std::string name, Stream s) {
    outputs.push_back({std::move(name), std::move(s)});
  }

  void render(std::span<float> buf) {
    for (auto& sample : buf) {
      ++current_tick;
      float sum = 0.f;
      for (auto& out : outputs) {
        float v = out.stream.next();
        out.lkg_val = v;
        sum += v;
      }
      sample = sum;
    }
  }

  void render_multi(std::vector<std::span<float>> bufs) {
    std::size_t frames = bufs.empty() ? 0 : bufs[0].size();
    for (std::size_t i = 0; i < frames; ++i) {
      ++current_tick;
      for (std::size_t j = 0; j < outputs.size() && j < bufs.size(); ++j) {
        float v = outputs[j].stream.next();
        outputs[j].lkg_val = v;
        bufs[j][i] = v;
      }
    }
  }

  std::size_t param_count() const { return params.size(); }
  std::size_t output_count() const { return outputs.size(); }
  float lkg(std::size_t output_idx) const {
    return output_idx < outputs.size() ? outputs[output_idx].lkg_val : 0.f;
  }

private:
  struct OutputSlot {
    std::string name;
    Stream stream;
    float lkg_val = 0.f;
  };

  std::vector<std::unique_ptr<Param>> params;
  std::vector<OutputSlot> outputs;
};

}  // namespace automata

// User implements this in graph.cpp — the only function they need to define
void define_graph(automata::Graph& g);
