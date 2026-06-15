#pragma once

#include <arch.hpp>
#include <protocols.hpp>

#include <atomic>
#include <cstdint>

struct Node {
  hft::proto::TaggedMessage message{};
  std::atomic<std::uint8_t> state{0};
};
