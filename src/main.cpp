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
float fm_depth = 5.f;

float lfo_freq = 0.5f;  // hz
float w_lfo = lfo_freq / static_cast<float>(sample_rate);

// float lag_state = 0.f;
SvfState svf_state;

// 2-op phase modulation
auto lfo = osc(w_lfo);

auto modulator = osc(w * fm_index);
auto carrier = osc(w, modulator * fm_depth * lfo);

auto sawtooth = saw(w);

// lag(Stream x, Stream a, float& yz)

auto svf_out = svf_lp(
    sawtooth,
    (2000.f + lfo * 1800.f) / static_cast<float>(sample_rate),
    0.9f,
    svf_state);

auto out = svf_out * 0.2;

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
