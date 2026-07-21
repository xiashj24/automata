#include "host/osc_listener.hpp"

#include <array>
#include <cstddef>
#include <cstdio>
#include <span>

#include "automata/engine/osc.hpp"

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <winsock2.h>
#else
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>
#endif

namespace automata::host {

namespace {

constexpr auto InvalidSocket = static_cast<std::uintptr_t>(-1);

// The receive timeout bounds shutdown latency: the loop rechecks stop_
// at least this often.
constexpr long ReceiveTimeoutMs = 100;

[[nodiscard]] std::uintptr_t open_udp_socket(std::uint16_t port) {
#if defined(_WIN32)
  WSADATA wsa{};
  if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
    return InvalidSocket;
  }
  const SOCKET s = ::socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
  if (s == INVALID_SOCKET) {
    WSACleanup();
    return InvalidSocket;
  }
  const DWORD timeout = ReceiveTimeoutMs;
  (void)::setsockopt(s, SOL_SOCKET, SO_RCVTIMEO,
                     reinterpret_cast<const char*>(&timeout), sizeof(timeout));
  sockaddr_in addr{};
  addr.sin_family = AF_INET;
  addr.sin_port = htons(port);
  addr.sin_addr.s_addr = htonl(INADDR_ANY);
  if (::bind(s, reinterpret_cast<const sockaddr*>(&addr), sizeof(addr)) != 0) {
    ::closesocket(s);
    WSACleanup();
    return InvalidSocket;
  }
  return static_cast<std::uintptr_t>(s);
#else
  const int s = ::socket(AF_INET, SOCK_DGRAM, 0);
  if (s < 0) {
    return InvalidSocket;
  }
  const timeval timeout{.tv_sec = 0, .tv_usec = ReceiveTimeoutMs * 1000};
  (void)::setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
  sockaddr_in addr{};
  addr.sin_family = AF_INET;
  addr.sin_port = htons(port);
  addr.sin_addr.s_addr = htonl(INADDR_ANY);
  if (::bind(s, reinterpret_cast<const sockaddr*>(&addr), sizeof(addr)) != 0) {
    ::close(s);
    return InvalidSocket;
  }
  return static_cast<std::uintptr_t>(s);
#endif
}

void close_udp_socket(std::uintptr_t socket) {
#if defined(_WIN32)
  ::closesocket(static_cast<SOCKET>(socket));
  WSACleanup();
#else
  ::close(static_cast<int>(socket));
#endif
}

// One datagram or nothing; <= 0 covers both timeout and transient errors.
[[nodiscard]] int receive_datagram(std::uintptr_t socket,
                                   std::span<std::byte> buffer) {
#if defined(_WIN32)
  return ::recvfrom(static_cast<SOCKET>(socket),
                    reinterpret_cast<char*>(buffer.data()),
                    static_cast<int>(buffer.size()), 0, nullptr, nullptr);
#else
  return static_cast<int>(::recvfrom(static_cast<int>(socket), buffer.data(),
                                     buffer.size(), 0, nullptr, nullptr));
#endif
}

}  // namespace

Result<std::unique_ptr<OscListener>> OscListener::start(std::uint16_t port,
                                                        ControlBus& bus) {
  const std::uintptr_t socket = open_udp_socket(port);
  if (socket == InvalidSocket) {
    return std::unexpected(Error::SocketBindFailed);
  }
  return std::unique_ptr<OscListener>(new OscListener(socket, port, bus));
}

OscListener::OscListener(std::uintptr_t socket,
                         std::uint16_t port,
                         ControlBus& bus)
    : socket_(socket),
      port_(port),
      bus_(bus),
      thread_([this] { receive_loop(); }) {}

OscListener::~OscListener() {
  stop_.store(true);
  thread_.join();
  close_udp_socket(socket_);
}

void OscListener::receive_loop() {
  // Every accepted write is echoed — the console is the confirmation that a
  // controller reached the bus.
  const OscWriteObserver echo = [](void*, std::string_view name,
                                   std::uint32_t ordinal, float value) {
    if (ordinal == 0) {
      std::printf("[osc] %.*s = %g\n", static_cast<int>(name.size()),
                  name.data(), static_cast<double>(value));
    } else {
      std::printf("[osc] %.*s/%u = %g\n", static_cast<int>(name.size()),
                  name.data(), ordinal, static_cast<double>(value));
    }
  };
  std::array<std::byte, 4096> buffer;
  while (!stop_.load()) {
    const int received = receive_datagram(socket_, buffer);
    if (received <= 0) {
      continue;
    }
    (void)apply_osc_packet({buffer.data(), static_cast<std::size_t>(received)},
                           bus_, echo, nullptr);
  }
}

}  // namespace automata::host
