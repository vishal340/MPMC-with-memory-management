#include <arch.hpp>
#include <memory_pool.hpp>
#include <mpmc.hpp>
#include <protocols.hpp>

#include <cstdio>
#include <cstring>

int main() {
  std::printf("target arch: %s (x86_64=%d arm64=%d page=%zu)\n",
              hft::arch::name, hft::arch::is_x86_64, hft::arch::is_arm64,
              hft::arch::page_size);

  hft::mem::ProtocolArena<1024, 1024, 1024, 256> arena;

  hft::MPMC<4096> queue;

  auto* itch = arena.acquire_itch();
  if (itch == nullptr) {
    return 1;
  }
  itch->stock_locate = 42;
  itch->shares = 100;
  std::memcpy(itch->stock, "AAPL    ", 8);

  hft::proto::TaggedMessage tagged{};
  arena.copy_to_tagged(*itch, tagged);
  arena.release(itch);

  queue.push(0, tagged);
  const hft::proto::TaggedMessage out = queue.pop(0);

  if (out.kind != hft::proto::Kind::itch) {
    return 2;
  }

  hft::proto::ItchAddOrder decoded{};
  std::memcpy(&decoded, out.bytes.data(), sizeof(decoded));
  if (decoded.stock_locate != 42 || decoded.shares != 100) {
    return 3;
  }

  std::printf("os-backed pool + mpmc ok: itch locate=%u shares=%u\n",
              decoded.stock_locate, decoded.shares);
  return 0;
}
