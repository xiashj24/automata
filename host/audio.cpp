#include "host/audio.hpp"

#include "automata/config.hpp"
#include "automata/engine/engine.hpp"

#include "miniaudio.h"

namespace automata::host {

struct AudioDevice::Impl {
  ma_device device{};
};

namespace {

void data_callback(ma_device* device,
                   void* out,
                   const void*,
                   ma_uint32 frames) {
  auto& engine = *static_cast<Engine*>(device->pUserData);
  engine.render(static_cast<float*>(out), frames);
}

}  // namespace

Result<std::unique_ptr<AudioDevice>> AudioDevice::start(Engine& engine) {
  std::unique_ptr<AudioDevice> device{new AudioDevice};
  device->impl_ = std::make_unique<Impl>();

  ma_device_config config = ma_device_config_init(ma_device_type_playback);
  config.playback.format = ma_format_f32;
  config.playback.channels = 2;
  config.sampleRate = SampleRate;
  config.dataCallback = &data_callback;
  config.pUserData = &engine;

  if (ma_device_init(nullptr, &config, &device->impl_->device) != MA_SUCCESS) {
    device->impl_.reset();  // nothing to uninit
    return std::unexpected(Error::DeviceInitFailed);
  }
  if (ma_device_start(&device->impl_->device) != MA_SUCCESS) {
    return std::unexpected(Error::DeviceStartFailed);
  }
  return device;
}

AudioDevice::~AudioDevice() {
  if (impl_ != nullptr) {
    ma_device_uninit(&impl_->device);
  }
}

std::string_view AudioDevice::device_name() const {
  return impl_->device.playback.name;
}

}  // namespace automata::host
