#pragma once

#include <os_memory.hpp>
#include <shared_inlet.hpp>

namespace hft::feed {

[[nodiscard]] inline SharedInlet* map_inlet(os::Region& region) {
  region = os::map_shared(kSharedInletMappedSize, alignof(SharedInlet));
  return static_cast<SharedInlet*>(region.data());
}

} // namespace hft::feed
