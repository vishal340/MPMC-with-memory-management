#pragma once

#include <core/arch.hpp>
#include <proto/protocols.hpp>

#include <atomic>

namespace hft {

// Ring slot publishes ownership of a pooled TaggedMessage via an atomic pointer.
// nullptr means the slot is empty. Payload bytes are never atomic.
struct Node {
  alignas(arch::cache_line_size) std::atomic<proto::TaggedMessage *> message{
      nullptr};
};

} // namespace hft
