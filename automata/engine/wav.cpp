#include "automata/engine/offline.hpp"

#include <utility>

#include "automata/config.hpp"

// The library's single miniaudio implementation TU: WAV encoding for this
// file, device I/O for the live host — every consumer links the one copy.
#define MINIAUDIO_IMPLEMENTATION
#include "miniaudio.h"

namespace automata {

Result<void> render_wav(GraphDef def, std::size_t nframes, const char* path) {
  const std::vector<float> pcm = render_interleaved(std::move(def), nframes);

  ma_encoder_config config = ma_encoder_config_init(
      ma_encoding_format_wav, ma_format_f32, 2, SampleRate);
  ma_encoder encoder;
  if (ma_encoder_init_file(path, &config, &encoder) != MA_SUCCESS) {
    return std::unexpected(Error::FileCreateFailed);
  }

  ma_uint64 frames_written = 0;
  const ma_result result = ma_encoder_write_pcm_frames(
      &encoder, pcm.data(), nframes, &frames_written);
  ma_encoder_uninit(&encoder);

  if (result != MA_SUCCESS || frames_written != nframes) {
    return std::unexpected(Error::EncodeFailed);
  }
  return {};
}

}  // namespace automata
