#include <lz4_codec.hpp>

#include <lz4.h>

namespace hft::lz4 {

bool decompress(const std::byte* input, const std::size_t input_len,
                std::byte* output, const std::size_t output_capacity,
                std::size_t& output_len) noexcept {
  if (input == nullptr || output == nullptr || input_len == 0 ||
      output_capacity == 0) {
    return false;
  }

  const int rc = LZ4_decompress_safe(
      reinterpret_cast<const char*>(input), reinterpret_cast<char*>(output),
      static_cast<int>(input_len), static_cast<int>(output_capacity));
  if (rc < 0) {
    return false;
  }

  output_len = static_cast<std::size_t>(rc);
  return true;
}

bool compress(const std::byte* input, const std::size_t input_len,
              std::byte* output, const std::size_t output_capacity,
              std::size_t& output_len) noexcept {
  if (input == nullptr || output == nullptr || input_len == 0 ||
      output_capacity == 0) {
    return false;
  }

  const int rc = LZ4_compress_default(
      reinterpret_cast<const char*>(input), reinterpret_cast<char*>(output),
      static_cast<int>(input_len), static_cast<int>(output_capacity));
  if (rc <= 0) {
    return false;
  }

  output_len = static_cast<std::size_t>(rc);
  return true;
}

} // namespace hft::lz4
