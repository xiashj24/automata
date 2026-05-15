#include <atomic>
#include <cstdlib>
#include <iostream>
#include <print>
#include <span>

#define MA_IMPLEMENTATION
#include <miniaudio.h>

#include <signal.hpp>
#include <stream.hpp>
#include <filter.hpp>


using namespace automata;

namespace {

constexpr int sample_rate = 48000;
constexpr int frame_count = 256;

float base_freq = 110.0f;  // hz
float w = base_freq / static_cast<float>(sample_rate);
float fm_index = 10.f;
float fm_depth = 1.f;

float exp_lfo_freq = 1.f;  // hz
float w_lfo = exp_lfo_freq / static_cast<float>(sample_rate);

float lag_state = 0.f;

// 2-op phase modulation
auto lfo = phasor(w_lfo);

auto modulator = osc(w * fm_index);
auto carrier = osc(w, modulator * fm_depth * lfo);

// lag(Stream x, Stream a, float& yz)

auto lag_out = lag(carrier, lfo, lag_state);

auto out = lag_out;

void audio_callback(ma_device*,
                    void* output,
                    [[maybe_unused]] const void* input,
                    ma_uint32 frames) {
  auto* buf = static_cast<float*>(output);
  out.render({buf, frames});
}

}  // namespace

int main() {
  ma_device_config config = ma_device_config_init(ma_device_type_playback);
  config.playback.format = ma_format_f32;
  config.playback.channels = 1;
  config.sampleRate = sample_rate;
  config.periodSizeInFrames = frame_count;
  config.dataCallback = audio_callback;

  ma_device device;
  if (ma_device_init(nullptr, &config, &device) != MA_SUCCESS) {
    std::println(stderr, "Failed to initialize playback device.");
    return EXIT_FAILURE;
  }

  if (ma_device_start(&device) != MA_SUCCESS) {
    ma_device_uninit(&device);
    std::println(stderr, "Failed to start playback device.");
    return EXIT_FAILURE;
  }

  std::println("Press Enter to stop.");
  std::cin.get();

  ma_device_uninit(&device);
  return EXIT_SUCCESS;
}
