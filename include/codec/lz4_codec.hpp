#pragma once

#include <cstddef>

namespace hft::lz4 {

[[nodiscard]] bool decompress(const std::byte* input, std::size_t input_len,
                              std::byte* output, std::size_t output_capacity,
                              std::size_t& output_len) noexcept;

[[nodiscard]] bool compress(const std::byte* input, std::size_t input_len,
                            std::byte* output, std::size_t output_capacity,
                            std::size_t& output_len) noexcept;

} // namespace hft::lz4
