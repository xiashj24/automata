#pragma once
#include <cstddef>
#include <array>
#include <algorithm>
#include <optional>
#include "automata/core/assert.hpp"

/**
 * @brief static fixed-size ring buffer, support random access iterators
 * @note N must be a power of 2
 * @note single-context only: head_/tail_ are plain, non-atomic counters, so
 *   this type is not safe to share between two concurrent contexts — use
 *   automata::LockfreeQueue (automata/core/lockfree_queue.hpp, ADR 0003)
 *   for that case instead.
 */

namespace automata {

template <typename T, size_t N>
class RingBuffer {
  static_assert((N > 1) && ((N & (N - 1)) == 0), "N must be power of 2");

public:
  [[nodiscard]] bool try_push(const T& value) noexcept(
      std::is_nothrow_copy_assignable_v<T>) {
    if (tail_ - head_ >= Capacity)  // full
      return false;

    data_[tail_++ & Capacity] = value;
    return true;
  }

  [[nodiscard]] bool try_push(T&& value) noexcept(
      std::is_nothrow_move_assignable_v<T>) {
    if (tail_ - head_ >= Capacity)  // full
      return false;

    data_[tail_++ & Capacity] = std::move(value);
    return true;
  }

  // silently overwrites earliest element if full
  void push(const T& value) noexcept(std::is_nothrow_copy_assignable_v<T>) {
    if (!try_push(value)) {
      T discarded;
      (void)try_pop(discarded);
      (void)try_push(value);
    }
  }

  // silently overwrites earliest element if full
  void push(T&& value) noexcept(std::is_nothrow_move_assignable_v<T>) {
    if (full()) {
      T discarded;
      (void)try_pop(discarded);
    }
    (void)try_push(std::move(value));
  }

  [[nodiscard]] bool try_pop(T& value) noexcept(
      std::is_nothrow_move_assignable_v<T>) {
    if (head_ == tail_)  // empty
      return false;

    value = std::move(data_[head_++ & Capacity]);
    return true;
  }

  [[nodiscard]] std::optional<T> pop() noexcept(
      std::is_nothrow_move_assignable_v<T>) {
    T value;
    if (!try_pop(value))
      return std::nullopt;
    return value;
  }

  [[nodiscard]] size_t size() const noexcept { return tail_ - head_; }
  [[nodiscard]] size_t capacity() const noexcept { return Capacity; }
  [[nodiscard]] bool full() const noexcept { return tail_ - head_ >= Capacity; }
  [[nodiscard]] bool empty() const noexcept { return head_ == tail_; }

  void clear() noexcept { head_ = tail_ = 0; }

  [[nodiscard]] T& front() noexcept {
    atm_assert(!empty());
    return data_[head_ & Capacity];
  }
  [[nodiscard]] const T& front() const noexcept {
    atm_assert(!empty());
    return data_[head_ & Capacity];
  }

  [[nodiscard]] T& back() noexcept {
    atm_assert(!empty());
    return data_[(tail_ - 1) & Capacity];
  }
  [[nodiscard]] const T& back() const noexcept {
    atm_assert(!empty());
    return data_[(tail_ - 1) & Capacity];
  }

  // no bounds checking in release: idx must be < size()
  [[nodiscard]] T& operator[](size_t idx) noexcept {
    atm_assert(idx < size());
    return data_[(head_ + idx) & Capacity];
  }
  [[nodiscard]] const T& operator[](size_t idx) const noexcept {
    atm_assert(idx < size());
    return data_[(head_ + idx) & Capacity];
  }

  class iterator {
  public:
    using iterator_category = std::random_access_iterator_tag;
    using value_type = T;
    using difference_type = std::ptrdiff_t;
    using pointer = T*;
    using reference = T&;

    iterator() noexcept : buf_(nullptr), pos_(0) {}
    iterator(RingBuffer* buf, size_t pos) noexcept : buf_(buf), pos_(pos) {}

    // const members returning T&: a const iterator object still designates a
    // mutable element (like T* const), which std algorithms rely on
    T& operator*() const noexcept { return buf_->data_[pos_ & Capacity]; }
    T* operator->() const noexcept { return &buf_->data_[pos_ & Capacity]; }
    T& operator[](difference_type offset) const noexcept {
      return buf_->data_[(pos_ + static_cast<size_t>(offset)) & Capacity];
    }

    iterator& operator++() noexcept {
      ++pos_;
      return *this;
    }
    iterator operator++(int) noexcept {
      iterator tmp = *this;
      ++pos_;
      return tmp;
    }
    iterator& operator--() noexcept {
      --pos_;
      return *this;
    }
    iterator operator--(int) noexcept {
      iterator tmp = *this;
      --pos_;
      return tmp;
    }
    iterator& operator+=(difference_type offset) noexcept {
      pos_ += static_cast<size_t>(offset);
      return *this;
    }
    iterator& operator-=(difference_type offset) noexcept {
      pos_ -= static_cast<size_t>(offset);
      return *this;
    }

    iterator operator+(difference_type offset) const noexcept {
      return {buf_, pos_ + static_cast<size_t>(offset)};
    }
    iterator operator-(difference_type offset) const noexcept {
      return {buf_, pos_ - static_cast<size_t>(offset)};
    }
    difference_type operator-(const iterator& other) const noexcept {
      return static_cast<difference_type>(pos_) -
             static_cast<difference_type>(other.pos_);
    }

    friend iterator operator+(difference_type offset,
                              const iterator& it) noexcept {
      return it + offset;
    }

    bool operator==(const iterator& other) const noexcept {
      return buf_ == other.buf_ && pos_ == other.pos_;
    }
    bool operator!=(const iterator& other) const noexcept {
      return !(*this == other);
    }
    bool operator<(const iterator& other) const noexcept {
      return pos_ < other.pos_;
    }
    bool operator>(const iterator& other) const noexcept {
      return pos_ > other.pos_;
    }
    bool operator<=(const iterator& other) const noexcept {
      return pos_ <= other.pos_;
    }
    bool operator>=(const iterator& other) const noexcept {
      return pos_ >= other.pos_;
    }

  private:
    RingBuffer* buf_;
    size_t pos_;
  };

  class const_iterator {
  public:
    using iterator_category = std::random_access_iterator_tag;
    using value_type = T;
    using difference_type = std::ptrdiff_t;
    using pointer = const T*;
    using reference = const T&;

    const_iterator() noexcept : buf_(nullptr), pos_(0) {}
    const_iterator(const RingBuffer* buf, size_t pos) noexcept
        : buf_(buf), pos_(pos) {}

    const T& operator*() const noexcept { return buf_->data_[pos_ & Capacity]; }
    const T* operator->() const noexcept {
      return &buf_->data_[pos_ & Capacity];
    }
    const T& operator[](difference_type offset) const noexcept {
      return buf_->data_[(pos_ + offset) & Capacity];
    }

    const_iterator& operator++() noexcept {
      ++pos_;
      return *this;
    }
    const_iterator operator++(int) noexcept {
      const_iterator tmp = *this;
      ++pos_;
      return tmp;
    }
    const_iterator& operator--() noexcept {
      --pos_;
      return *this;
    }
    const_iterator operator--(int) noexcept {
      const_iterator tmp = *this;
      --pos_;
      return tmp;
    }
    const_iterator& operator+=(difference_type offset) noexcept {
      pos_ += offset;
      return *this;
    }
    const_iterator& operator-=(difference_type offset) noexcept {
      pos_ -= offset;
      return *this;
    }

    const_iterator operator+(difference_type offset) const noexcept {
      return {buf_, pos_ + offset};
    }
    const_iterator operator-(difference_type offset) const noexcept {
      return {buf_, pos_ - offset};
    }
    difference_type operator-(const const_iterator& other) const noexcept {
      return static_cast<difference_type>(pos_) -
             static_cast<difference_type>(other.pos_);
    }

    friend const_iterator operator+(difference_type offset,
                                    const const_iterator& it) noexcept {
      return it + offset;
    }

    bool operator==(const const_iterator& other) const noexcept {
      return buf_ == other.buf_ && pos_ == other.pos_;
    }
    bool operator!=(const const_iterator& other) const noexcept {
      return !(*this == other);
    }
    bool operator<(const const_iterator& other) const noexcept {
      return pos_ < other.pos_;
    }
    bool operator>(const const_iterator& other) const noexcept {
      return pos_ > other.pos_;
    }
    bool operator<=(const const_iterator& other) const noexcept {
      return pos_ <= other.pos_;
    }
    bool operator>=(const const_iterator& other) const noexcept {
      return pos_ >= other.pos_;
    }

  private:
    const RingBuffer* buf_;
    size_t pos_;
  };

  [[nodiscard]] iterator begin() noexcept { return {this, head_}; }
  [[nodiscard]] iterator end() noexcept { return {this, tail_}; }
  [[nodiscard]] const_iterator begin() const noexcept { return {this, head_}; }
  [[nodiscard]] const_iterator end() const noexcept { return {this, tail_}; }
  [[nodiscard]] const_iterator cbegin() const noexcept { return begin(); }
  [[nodiscard]] const_iterator cend() const noexcept { return end(); }

  iterator erase(iterator pos) noexcept(std::is_nothrow_move_assignable_v<T>) {
    if (pos == end())
      return end();

    std::move(pos + 1, end(), pos);
    --tail_;
    return pos;
  }

  iterator erase(iterator first,
                 iterator last) noexcept(std::is_nothrow_move_assignable_v<T>) {
    std::move(last, end(), first);
    tail_ -= static_cast<size_t>(last - first);
    return first;
  }

private:
  static constexpr size_t Capacity = N - 1;  // also used as bitmask
  std::array<T, N> data_{};

  size_t head_{0};
  size_t tail_{0};
};

}  // namespace automata
