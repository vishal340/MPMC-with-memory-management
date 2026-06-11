#pragma once

#include <arch.hpp>
#include <frame.hpp>

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstring>

#include <utils.hpp>

namespace hft::feed {

enum class Kind : std::uint8_t {
  none = 0,
  itch = 1,
  mtbt = 2,
};

enum class InletFlags : std::uint16_t {
  none = 0,
  lzo_compressed = 1 << 0,
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

// Layout written by an external producer (another process, FPGA shim, etc.).
// Writer stores payload + metadata, then publishes with sequence.fetch_add(1).
struct SharedInletHeader {
  static constexpr std::uint32_t kMagic = 0x48465449; // 'HFTI'

  std::atomic<std::uint64_t> sequence{0};
  std::atomic<std::uint64_t> processed{0};
  std::uint32_t magic{kMagic};
  std::uint32_t payload_len{0};
  Kind kind{Kind::none};
  InletFlags flags{InletFlags::none};
  std::uint32_t uncompressed_len{0};
  std::uint32_t reserved{0};
};

struct SharedInlet {
  SharedInletHeader header{};
  alignas(arch::cache_line_size)
      std::array<std::byte, frame::kCapacity> payload{};
};

inline constexpr std::size_t kSharedInletMappedSize = sizeof(SharedInlet);

inline void wait_until_processed(const SharedInlet& inlet,
                                 std::uint64_t sequence) noexcept {
  while (inlet.header.processed.load(std::memory_order_acquire) < sequence) {
    cpu_pause();
  }
}

inline void publish(SharedInlet& inlet, Kind kind, InletFlags flags,
                    const std::byte* data, std::size_t len,
                    std::uint32_t uncompressed_len = 0) noexcept {
  if (data == nullptr || len > inlet.payload.size()) {
    return;
  }

  inlet.header.magic = SharedInletHeader::kMagic;
  inlet.header.kind = kind;
  inlet.header.flags = flags;
  inlet.header.payload_len = static_cast<std::uint32_t>(len);
  inlet.header.uncompressed_len = uncompressed_len;
  std::memcpy(inlet.payload.data(), data, len);
  inlet.header.sequence.fetch_add(1, std::memory_order_release);
}

} // namespace hft::feed
