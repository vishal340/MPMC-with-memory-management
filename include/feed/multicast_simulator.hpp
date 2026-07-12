#pragma once

#include <config/feed_multicast.hpp>
#include <net/udp_multicast.hpp>

#include <array>
#include <cstddef>

namespace hft::feed {

class MulticastSimulator {
public:
  [[nodiscard]] bool start() noexcept {
    stop();
    return open_sender(0, config::feed::kItch) &&
           open_sender(1, config::feed::kMtbt) &&
           open_sender(2, config::feed::kMcx);
  }

  void stop() noexcept {
    for (auto &socket : sockets_) {
      socket.close();
    }
  }

  void publish_itch(const std::byte *frame, std::size_t len) noexcept {
    send(0, frame, len);
  }

  void publish_mtbt(const std::byte *frame, std::size_t len) noexcept {
    send(1, frame, len);
  }

  void publish_mcx(const std::byte *frame, std::size_t len) noexcept {
    send(2, frame, len);
  }

private:
  [[nodiscard]] bool open_sender(std::size_t index,
                                 const config::feed::MulticastFeed &feed) noexcept {
    hft::net::MulticastEndpoint endpoint{feed.group, feed.port, feed.ttl};
    return sockets_[index].open_sender(endpoint, config::feed::kMulticastLoopback);
  }

  void send(std::size_t index, const std::byte *data, std::size_t len) noexcept {
    if (data == nullptr || len == 0) {
      return;
    }
    for (int attempt = 0; attempt < 3; ++attempt) {
      if (sockets_[index].send(data, len) > 0) {
        return;
      }
    }
  }

  std::array<hft::net::UdpMulticastSocket, 3> sockets_{};
};

} // namespace hft::feed
