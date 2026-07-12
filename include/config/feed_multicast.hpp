#pragma once

#include <cstdint>

namespace hft::config::feed {

struct MulticastFeed {
  const char *group{};
  std::uint16_t port{};
  std::uint8_t ttl{1};
};

// Lab / replay defaults — replace with venue-specific groups in production.
inline constexpr MulticastFeed kItch{"239.255.1.1", 30001, 1};
inline constexpr MulticastFeed kMtbt{"239.255.1.2", 30002, 1};
inline constexpr MulticastFeed kMcx{"239.255.1.3", 30003, 1};

inline constexpr int kPollTimeoutMs = 50;
inline constexpr bool kMulticastLoopback = true;

} // namespace hft::config::feed
