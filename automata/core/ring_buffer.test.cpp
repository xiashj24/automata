#include <catch2/catch_test_macros.hpp>

#include "automata/core/ring_buffer.hpp"

#include <algorithm>
#include <memory>

TEST_CASE("ring buffer starts empty with capacity N-1", "[core][ring_buffer]") {
  automata::RingBuffer<int, 8> rb;

  REQUIRE(rb.empty());
  REQUIRE_FALSE(rb.full());
  REQUIRE(rb.size() == 0);
  REQUIRE(rb.capacity() == 7);  // one slot of the 2^N storage is sacrificed
}

TEST_CASE("try_push / try_pop preserve FIFO order", "[core][ring_buffer]") {
  automata::RingBuffer<int, 8> rb;

  for (int i = 1; i <= 3; ++i) {
    REQUIRE(rb.try_push(i));
  }

  REQUIRE(rb.size() == 3);

  int value = 0;
  REQUIRE(rb.try_pop(value));
  REQUIRE(value == 1);
  REQUIRE(rb.try_pop(value));
  REQUIRE(value == 2);
  REQUIRE(rb.try_pop(value));
  REQUIRE(value == 3);
  REQUIRE(rb.empty());
}

TEST_CASE("try_push fails when full, try_pop fails when empty",
          "[core][ring_buffer]") {
  automata::RingBuffer<int, 4> rb;

  int value = 0;
  REQUIRE_FALSE(rb.try_pop(value));

  REQUIRE(rb.try_push(1));
  REQUIRE(rb.try_push(2));
  REQUIRE(rb.try_push(3));
  REQUIRE(rb.full());
  REQUIRE_FALSE(rb.try_push(4));
  REQUIRE(rb.size() == 3);
}

TEST_CASE("push overwrites the oldest element when full",
          "[core][ring_buffer]") {
  automata::RingBuffer<int, 4> rb;

  SECTION("copy overload") {
    for (int i = 1; i <= 5; ++i) {
      rb.push(i);
    }

    REQUIRE(rb.full());
    REQUIRE(rb.front() == 3);
    REQUIRE(rb.back() == 5);
  }

  SECTION("move overload") {
    for (int i = 1; i <= 5; ++i) {
      rb.push(i + 0);  // rvalue
    }

    REQUIRE(rb.full());
    REQUIRE(rb.front() == 3);
    REQUIRE(rb.back() == 5);
  }
}

TEST_CASE("pop returns the value or nullopt", "[core][ring_buffer]") {
  automata::RingBuffer<int, 4> rb;

  REQUIRE_FALSE(rb.pop().has_value());

  rb.push(42);
  auto popped = rb.pop();
  REQUIRE(popped.has_value());
  REQUIRE(*popped == 42);
  REQUIRE_FALSE(rb.pop().has_value());
}

TEST_CASE("front, back and operator[] access queued elements",
          "[core][ring_buffer]") {
  automata::RingBuffer<int, 8> rb;
  rb.push(10);
  rb.push(20);
  rb.push(30);

  REQUIRE(rb.front() == 10);
  REQUIRE(rb.back() == 30);
  REQUIRE(rb[0] == 10);
  REQUIRE(rb[1] == 20);
  REQUIRE(rb[2] == 30);

  rb[1] = 25;
  REQUIRE(rb[1] == 25);

  const auto& crb = rb;
  REQUIRE(crb.front() == 10);
  REQUIRE(crb.back() == 30);
  REQUIRE(crb[1] == 25);
}

TEST_CASE("clear empties the buffer for reuse", "[core][ring_buffer]") {
  automata::RingBuffer<int, 4> rb;
  rb.push(1);
  rb.push(2);

  rb.clear();

  REQUIRE(rb.empty());
  REQUIRE(rb.size() == 0);
  REQUIRE(rb.try_push(7));
  REQUIRE(rb.front() == 7);
}

TEST_CASE("indices stay consistent across many wrap-arounds",
          "[core][ring_buffer]") {
  automata::RingBuffer<int, 4> rb;

  // Steady-state occupancy of 2 while the head/tail counters advance far
  // past N, so the masked indices wrap many times.
  int next = 0;
  int expected = 0;
  REQUIRE(rb.try_push(next++));
  REQUIRE(rb.try_push(next++));

  for (int i = 0; i < 100; ++i) {
    REQUIRE(rb.try_push(next++));

    int value = -1;
    REQUIRE(rb.try_pop(value));
    REQUIRE(value == expected++);
    REQUIRE(rb.size() == 2);
  }
}

TEST_CASE("iterators traverse in FIFO order and support random access",
          "[core][ring_buffer]") {
  automata::RingBuffer<int, 8> rb;

  // Advance head first so iteration crosses the physical wrap boundary.
  for (int i = 0; i < 5; ++i) {
    rb.push(-1);
    (void)rb.pop();
  }
  for (int i = 1; i <= 6; ++i) {
    rb.push(i * 10);
  }

  int expected = 10;
  for (int value : rb) {
    REQUIRE(value == expected);
    expected += 10;
  }

  auto it = rb.begin();
  REQUIRE(*(it + 2) == 30);
  REQUIRE(it[4] == 50);
  REQUIRE(rb.end() - rb.begin() == 6);
  REQUIRE(rb.begin() < rb.end());

  const auto& crb = rb;
  REQUIRE(std::distance(crb.cbegin(), crb.cend()) == 6);
  REQUIRE(*crb.begin() == 10);
}

TEST_CASE("std::sort works across the wrap boundary", "[core][ring_buffer]") {
  automata::RingBuffer<int, 8> rb;

  for (int i = 0; i < 6; ++i) {
    rb.push(-1);
    (void)rb.pop();
  }
  for (int value : {4, 1, 7, 3, 6, 2, 5}) {
    rb.push(value);
  }

  std::sort(rb.begin(), rb.end());

  for (size_t i = 0; i < rb.size(); ++i) {
    REQUIRE(rb[i] == static_cast<int>(i) + 1);
  }
}

TEST_CASE("erase removes a single element and preserves order",
          "[core][ring_buffer]") {
  automata::RingBuffer<int, 8> rb;
  for (int i = 1; i <= 5; ++i) {
    rb.push(i);
  }

  auto it = rb.erase(rb.begin() + 1);  // remove 2

  REQUIRE(*it == 3);
  REQUIRE(rb.size() == 4);
  REQUIRE(rb[0] == 1);
  REQUIRE(rb[1] == 3);
  REQUIRE(rb[2] == 4);
  REQUIRE(rb[3] == 5);

  REQUIRE(rb.erase(rb.end()) == rb.end());
  REQUIRE(rb.size() == 4);
}

TEST_CASE("erase removes a range, including across the wrap boundary",
          "[core][ring_buffer]") {
  automata::RingBuffer<int, 8> rb;

  for (int i = 0; i < 6; ++i) {
    rb.push(-1);
    (void)rb.pop();
  }
  for (int i = 1; i <= 7; ++i) {
    rb.push(i);
  }

  auto it = rb.erase(rb.begin() + 2, rb.begin() + 5);  // remove 3,4,5

  REQUIRE(*it == 6);
  REQUIRE(rb.size() == 4);
  REQUIRE(rb[0] == 1);
  REQUIRE(rb[1] == 2);
  REQUIRE(rb[2] == 6);
  REQUIRE(rb[3] == 7);
}

TEST_CASE("move-only types work through the try_* interface",
          "[core][ring_buffer]") {
  automata::RingBuffer<std::unique_ptr<int>, 4> rb;

  REQUIRE(rb.try_push(std::make_unique<int>(1)));
  REQUIRE(rb.try_push(std::make_unique<int>(2)));

  std::unique_ptr<int> value;
  REQUIRE(rb.try_pop(value));
  REQUIRE(*value == 1);

  auto popped = rb.pop();
  REQUIRE(popped.has_value());
  REQUIRE(**popped == 2);
  REQUIRE(rb.empty());
}
