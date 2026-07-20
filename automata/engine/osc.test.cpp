#include <catch2/catch_test_macros.hpp>

#include "automata/engine/osc.hpp"

#include <bit>
#include <cstdint>
#include <limits>
#include <string_view>
#include <type_traits>
#include <vector>

using namespace automata;

namespace {

// Wire-format builders: OSC pads strings (and blobs) to 4 bytes and writes
// numbers big-endian.
void put_padded_string(std::vector<std::byte>& p, std::string_view text) {
  for (const char c : text) {
    p.push_back(static_cast<std::byte>(c));
  }
  p.push_back(std::byte{0});
  while (p.size() % 4 != 0) {
    p.push_back(std::byte{0});
  }
}

template <typename T>
void put_big_endian(std::vector<std::byte>& p, T value) {
  using U = std::conditional_t<sizeof(T) == 4, std::uint32_t, std::uint64_t>;
  // Value-based shifts emit the numeric high byte first: big-endian on any
  // host.
  const U raw = std::bit_cast<U>(value);
  for (std::size_t i = 0; i < sizeof(U); ++i) {
    p.push_back(
        static_cast<std::byte>((raw >> (8 * (sizeof(U) - 1 - i))) & 0xFF));
  }
}

std::vector<std::byte> message(std::string_view address,
                               std::string_view tags) {
  std::vector<std::byte> p;
  put_padded_string(p, address);
  std::vector<char> tag_string{','};
  tag_string.insert(tag_string.end(), tags.begin(), tags.end());
  put_padded_string(p, {tag_string.data(), tag_string.size()});
  return p;
}

std::vector<std::byte> float_message(std::string_view address, float value) {
  auto p = message(address, "f");
  put_big_endian(p, value);
  return p;
}

std::vector<std::byte> bundle(
    const std::vector<std::vector<std::byte>>& elements) {
  std::vector<std::byte> p;
  put_padded_string(p, "#bundle");
  put_big_endian(p, std::uint64_t{1});  // immediate timetag
  for (const auto& e : elements) {
    put_big_endian(p, static_cast<std::int32_t>(e.size()));
    p.insert(p.end(), e.begin(), e.end());
  }
  return p;
}

float read(ControlBus& bus, std::string_view name) {
  return bus.channel(name)->read_or(std::numeric_limits<float>::quiet_NaN());
}

}  // namespace

TEST_CASE("a float message lands on the slot the address names", "[osc]") {
  ControlBus bus;
  CHECK(apply_osc_packet(float_message("/cutoff", 800.f), bus) == 1);
  CHECK(read(bus, "cutoff") == 800.f);

  // Nested address segments are just part of the name.
  CHECK(apply_osc_packet(float_message("/synth/freq", 440.f), bus) == 1);
  CHECK(read(bus, "synth/freq") == 440.f);
}

TEST_CASE("numeric tags convert: int, double, int64, true, false", "[osc]") {
  ControlBus bus;
  auto p = message("/v", "idhTF");
  put_big_endian(p, std::int32_t{-7});
  put_big_endian(p, 2.5);
  put_big_endian(p, std::int64_t{3});
  REQUIRE(apply_osc_packet(p, bus) == 5);
  CHECK(read(bus, "v") == -7.f);
  CHECK(read(bus, "v/1") == 2.5f);
  CHECK(read(bus, "v/2") == 3.f);
  CHECK(read(bus, "v/3") == 1.f);
  CHECK(read(bus, "v/4") == 0.f);
}

TEST_CASE("extra numeric args write name/1, name/2, and so on", "[osc]") {
  ControlBus bus;
  auto p = message("/pad", "ff");
  put_big_endian(p, 0.3f);
  put_big_endian(p, 0.7f);
  REQUIRE(apply_osc_packet(p, bus) == 2);
  CHECK(read(bus, "pad") == 0.3f);
  CHECK(read(bus, "pad/1") == 0.7f);
}

TEST_CASE("non-numeric args are skipped but keep the stream aligned", "[osc]") {
  ControlBus bus;
  // A string before the float: the float is still numeric arg 0.
  auto p = message("/named", "sf");
  put_padded_string(p, "label");
  put_big_endian(p, 5.f);
  REQUIRE(apply_osc_packet(p, bus) == 1);
  CHECK(read(bus, "named") == 5.f);

  // A blob (padded odd size) before the float.
  auto q = message("/blobbed", "bf");
  put_big_endian(q, std::int32_t{5});
  q.insert(q.end(), 5, std::byte{0xAB});
  while (q.size() % 4 != 0) {
    q.push_back(std::byte{0});
  }
  put_big_endian(q, 6.f);
  REQUIRE(apply_osc_packet(q, bus) == 1);
  CHECK(read(bus, "blobbed") == 6.f);
}

TEST_CASE("a non-finite float is dropped without shifting ordinals", "[osc]") {
  ControlBus bus;
  auto p = message("/pad", "ff");
  put_big_endian(p, std::numeric_limits<float>::quiet_NaN());
  put_big_endian(p, 0.7f);
  CHECK(apply_osc_packet(p, bus) == 1);
  // The y write stays at pad/1; pad itself was never written.
  CHECK(bus.channel("pad")->read_or(-1.f) == -1.f);
  CHECK(read(bus, "pad/1") == 0.7f);
}

TEST_CASE("malformed packets write nothing", "[osc]") {
  ControlBus bus;

  const auto rejects = [&](std::span<const std::byte> packet) {
    return apply_osc_packet(packet, bus) == 0;
  };

  CHECK(rejects({}));

  // Not an address (SSDP junk arrives on open UDP ports).
  std::vector<std::byte> junk;
  put_padded_string(junk, "M-SEARCH * HTTP/1.1");
  CHECK(rejects(junk));

  // Address without a type-tag string.
  std::vector<std::byte> untagged;
  put_padded_string(untagged, "/x");
  CHECK(rejects(untagged));

  // Type tags not starting with ','.
  std::vector<std::byte> commaless;
  put_padded_string(commaless, "/x");
  put_padded_string(commaless, "f");
  CHECK(rejects(commaless));

  // Truncated payload: 'f' promised, no bytes follow.
  CHECK(rejects(message("/x", "f")));

  // Not a multiple of four.
  auto ragged = float_message("/x", 1.f);
  ragged.pop_back();
  CHECK(rejects(ragged));

  // Unknown tag rejects the whole message, valid float notwithstanding.
  auto unknown = message("/x", "fq");
  put_big_endian(unknown, 1.f);
  CHECK(rejects(unknown));

  // Blob whose declared size overruns the packet.
  auto blob = message("/x", "b");
  put_big_endian(blob, std::int32_t{64});
  CHECK(rejects(blob));

  CHECK(bus.channel("x")->read_or(-1.f) == -1.f);
  CHECK(bus.dropped() == 0);
}

TEST_CASE("bundles unpack recursively, timetags ignored", "[osc]") {
  ControlBus bus;
  const auto inner = bundle({float_message("/b", 2.f)});
  const auto outer =
      bundle({float_message("/a", 1.f), inner, float_message("/c", 3.f)});
  REQUIRE(apply_osc_packet(outer, bus) == 3);
  CHECK(read(bus, "a") == 1.f);
  CHECK(read(bus, "b") == 2.f);
  CHECK(read(bus, "c") == 3.f);
}

TEST_CASE("a malformed bundle keeps only the elements before the damage",
          "[osc]") {
  ControlBus bus;
  auto p = bundle({float_message("/good", 1.f)});
  put_big_endian(p, std::int32_t{100});  // truncated second element
  CHECK(apply_osc_packet(p, bus) == 1);
  CHECK(read(bus, "good") == 1.f);

  // A bare or short bundle header is rejected outright.
  std::vector<std::byte> header;
  put_padded_string(header, "#bundle");
  CHECK(apply_osc_packet(header, bus) == 0);
}
