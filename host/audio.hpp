#pragma once

#include <memory>
#include <string_view>

#include "automata/core/error.hpp"

// The playback device: its callback runs Engine::render and nothing else
// (ADR 0003). Construct after the Engine and destroy before it, so the
// audio thread is silenced before anything it renders from.

namespace automata {
class Engine;
}

namespace automata::host {

class AudioDevice {
public:
  // Opens and starts the default playback device at the engine's format;
  // miniaudio resamples if the hardware differs (ADR 0004).
  // Errors: DeviceInitFailed, DeviceStartFailed.
  [[nodiscard]] static Result<std::unique_ptr<AudioDevice>> start(
      Engine& engine);

  ~AudioDevice();
  AudioDevice(const AudioDevice&) = delete;
  AudioDevice& operator=(const AudioDevice&) = delete;

  [[nodiscard]] std::string_view device_name() const;

private:
  AudioDevice() = default;

  struct Impl;  // keeps miniaudio out of the header
  std::unique_ptr<Impl> impl_;
};

}  // namespace automata::host
