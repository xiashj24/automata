#include "automata/engine/offline.hpp"

#include <algorithm>
#include <utility>

#include "automata/config.hpp"
#include "automata/core/transport.hpp"
#include "automata/engine/graph.hpp"

namespace automata {

std::vector<float> render_interleaved(GraphDef def, std::size_t nframes) {
  Transport transport;  // default tempo; cycles land on the same beats live
  Graph graph(std::move(def), nullptr, &transport);
  std::vector<float> out(nframes * 2, 0.f);
  std::size_t written = 0;
  while (written < nframes) {
    graph.process_block();
    transport.advance(BlockSize);
    const std::size_t take = std::min(BlockSize, nframes - written);
    const std::span<const float> left = graph.left();
    const std::span<const float> right = graph.right();
    for (std::size_t i = 0; i < take; ++i) {
      out[2 * (written + i)] = left[i];
      out[2 * (written + i) + 1] = right[i];
    }
    written += take;
  }
  return out;
}

}  // namespace automata
