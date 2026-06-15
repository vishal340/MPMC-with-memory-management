#pragma once

#include <mem/os_memory.hpp>
#include <feed/shared_inlet.hpp>

namespace hft::feed {

[[nodiscard]] inline SharedInlet* map_inlet(os::Region& region) {
  region = os::map_shared(kSharedInletMappedSize, alignof(SharedInlet));
  return static_cast<SharedInlet*>(region.data());
}

} // namespace hft::feed
