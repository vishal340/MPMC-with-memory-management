#pragma once

#include <arch.hpp>
#include <os_memory.hpp>
#include <protocols.hpp>

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <type_traits>

namespace hft::mem {

namespace detail {

inline std::size_t index_of(const void *slot, const void *base,
                            std::size_t slot_stride) noexcept {
  const auto base_addr = reinterpret_cast<std::uintptr_t>(base);
  const auto slot_addr = reinterpret_cast<std::uintptr_t>(slot);
  return (slot_addr - base_addr) / slot_stride;
}

} // namespace detail

template <proto::WireMessage T, std::size_t Capacity>
  requires(Capacity > 0)
class TypedPool {
  struct alignas(arch::cache_line_size) Slot {
    alignas(alignof(T)) std::byte storage[sizeof(T)]{};
    std::atomic<std::uint8_t> state{0}; // 0 = free, 1 = acquired
  };

public:
  TypedPool() {
    constexpr std::size_t bytes = Capacity * sizeof(Slot);
    region_ = os::map(bytes, alignof(Slot));
    slots_ = static_cast<Slot *>(region_.data());

    for (std::size_t i = 0; i < Capacity; ++i) {
      slots_[i].state.store(0, std::memory_order_relaxed);
    }
  }

  TypedPool(const TypedPool &) = delete;
  TypedPool &operator=(const TypedPool &) = delete;

  [[nodiscard]] T *acquire() noexcept {
    for (std::size_t i = 0; i < Capacity; ++i) {
      std::uint8_t expected = 0;
      if (slots_[i].state.compare_exchange_weak(expected, 1,
                                                std::memory_order_acquire,
                                                std::memory_order_relaxed)) {
        return reinterpret_cast<T *>(slots_[i].storage);
      }
    }
    return nullptr;
  }

  void release(T *object) noexcept {
    if (object == nullptr) {
      return;
    }

    const std::size_t index = detail::index_of(object, slots_, sizeof(Slot));
    if (index >= Capacity) {
      return;
    }

    slots_[index].state.store(0, std::memory_order_release);
  }

  [[nodiscard]] std::size_t capacity() const noexcept { return Capacity; }
  [[nodiscard]] const os::Region &region() const noexcept { return region_; }

private:
  os::Region region_{};
  Slot *slots_{nullptr};
};

template <std::size_t ItchCap, std::size_t OuchCap, std::size_t MtbtCap,
          std::size_t NnfCap>
class ProtocolArena {
public:
  [[nodiscard]] proto::ItchAddOrder *acquire_itch() noexcept {
    return itch_.acquire();
  }
  void release(proto::ItchAddOrder *msg) noexcept { itch_.release(msg); }

  [[nodiscard]] proto::OuchEnterOrder *acquire_ouch() noexcept {
    return ouch_.acquire();
  }
  void release(proto::OuchEnterOrder *msg) noexcept { ouch_.release(msg); }

  [[nodiscard]] proto::MtbtNewOrder *acquire_mtbt() noexcept {
    return mtbt_.acquire();
  }
  void release(proto::MtbtNewOrder *msg) noexcept { mtbt_.release(msg); }

  [[nodiscard]] proto::NnfOrderEntry *acquire_nnf() noexcept {
    return nnf_.acquire();
  }
  void release(proto::NnfOrderEntry *msg) noexcept { nnf_.release(msg); }

  template <proto::WireMessage T> [[nodiscard]] T *acquire() noexcept {
    if constexpr (std::same_as<T, proto::ItchAddOrder>) {
      return acquire_itch();
    } else if constexpr (std::same_as<T, proto::OuchEnterOrder>) {
      return acquire_ouch();
    } else if constexpr (std::same_as<T, proto::MtbtNewOrder>) {
      return acquire_mtbt();
    } else if constexpr (std::same_as<T, proto::NnfOrderEntry>) {
      return acquire_nnf();
    } else {
      return nullptr;
    }
  }

  template <proto::WireMessage T> void release(T *msg) noexcept {
    if constexpr (std::same_as<T, proto::ItchAddOrder>) {
      itch_.release(msg);
    } else if constexpr (std::same_as<T, proto::OuchEnterOrder>) {
      ouch_.release(msg);
    } else if constexpr (std::same_as<T, proto::MtbtNewOrder>) {
      mtbt_.release(msg);
    } else if constexpr (std::same_as<T, proto::NnfOrderEntry>) {
      nnf_.release(msg);
    }
  }

  void copy_to_tagged(const proto::ItchAddOrder &src,
                      proto::TaggedMessage &dst) noexcept {
    dst.kind = proto::Kind::itch;
    std::memcpy(dst.bytes.data(), &src, sizeof(src));
  }

  void copy_to_tagged(const proto::OuchEnterOrder &src,
                      proto::TaggedMessage &dst) noexcept {
    dst.kind = proto::Kind::ouch;
    std::memcpy(dst.bytes.data(), &src, sizeof(src));
  }

  void copy_to_tagged(const proto::MtbtNewOrder &src,
                      proto::TaggedMessage &dst) noexcept {
    dst.kind = proto::Kind::mtbt;
    std::memcpy(dst.bytes.data(), &src, sizeof(src));
  }

  void copy_to_tagged(const proto::NnfOrderEntry &src,
                      proto::TaggedMessage &dst) noexcept {
    dst.kind = proto::Kind::nnf;
    std::memcpy(dst.bytes.data(), &src, sizeof(src));
  }

private:
  TypedPool<proto::ItchAddOrder, ItchCap> itch_{};
  TypedPool<proto::OuchEnterOrder, OuchCap> ouch_{};
  TypedPool<proto::MtbtNewOrder, MtbtCap> mtbt_{};
  TypedPool<proto::NnfOrderEntry, NnfCap> nnf_{};
};

} // namespace hft::mem
