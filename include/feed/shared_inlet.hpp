#pragma once

#include <core/arch.hpp>
#include <feed/frame.hpp>

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstring>

#include <core/utils.hpp>

namespace hft::feed {

enum class Kind : std::uint8_t {
  none = 0,
  itch = 1,
  mtbt = 2,
  mcx = 3,
};

enum class InletFlags : std::uint16_t {
  none = 0,
  lz4_compressed = 1 << 0,
};

constexpr InletFlags operator|(InletFlags lhs, InletFlags rhs) noexcept {
  return static_cast<InletFlags>(static_cast<std::uint16_t>(lhs) |
                                 static_cast<std::uint16_t>(rhs));
}

constexpr InletFlags operator&(InletFlags lhs, InletFlags rhs) noexcept {
  return static_cast<InletFlags>(static_cast<std::uint16_t>(lhs) &
                                 static_cast<std::uint16_t>(rhs));
}

constexpr bool has_flag(InletFlags flags, InletFlags flag) noexcept {
  return (flags & flag) == flag;
}

// Plain frame body — never accessed through atomics.
struct FrameSlot {
  static constexpr std::uint32_t kMagic = 0x48465449; // 'HFTI'

  std::uint32_t magic{kMagic};
  std::uint32_t payload_len{0};
  Kind kind{Kind::none};
  InletFlags flags{InletFlags::none};
  std::uint32_t uncompressed_len{0};
  std::uint32_t reserved{0};
  alignas(arch::cache_line_size)
      std::array<std::byte, frame::kCapacity> payload{};
};

// Double-buffered inlet: writer fills the inactive slot, then publishes an
// atomic index. Readers load that index and read the plain FrameSlot.
struct SharedInlet {
  static constexpr std::size_t kSlots = 2;

  std::atomic<std::uint64_t> sequence{0};
  std::atomic<std::uint64_t> processed{0};
  // Index of the slot that is safe to read after observing sequence.
  std::atomic<std::uint32_t> published{0};
  std::array<FrameSlot, kSlots> slots{};
};

inline constexpr std::size_t kSharedInletMappedSize = sizeof(SharedInlet);

inline void wait_until_processed(const SharedInlet &inlet,
                                 std::uint64_t sequence) noexcept {
  while (inlet.processed.load(std::memory_order_acquire) < sequence) {
    cpu_pause();
  }
}

[[nodiscard]] inline const FrameSlot *
published_slot(const SharedInlet &inlet) noexcept {
  const std::uint32_t index =
      inlet.published.load(std::memory_order_acquire) % SharedInlet::kSlots;
  return &inlet.slots[index];
}

inline void publish(SharedInlet &inlet, Kind kind, InletFlags flags,
                    const std::byte *data, std::size_t len,
                    std::uint32_t uncompressed_len = 0) noexcept {
  if (data == nullptr || len > frame::kCapacity) {
    return;
  }

  const std::uint32_t current =
      inlet.published.load(std::memory_order_relaxed) % SharedInlet::kSlots;
  const std::uint32_t write = 1U - current;
  FrameSlot &slot = inlet.slots[write];

  slot.magic = FrameSlot::kMagic;
  slot.kind = kind;
  slot.flags = flags;
  slot.payload_len = static_cast<std::uint32_t>(len);
  slot.uncompressed_len = uncompressed_len;
  slot.reserved = 0;
  std::memcpy(slot.payload.data(), data, len);

  inlet.published.store(write, std::memory_order_release);
  inlet.sequence.fetch_add(1, std::memory_order_release);
}

} // namespace hft::feed
