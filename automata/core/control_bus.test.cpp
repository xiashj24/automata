#include <catch2/catch_test_macros.hpp>

#include "automata/core/control_bus.hpp"

#include <cstddef>

using namespace automata;

TEST_CASE("a name binds one slot, stable across lookups", "[control_bus]") {
  ControlBus bus;
  ControlBus::Slot* by_string = bus.channel("cutoff");
  REQUIRE(by_string != nullptr);
  CHECK(bus.channel("cutoff") == by_string);
  CHECK(bus.channel(hash_string("cutoff")) == by_string);
  CHECK(bus.channel("other") != by_string);
}

TEST_CASE("a slot falls back until written, then stays live", "[control_bus]") {
  ControlBus bus;
  ControlBus::Slot* slot = bus.channel("gain");
  REQUIRE(slot != nullptr);
  CHECK(slot->read_or(0.25f) == 0.25f);

  bus.set("gain", 0.75f);
  CHECK(slot->read_or(0.25f) == 0.75f);
  bus.set("gain", 0.5f);
  CHECK(slot->read_or(0.25f) == 0.5f);
}

TEST_CASE("a full bus drops writes and counts them", "[control_bus]") {
  ControlBus bus;
  for (std::size_t i = 0; i < ControlBusCapacity; ++i) {
    REQUIRE(bus.channel(Hash{i + 1}) != nullptr);
  }
  CHECK(bus.channel("one-more") == nullptr);
  CHECK(bus.dropped() == 0);

  bus.set("one-more", 1.f);
  CHECK(bus.dropped() == 1);

  // Names already bound keep working at capacity.
  bus.set(Hash{1}, 2.f);
  CHECK(bus.channel(Hash{1})->read_or(0.f) == 2.f);
  CHECK(bus.dropped() == 1);
}
