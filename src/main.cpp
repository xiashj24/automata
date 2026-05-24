#define NOMINMAX
#define MA_IMPLEMENTATION
#include <miniaudio.h>

#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <print>

#include <graph.hpp>
#include <samplerate.hpp>

// ─── Graph runtime
// ────────────────────────────────────────────────────────────

namespace {

std::shared_ptr<automata::Graph> build_graph() {
  auto g = std::make_shared<automata::Graph>();
  define_graph(*g);
  return g;
}

std::atomic<std::shared_ptr<automata::Graph>> live_graph{build_graph()};

void audio_callback(ma_device*,
                    void* output,
                    [[maybe_unused]] const void* input,
                    ma_uint32 frames) {
  live_graph.load(std::memory_order_acquire)
      ->render({static_cast<float*>(output), frames});
}

}  // namespace

// ─────────────────────────────────────────────────────────────────────────────
constexpr int frame_count = 256;

int main() {
  {
    auto g = live_graph.load(std::memory_order_acquire);
    std::printf("graph: %zu params, %zu outputs\n", g->param_count(),
                g->output_count());
  }

  ma_device_config config = ma_device_config_init(ma_device_type_playback);
  config.playback.format = ma_format_f32;
  config.playback.channels = 1;
  config.sampleRate = automata::SampleRate;
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
