#pragma once

#include <config/feed_multicast.hpp>
#include <feed/shared_inlet.hpp>
#include <net/udp_multicast.hpp>

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <thread>

#include <poll.h>

namespace hft::feed {

enum class MulticastChannel : std::uint8_t {
  itch = 0,
  mtbt = 1,
  mcx = 2,
};

class MulticastHub {
public:
  MulticastHub() = default;
  ~MulticastHub() { stop(); }

  MulticastHub(const MulticastHub &) = delete;
  MulticastHub &operator=(const MulticastHub &) = delete;

  [[nodiscard]] bool start(SharedInlet *inlet) noexcept {
    stop();
    if (inlet == nullptr) {
      return false;
    }

    if (!open_channel(MulticastChannel::itch, config::feed::kItch) ||
        !open_channel(MulticastChannel::mtbt, config::feed::kMtbt) ||
        !open_channel(MulticastChannel::mcx, config::feed::kMcx)) {
      stop();
      return false;
    }

    inlet_ = inlet;
    running_.store(true, std::memory_order_release);
    thread_ = std::thread([this] { run(); });
    return true;
  }

  void stop() {
    running_.store(false, std::memory_order_release);
    if (thread_.joinable()) {
      thread_.join();
    }
    for (auto &socket : sockets_) {
      socket.close();
    }
    inlet_ = nullptr;
  }

  [[nodiscard]] std::uint64_t
  datagrams(MulticastChannel channel) const noexcept {
    switch (channel) {
    case MulticastChannel::itch:
      return itch_datagrams_.load(std::memory_order_acquire);
    case MulticastChannel::mtbt:
      return mtbt_datagrams_.load(std::memory_order_acquire);
    case MulticastChannel::mcx:
      return mcx_datagrams_.load(std::memory_order_acquire);
    }
    return 0;
  }

private:
  [[nodiscard]] bool open_channel(MulticastChannel channel,
                                  const config::feed::MulticastFeed &feed) noexcept {
    hft::net::MulticastEndpoint endpoint{feed.group, feed.port, feed.ttl};
    return sockets_[static_cast<std::size_t>(channel)].open_receiver(
        endpoint, config::feed::kMulticastLoopback);
  }

  void run() {
    std::array<std::byte, frame::kCapacity> buffer{};
    std::array<pollfd, 3> descriptors{};

    for (std::size_t i = 0; i < sockets_.size(); ++i) {
      descriptors[i].fd = sockets_[i].fd();
      descriptors[i].events = POLLIN;
    }

    while (running_.load(std::memory_order_acquire)) {
      const int ready =
          ::poll(descriptors.data(), static_cast<nfds_t>(descriptors.size()),
                 config::feed::kPollTimeoutMs);
      if (ready <= 0) {
        continue;
      }

      for (std::size_t i = 0; i < descriptors.size(); ++i) {
        if ((descriptors[i].revents & POLLIN) == 0) {
          continue;
        }

        const std::ptrdiff_t received =
            sockets_[i].recv(buffer.data(), buffer.size());
        if (received <= 0) {
          continue;
        }

        const auto channel = static_cast<MulticastChannel>(i);
        publish_datagram(channel, buffer.data(),
                         static_cast<std::size_t>(received));
      }
    }
  }

  void publish_datagram(MulticastChannel channel, const std::byte *data,
                        std::size_t len) noexcept {
    switch (channel) {
    case MulticastChannel::itch:
      publish(*inlet_, Kind::itch, InletFlags::none, data, len);
      itch_datagrams_.fetch_add(1, std::memory_order_relaxed);
      break;
    case MulticastChannel::mtbt:
      publish(*inlet_, Kind::mtbt, InletFlags::none, data, len);
      mtbt_datagrams_.fetch_add(1, std::memory_order_relaxed);
      break;
    case MulticastChannel::mcx:
      publish(*inlet_, Kind::mcx, InletFlags::none, data, len);
      mcx_datagrams_.fetch_add(1, std::memory_order_relaxed);
      break;
    }
    const std::uint64_t sequence =
        inlet_->sequence.load(std::memory_order_acquire);
    wait_until_processed(*inlet_, sequence);
  }

  SharedInlet *inlet_{nullptr};
  std::array<hft::net::UdpMulticastSocket, 3> sockets_{};
  std::atomic<bool> running_{false};
  std::thread thread_;
  std::atomic<std::uint64_t> itch_datagrams_{0};
  std::atomic<std::uint64_t> mtbt_datagrams_{0};
  std::atomic<std::uint64_t> mcx_datagrams_{0};
};

} // namespace hft::feed
