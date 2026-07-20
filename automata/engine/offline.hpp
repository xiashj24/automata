#pragma once

#include <cstddef>
#include <vector>

#include "automata/core/error.hpp"
#include "automata/graph/def.hpp"

// Offline rendering (ADR 0004): the same block path the live host drives,
// called in a loop. Deterministic — same def, same samples, any speed.

namespace automata {

// Interleaved stereo, nframes * 2 floats.
[[nodiscard]] std::vector<float> render_interleaved(GraphDef def,
                                                    std::size_t nframes);

// 32-bit float stereo WAV at SampleRate.
// Errors: FileCreateFailed, EncodeFailed.
[[nodiscard]] Result<void> render_wav(GraphDef def,
                                      std::size_t nframes,
                                      const char* path);

}  // namespace automata
