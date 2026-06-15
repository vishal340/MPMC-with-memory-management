#pragma once

#include <cstddef>

namespace hft::frame {

inline constexpr std::size_t kCapacity = 1500;

struct Stats {
  std::size_t saved{0};
  std::size_t skipped{0};
  std::size_t consumed{0};
};

} // namespace hft::frame
