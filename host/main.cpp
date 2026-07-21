#include <algorithm>
#include <atomic>
#include <charconv>
#include <chrono>
#include <csignal>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <optional>
#include <string_view>
#include <thread>
#include <utility>

#include "automata/config.hpp"
#include "automata/engine/engine.hpp"
#include "automata/engine/reconcile.hpp"
#include "automata/graph/builder.hpp"
#include "automata/graph/rebind.hpp"
#include "automata/ugens/vocabulary.hpp"
#include "host/audio.hpp"
#include "host/generation.hpp"
#include "host/mouse.hpp"
#include "host/osc_listener.hpp"
#include "host/watcher.hpp"

// The live host (ADR 0001/0003): watch the built patch library, load each
// new generation, describe and rebind its def, and hand it to the
// reconciler while the device thread renders. Declaration order in run() is
// the destruction-order contract: pool outlives engine outlives reconciler,
// and the device dies first, silencing the audio thread before anything it
// renders from.

namespace {

std::atomic<bool> g_stop{false};

void handle_signal(int) {
  g_stop.store(true);
}

struct Options {
  std::filesystem::path dll;
  float bpm = 0.f;  // 0 = keep the transport default
  automata::Quantize quantize = automata::Quantize::Immediate;
  float fade_seconds = automata::DefaultCrossfadeSeconds;
  std::uint16_t osc_port = 0;  // 0 = no listener
};

[[nodiscard]] std::optional<float> parse_float(std::string_view text) {
  float value = 0.f;
  const auto [ptr, ec] =
      std::from_chars(text.data(), text.data() + text.size(), value);
  if (ec != std::errc{} || ptr != text.data() + text.size()) {
    return std::nullopt;
  }
  return value;
}

[[nodiscard]] std::optional<std::uint16_t> parse_port(std::string_view text) {
  std::uint16_t value = 0;
  const auto [ptr, ec] =
      std::from_chars(text.data(), text.data() + text.size(), value);
  if (ec != std::errc{} || ptr != text.data() + text.size() || value == 0) {
    return std::nullopt;
  }
  return value;
}

[[nodiscard]] std::optional<Options> parse_args(int argc, char** argv) {
  if (argc < 2) {
    return std::nullopt;
  }
  Options options{.dll = argv[1]};
  for (int i = 2; i < argc; ++i) {
    const std::string_view arg = argv[i];
    const std::string_view value = i + 1 < argc ? argv[i + 1] : "";
    if (arg == "--bpm") {
      const auto bpm = parse_float(value);
      if (!bpm.has_value() || *bpm <= 0.f) {
        return std::nullopt;
      }
      options.bpm = *bpm;
      ++i;
    } else if (arg == "--fade") {
      const auto fade = parse_float(value);
      if (!fade.has_value() || *fade < 0.f) {
        return std::nullopt;
      }
      options.fade_seconds = *fade;
      ++i;
    } else if (arg == "--osc") {
      const auto port = parse_port(value);
      if (!port.has_value()) {
        return std::nullopt;
      }
      options.osc_port = *port;
      ++i;
    } else if (arg == "--quantize") {
      if (value == "immediate") {
        options.quantize = automata::Quantize::Immediate;
      } else if (value == "beat") {
        options.quantize = automata::Quantize::NextBeat;
      } else if (value == "bar") {
        options.quantize = automata::Quantize::NextBar;
      } else {
        return std::nullopt;
      }
      ++i;
    } else {
      return std::nullopt;
    }
  }
  return options;
}

[[nodiscard]] const char* error_name(automata::Error error) {
  switch (error) {
    case automata::Error::FileCreateFailed:
      return "file create failed";
    case automata::Error::EncodeFailed:
      return "encode failed";
    case automata::Error::FileCopyFailed:
      return "file copy failed (still being written?)";
    case automata::Error::LibraryLoadFailed:
      return "library load failed";
    case automata::Error::SymbolNotFound:
      return "not a patch library (missing AUTOMATA_PATCH exports)";
    case automata::Error::AbiMismatch:
      return "ABI stamp mismatch (rebuild patch and host together)";
    case automata::Error::DeviceInitFailed:
      return "audio device init failed";
    case automata::Error::DeviceStartFailed:
      return "audio device start failed";
    case automata::Error::SocketBindFailed:
      return "OSC socket bind failed (port in use?)";
  }
  std::unreachable();
}

void reload(const Options& options,
            automata::host::GenerationPool& pool,
            const automata::KernelRegistry& registry,
            automata::Reconciler& reconciler,
            automata::host::FileWatcher& watcher,
            automata::Hash& live_def_hash) {
  auto loaded = pool.load(options.dll);
  if (!loaded.has_value()) {
    // A copy that failed mid-write retries on the next poll; anything else
    // is final until the file changes again.
    if (loaded.error() != automata::Error::FileCopyFailed) {
      watcher.mark_handled();
      std::fprintf(stderr, "[host] load: %s\n", error_name(loaded.error()));
    }
    return;
  }
  automata::host::GenerationRef gen = std::move(*loaded);

  automata::GraphBuilder builder;
  gen->describe()(&builder);
  if (!builder.has_out()) {
    watcher.mark_handled();
    std::fprintf(stderr, "[host] gen %llu: patch never called g.out()\n",
                 static_cast<unsigned long long>(gen->id()));
    return;
  }
  automata::GraphDef def = builder.build();
  const std::uint32_t foreign = rebind_kernels(def, registry);

  const char* landing = def.def_hash == live_def_hash ? "values" : "swap";
  std::printf("[host] gen %llu: %zu nodes -> %s%s\n",
              static_cast<unsigned long long>(gen->id()), def.nodes.size(),
              landing,
              foreign > 0 ? " (custom kernels: generation pinned)" : "");
  live_def_hash = def.def_hash;

  reconciler.update(
      std::move(def), options.quantize, options.fade_seconds,
      foreign > 0 ? std::move(gen) : automata::host::GenerationRef{});
  watcher.mark_handled();
}

[[nodiscard]] int run(const Options& options) {
  automata::host::GenerationPool pool;
  const automata::KernelRegistry registry = automata::vocabulary_registry();
  automata::Engine engine;
  automata::Reconciler reconciler(engine);
  automata::host::FileWatcher watcher(options.dll);

  if (options.bpm > 0.f) {
    (void)engine.inbox().try_push(
        {.kind = automata::InMsg::Kind::SetBpm, .bpm = options.bpm});
  }

  std::unique_ptr<automata::host::OscListener> osc;
  if (options.osc_port != 0) {
    auto listener =
        automata::host::OscListener::start(options.osc_port, engine.bus());
    if (!listener.has_value()) {
      std::fprintf(stderr, "[host] %s\n", error_name(listener.error()));
      return EXIT_FAILURE;
    }
    osc = std::move(*listener);
    std::printf("[host] OSC listening on udp/%u (params + /bpm)\n",
                osc->port());
  }

  // /bpm is a reserved slot bridged to the transport: this loop is the
  // inbox's single producer, so the OSC thread must go through the bus.
  automata::ControlBus::Slot* bpm_slot = engine.bus().channel("bpm");
  float bpm_seen = 0.f;

  auto device = automata::host::AudioDevice::start(engine);
  if (!device.has_value()) {
    std::fprintf(stderr, "[host] %s\n", error_name(device.error()));
    return EXIT_FAILURE;
  }
  const std::string_view name = (*device)->device_name();
  std::printf("[host] %.*s @ %u Hz | watching %s (Ctrl+C stops)\n",
              static_cast<int>(name.size()), name.data(), automata::SampleRate,
              options.dll.string().c_str());

  automata::Hash live_def_hash = 0;
  while (!g_stop.load()) {
    automata::host::poll_mouse(engine.bus());
    if (bpm_slot != nullptr) {
      const float requested = bpm_slot->read_or(0.f);
      if (requested > 0.f && requested != bpm_seen) {
        const float bpm = std::clamp(requested, 20.f, 300.f);
        if (engine.inbox().try_push(
                {.kind = automata::InMsg::Kind::SetBpm, .bpm = bpm})) {
          bpm_seen = requested;  // a full inbox retries on the next tick
          std::printf("[host] bpm -> %g\n", static_cast<double>(bpm));
        }
      }
    }
    if (watcher.poll()) {
      reload(options, pool, registry, reconciler, watcher, live_def_hash);
    }
    reconciler.drain();
    pool.collect();
    std::this_thread::sleep_for(std::chrono::milliseconds(25));
  }
  std::printf("[host] stopped\n");
  return EXIT_SUCCESS;
}

}  // namespace

int main(int argc, char** argv) {
  // Logs are a live surface: flush as they happen, even piped into an editor.
  (void)std::setvbuf(stdout, nullptr, _IONBF, 0);
  const std::optional<Options> options = parse_args(argc, argv);
  if (!options.has_value()) {
    std::fprintf(stderr,
                 "usage: host <patch-library> [--bpm N] "
                 "[--quantize immediate|beat|bar] [--fade seconds] "
                 "[--osc port]\n");
    return EXIT_FAILURE;
  }
  (void)std::signal(SIGINT, &handle_signal);
  (void)std::signal(SIGTERM, &handle_signal);
  return run(*options);
}
