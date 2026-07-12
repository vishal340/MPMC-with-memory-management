#pragma once

#include <cstddef>
#include <cstdint>

namespace hft::net {

struct MulticastEndpoint {
  const char *group{};
  std::uint16_t port{};
  std::uint8_t ttl{1};
};

class UdpMulticastSocket {
public:
  UdpMulticastSocket() noexcept = default;
  ~UdpMulticastSocket() noexcept;

  UdpMulticastSocket(const UdpMulticastSocket &) = delete;
  UdpMulticastSocket &operator=(const UdpMulticastSocket &) = delete;

  [[nodiscard]] bool open_receiver(const MulticastEndpoint &endpoint,
                                   bool loopback) noexcept;
  [[nodiscard]] bool open_sender(const MulticastEndpoint &endpoint,
                                 bool loopback) noexcept;

  [[nodiscard]] int fd() const noexcept { return fd_; }
  [[nodiscard]] bool valid() const noexcept { return fd_ >= 0; }

  [[nodiscard]] std::ptrdiff_t recv(void *buffer, std::size_t capacity) noexcept;
  [[nodiscard]] std::ptrdiff_t send(const void *buffer,
                                    std::size_t length) noexcept;

  void close() noexcept;

private:
  int fd_{-1};
  MulticastEndpoint endpoint_{};
};

} // namespace hft::net
