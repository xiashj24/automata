#pragma once

#include <atomic>
#include <cstdint>
#include <memory>
#include <thread>

#include "automata/core/control_bus.hpp"
#include "automata/core/error.hpp"

// The OSC thread (ADR 0003/0007): a UDP socket drained by a background
// thread straight into the ControlBus. Bound on all interfaces so a phone
// or tablet controller on the LAN can reach the instrument.

namespace automata::host {

class OscListener {
public:
  // Errors: SocketBindFailed.
  [[nodiscard]] static Result<std::unique_ptr<OscListener>> start(
      std::uint16_t port,
      ControlBus& bus);
  ~OscListener();
  OscListener(const OscListener&) = delete;
  OscListener& operator=(const OscListener&) = delete;

  [[nodiscard]] std::uint16_t port() const noexcept { return port_; }

private:
  OscListener(std::uintptr_t socket, std::uint16_t port, ControlBus& bus);
  void receive_loop();

  std::uintptr_t socket_;
  std::uint16_t port_;
  ControlBus& bus_;
  std::atomic<bool> stop_{false};
  std::thread thread_;  // last: it runs against the members above
};

}  // namespace automata::host
