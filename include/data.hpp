#pragma once

#include <arch.hpp>
#include <protocols.hpp>

#include <atomic>
#include <cstdint>

struct Node {
  std::atomic<std::uint8_t> state{0};
  alignas(hft::arch::cache_line_size) hft::proto::TaggedMessage message{};
};
