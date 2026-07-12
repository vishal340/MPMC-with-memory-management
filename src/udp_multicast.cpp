#include <net/udp_multicast.hpp>

#include <cstdio>
#include <cstring>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

namespace hft::net {

namespace {

bool set_reuseaddr(int fd) noexcept {
  const int yes = 1;
  return setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes)) == 0;
}

bool set_loopback(int fd, bool enabled) noexcept {
  const unsigned char loop = enabled ? 1 : 0;
  return setsockopt(fd, IPPROTO_IP, IP_MULTICAST_LOOP, &loop, sizeof(loop)) == 0;
}

bool set_ttl(int fd, std::uint8_t ttl) noexcept {
  const unsigned char value = ttl;
  return setsockopt(fd, IPPROTO_IP, IP_MULTICAST_TTL, &value, sizeof(value)) == 0;
}

bool join_group(int fd, const char *group, bool loopback) noexcept {
  ip_mreq request{};
  if (inet_pton(AF_INET, group, &request.imr_multiaddr) != 1) {
    return false;
  }
  if (loopback) {
    request.imr_interface.s_addr = inet_addr("127.0.0.1");
  } else {
    request.imr_interface.s_addr = htonl(INADDR_ANY);
  }
  return setsockopt(fd, IPPROTO_IP, IP_ADD_MEMBERSHIP, &request,
                    sizeof(request)) == 0;
}

bool bind_port(int fd, std::uint16_t port) noexcept {
  sockaddr_in address{};
  address.sin_family = AF_INET;
  address.sin_addr.s_addr = htonl(INADDR_ANY);
  address.sin_port = htons(port);
  return bind(fd, reinterpret_cast<sockaddr *>(&address), sizeof(address)) == 0;
}

bool set_multicast_if(int fd, bool loopback) noexcept {
  in_addr iface{};
  iface.s_addr = loopback ? inet_addr("127.0.0.1") : htonl(INADDR_ANY);
  return setsockopt(fd, IPPROTO_IP, IP_MULTICAST_IF, &iface, sizeof(iface)) == 0;
}

} // namespace

UdpMulticastSocket::~UdpMulticastSocket() noexcept { close(); }

void UdpMulticastSocket::close() noexcept {
  if (fd_ >= 0) {
    ::close(fd_);
    fd_ = -1;
  }
}

bool UdpMulticastSocket::open_receiver(const MulticastEndpoint &endpoint,
                                       bool loopback) noexcept {
  close();
  endpoint_ = endpoint;

  const int fd = ::socket(AF_INET, SOCK_DGRAM, 0);
  if (fd < 0) {
    return false;
  }

  if (!set_reuseaddr(fd) || !bind_port(fd, endpoint.port) ||
      !join_group(fd, endpoint.group, loopback) ||
      !set_loopback(fd, loopback)) {
    ::close(fd);
    return false;
  }

  fd_ = fd;
  return true;
}

bool UdpMulticastSocket::open_sender(const MulticastEndpoint &endpoint,
                                     bool loopback) noexcept {
  close();
  endpoint_ = endpoint;

  const int fd = ::socket(AF_INET, SOCK_DGRAM, 0);
  if (fd < 0) {
    return false;
  }

  if (!set_ttl(fd, endpoint.ttl) || !set_loopback(fd, loopback) ||
      !set_multicast_if(fd, loopback)) {
    ::close(fd);
    return false;
  }

  fd_ = fd;
  return true;
}

std::ptrdiff_t UdpMulticastSocket::recv(void *buffer,
                                        std::size_t capacity) noexcept {
  if (fd_ < 0 || buffer == nullptr || capacity == 0) {
    return -1;
  }
  return ::recvfrom(fd_, buffer, capacity, 0, nullptr, nullptr);
}

std::ptrdiff_t UdpMulticastSocket::send(const void *buffer,
                                        std::size_t length) noexcept {
  if (fd_ < 0 || buffer == nullptr || length == 0 ||
      endpoint_.group == nullptr || endpoint_.port == 0) {
    return -1;
  }

  sockaddr_in address{};
  address.sin_family = AF_INET;
  address.sin_port = htons(endpoint_.port);
  if (inet_pton(AF_INET, endpoint_.group, &address.sin_addr) != 1) {
    return -1;
  }

  return ::sendto(fd_, buffer, length, 0,
                  reinterpret_cast<sockaddr *>(&address), sizeof(address));
}

} // namespace hft::net
