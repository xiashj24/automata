#pragma once

#include <cstddef>
#include <cstdint>

// Compile-time engine constants (ADR 0003, ADR 0004). Every audio-thread
// capacity in the system is fixed here — nothing on that thread is sized at
// runtime. Queue depths must be powers of two (LockfreeQueue contract).

namespace automata {

inline constexpr std::uint32_t SampleRate = 48'000;
inline constexpr std::size_t BlockSize = 128;

inline constexpr std::size_t MaxGraphNodes = 1024;

// Audio↔control SPSC channel depths. The outbox is deeper: it also carries
// log records, and dropping a retirement would leak.
inline constexpr std::size_t InboxCapacity = 64;
inline constexpr std::size_t OutboxCapacity = 256;

inline constexpr std::size_t ControlBusCapacity = 256;

// Per-tap delay buffer (power of two): ~2.7 s at 48 kHz.
inline constexpr std::size_t TapBufferSamples = std::size_t{1} << 17;

// Reconciliation defaults (ADR 0002): value edits glide, swaps crossfade.
inline constexpr float DefaultValueRampSeconds = 0.010f;
inline constexpr float DefaultCrossfadeSeconds = 0.050f;

}  // namespace automata
