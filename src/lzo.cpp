#include <lzo.hpp>

#include <lzo/lzo1x.h>

namespace hft::lzo {

bool init() noexcept {
  return lzo_init() == LZO_E_OK;
}

bool decompress(const std::byte* input, const std::size_t input_len,
                std::byte* output, const std::size_t output_capacity,
                std::size_t& output_len) noexcept {
  if (input == nullptr || output == nullptr || input_len == 0 ||
      output_capacity == 0) {
    return false;
  }

  lzo_uint out_len = static_cast<lzo_uint>(output_capacity);
  const int rc = lzo1x_decompress(
      reinterpret_cast<const lzo_bytep>(input), static_cast<lzo_uint>(input_len),
      reinterpret_cast<lzo_bytep>(output), &out_len, nullptr);
  if (rc != LZO_E_OK) {
    return false;
  }

  output_len = static_cast<std::size_t>(out_len);
  return true;
}

bool compress(const std::byte* input, const std::size_t input_len,
              std::byte* output, const std::size_t output_capacity,
              std::size_t& output_len) noexcept {
  if (input == nullptr || output == nullptr || input_len == 0 ||
      output_capacity == 0) {
    return false;
  }

  alignas(8) lzo_align_t wrkmem[LZO1X_1_15_MEM_COMPRESS];
  lzo_uint out_len = static_cast<lzo_uint>(output_capacity);
  const int rc = lzo1x_1_15_compress(
      reinterpret_cast<const lzo_bytep>(input), static_cast<lzo_uint>(input_len),
      reinterpret_cast<lzo_bytep>(output), &out_len, wrkmem);
  if (rc != LZO_E_OK) {
    return false;
  }

  output_len = static_cast<std::size_t>(out_len);
  return true;
}

} // namespace hft::lzo
