#include "automata/engine/osc.hpp"

#include <array>
#include <bit>
#include <charconv>
#include <cmath>
#include <cstring>
#include <string_view>
#include <type_traits>

namespace automata {

namespace {

// Enough for any controller message; extras still parse, just don't write.
constexpr std::size_t MaxNumericArgs = 32;
constexpr int MaxBundleDepth = 8;

[[nodiscard]] constexpr std::size_t padded4(std::size_t n) noexcept {
  return (n + 3u) & ~std::size_t{3};
}

template <typename T>
[[nodiscard]] T read_big_endian(const std::byte* src) noexcept {
  static_assert(sizeof(T) == 4 || sizeof(T) == 8);
  using U = std::conditional_t<sizeof(T) == 4, std::uint32_t, std::uint64_t>;
  U raw = 0;
  std::memcpy(&raw, src, sizeof(U));
  if constexpr (std::endian::native != std::endian::big) {
    raw = std::byteswap(raw);
  }
  return std::bit_cast<T>(raw);
}

// Argument k of an address hashes as if the name were literally "name/k",
// so an xy pad's second float is reachable as param("pad/1").
[[nodiscard]] Hash arg_slot_name(Hash base, std::size_t k) noexcept {
  if (k == 0) {
    return base;
  }
  std::array<char, 12> text{'/'};
  const auto [end, ec] =
      std::to_chars(text.data() + 1, text.data() + text.size(), k);
  (void)ec;  // k < MaxNumericArgs always fits
  return hash_string({text.data(), static_cast<std::size_t>(end - text.data())},
                     base);
}

struct NumericArgs {
  std::array<float, MaxNumericArgs> values{};
  std::array<bool, MaxNumericArgs> finite{};
  std::size_t count = 0;

  // A non-finite value keeps its ordinal (an xy pad's y stays y) but is
  // never written.
  void push(float value) {
    if (count == MaxNumericArgs) {
      return;
    }
    values[count] = value;
    finite[count] = std::isfinite(value);
    ++count;
  }
};

// One message: commit to the bus only after the whole thing parses.
[[nodiscard]] std::uint32_t apply_message(std::span<const std::byte> packet,
                                          ControlBus& bus,
                                          OscWriteObserver observer,
                                          void* context) {
  const std::byte* data = packet.data();
  const std::size_t total = packet.size();
  if (total < 4 || total % 4 != 0 || static_cast<char>(data[0]) != '/') {
    return 0;
  }

  std::size_t addr_len = 0;
  while (addr_len < total && static_cast<char>(data[addr_len]) != '\0') {
    ++addr_len;
  }
  if (addr_len < 2 || addr_len == total) {
    return 0;
  }
  std::size_t pos = padded4(addr_len + 1);

  if (pos >= total || static_cast<char>(data[pos]) != ',') {
    return 0;
  }
  const std::size_t tag_start = pos + 1;
  std::size_t tag_len = 0;
  while (tag_start + tag_len < total &&
         static_cast<char>(data[tag_start + tag_len]) != '\0') {
    ++tag_len;
  }
  if (tag_start + tag_len == total) {
    return 0;
  }
  pos += padded4(tag_len + 2);  // ',' + tags + '\0'

  NumericArgs args;
  for (std::size_t i = 0; i < tag_len; ++i) {
    switch (static_cast<char>(data[tag_start + i])) {
      case 'f': {
        if (pos + 4 > total) {
          return 0;
        }
        args.push(read_big_endian<float>(data + pos));
        pos += 4;
        break;
      }
      case 'i': {
        if (pos + 4 > total) {
          return 0;
        }
        args.push(
            static_cast<float>(read_big_endian<std::int32_t>(data + pos)));
        pos += 4;
        break;
      }
      case 'd': {
        if (pos + 8 > total) {
          return 0;
        }
        args.push(static_cast<float>(read_big_endian<double>(data + pos)));
        pos += 8;
        break;
      }
      case 'h': {
        if (pos + 8 > total) {
          return 0;
        }
        args.push(
            static_cast<float>(read_big_endian<std::int64_t>(data + pos)));
        pos += 8;
        break;
      }
      case 'T': {
        args.push(1.f);
        break;
      }
      case 'F': {
        args.push(0.f);
        break;
      }
      case 'N':
      case 'I': {
        break;  // tag-only, not numeric control
      }
      case 's':
      case 'S': {
        std::size_t len = 0;
        while (pos + len < total &&
               static_cast<char>(data[pos + len]) != '\0') {
          ++len;
        }
        if (pos + len == total) {
          return 0;
        }
        pos += padded4(len + 1);
        break;
      }
      case 'b': {
        if (pos + 4 > total) {
          return 0;
        }
        const std::int32_t size = read_big_endian<std::int32_t>(data + pos);
        pos += 4;
        if (size < 0 || pos + static_cast<std::size_t>(size) > total) {
          return 0;
        }
        pos += padded4(static_cast<std::size_t>(size));
        break;
      }
      case 't': {
        if (pos + 8 > total) {
          return 0;
        }
        pos += 8;
        break;
      }
      case 'c':
      case 'r':
      case 'm': {
        if (pos + 4 > total) {
          return 0;
        }
        pos += 4;
        break;
      }
      default:
        return 0;  // unknown tag: its size is unknowable, drop the message
    }
  }

  const auto* chars = reinterpret_cast<const char*>(data);
  const std::string_view name{chars + 1, addr_len - 1};
  const Hash base = hash_string(name);
  std::uint32_t written = 0;
  for (std::size_t k = 0; k < args.count; ++k) {
    if (!args.finite[k]) {
      continue;
    }
    bus.set(arg_slot_name(base, k), args.values[k]);
    ++written;
    if (observer != nullptr) {
      observer(context, name, static_cast<std::uint32_t>(k), args.values[k]);
    }
  }
  return written;
}

[[nodiscard]] std::uint32_t apply_packet(std::span<const std::byte> packet,
                                         ControlBus& bus,
                                         OscWriteObserver observer,
                                         void* context,
                                         int depth) {
  constexpr std::string_view Header{"#bundle\0", 8};
  if (packet.size() >= Header.size() &&
      std::memcmp(packet.data(), Header.data(), Header.size()) == 0) {
    if (depth == MaxBundleDepth || packet.size() < 16 ||
        packet.size() % 4 != 0) {
      return 0;
    }
    std::uint32_t written = 0;
    std::size_t pos = 16;  // past header and timetag
    while (pos < packet.size()) {
      if (pos + 4 > packet.size()) {
        return written;
      }
      const std::int32_t size =
          read_big_endian<std::int32_t>(packet.data() + pos);
      pos += 4;
      if (size < 0 || size % 4 != 0 ||
          pos + static_cast<std::size_t>(size) > packet.size()) {
        return written;
      }
      written +=
          apply_packet(packet.subspan(pos, static_cast<std::size_t>(size)), bus,
                       observer, context, depth + 1);
      pos += static_cast<std::size_t>(size);
    }
    return written;
  }
  return apply_message(packet, bus, observer, context);
}

}  // namespace

std::uint32_t apply_osc_packet(std::span<const std::byte> packet,
                               ControlBus& bus,
                               OscWriteObserver observer,
                               void* context) {
  return apply_packet(packet, bus, observer, context, 0);
}

}  // namespace automata
