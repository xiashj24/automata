#include <catch2/catch_test_macros.hpp>

#include "automata/core/lockfree_queue.hpp"

#include <memory>
#include <thread>
#include <vector>

// The port's one adaptation (ADR 0003): indices on separate cache lines.
static_assert(alignof(automata::LockfreeQueue<int, 8>) >=
              automata::detail::DestructiveInterferenceSize);

TEST_CASE("lockfree queue starts empty with capacity N-1",
          "[core][lockfree_queue]") {
  automata::LockfreeQueue<int, 8> q;

  REQUIRE(q.empty());
  REQUIRE_FALSE(q.full());
  REQUIRE(q.size() == 0);
  REQUIRE(q.capacity() == 7);
}

TEST_CASE("try_push / try_pop preserve FIFO order", "[core][lockfree_queue]") {
  automata::LockfreeQueue<int, 8> q;

  for (int i = 1; i <= 3; ++i) {
    REQUIRE(q.try_push(i));
  }
  REQUIRE(q.size() == 3);

  int value = 0;
  REQUIRE(q.try_pop(value));
  REQUIRE(value == 1);
  REQUIRE(q.try_pop(value));
  REQUIRE(value == 2);
  REQUIRE(q.try_pop(value));
  REQUIRE(value == 3);
  REQUIRE(q.empty());
}

TEST_CASE("try_push fails when full, try_pop fails when empty",
          "[core][lockfree_queue]") {
  automata::LockfreeQueue<int, 4> q;

  int value = 0;
  REQUIRE_FALSE(q.try_pop(value));

  REQUIRE(q.try_push(1));
  REQUIRE(q.try_push(2));
  REQUIRE(q.try_push(3));
  REQUIRE(q.full());
  REQUIRE_FALSE(q.try_push(4));
  REQUIRE(q.size() == 3);
}

TEST_CASE("clear empties the queue for reuse", "[core][lockfree_queue]") {
  automata::LockfreeQueue<int, 4> q;
  REQUIRE(q.try_push(1));
  REQUIRE(q.try_push(2));

  q.clear();

  REQUIRE(q.empty());
  REQUIRE(q.size() == 0);
  REQUIRE(q.try_push(7));
}

TEST_CASE("indices stay consistent across many wrap-arounds",
          "[core][lockfree_queue]") {
  automata::LockfreeQueue<int, 4> q;

  int next = 0;
  int expected = 0;
  REQUIRE(q.try_push(next++));
  REQUIRE(q.try_push(next++));

  for (int i = 0; i < 100; ++i) {
    REQUIRE(q.try_push(next++));

    int value = -1;
    REQUIRE(q.try_pop(value));
    REQUIRE(value == expected++);
    REQUIRE(q.size() == 2);
  }
}

TEST_CASE("move-only types work through the try_* interface",
          "[core][lockfree_queue]") {
  automata::LockfreeQueue<std::unique_ptr<int>, 4> q;

  REQUIRE(q.try_push(std::make_unique<int>(1)));
  REQUIRE(q.try_push(std::make_unique<int>(2)));

  std::unique_ptr<int> value;
  REQUIRE(q.try_pop(value));
  REQUIRE(*value == 1);
  REQUIRE(q.try_pop(value));
  REQUIRE(*value == 2);
  REQUIRE(q.empty());
}

TEST_CASE(
    "a producer and consumer thread see every item exactly once, in order",
    "[core][lockfree_queue]") {
  constexpr int ItemCount = 50'000;
  automata::LockfreeQueue<int, 256> q;

  std::vector<int> received;
  received.reserve(ItemCount);

  std::thread producer([&q] {
    for (int i = 0; i < ItemCount; ++i) {
      while (!q.try_push(i)) {
        std::this_thread::yield();
      }
    }
  });

  std::thread consumer([&q, &received] {
    int value = 0;
    int count = 0;
    while (count < ItemCount) {
      if (q.try_pop(value)) {
        received.push_back(value);
        ++count;
      } else {
        std::this_thread::yield();
      }
    }
  });

  producer.join();
  consumer.join();

  REQUIRE(received.size() == static_cast<size_t>(ItemCount));

  bool in_order = true;
  for (int i = 0; i < ItemCount; ++i) {
    if (received[static_cast<size_t>(i)] != i) {
      in_order = false;
      break;
    }
  }
  REQUIRE(in_order);
}
