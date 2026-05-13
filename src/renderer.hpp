#pragma once
#include "image.hpp"
#include <algorithm>
#include <cstddef>
#include <span>
#include <string_view>
#include <vector>

namespace automata {

struct RenderConfig {
  std::size_t pixel_scale = 32;
  std::size_t num_periods = 1;
  std::size_t separator_px = 3;
  Color hit_color = {220, 220, 220};
  Color rest_color = {30, 30, 30};
  Color separator_color = {70, 70, 70};
};

struct RenderOutput {
  std::vector<Color> pixels;
  std::size_t width;
  std::size_t height;
};

// Renders multiple rhythms stacked vertically with a separator between bands.
// Image width = max(lengths) * pixel_scale; shorter rhythms tile to fill.
inline RenderOutput render(std::span<const std::string> rhythm_bits_list,
                           const RenderConfig& cfg) {
  const std::size_t num_bands = rhythm_bits_list.size();
  const std::size_t max_steps =
      std::ranges::max_element(rhythm_bits_list, {}, &std::string::size)
          ->size();

  const std::size_t width = max_steps * cfg.pixel_scale;
  const std::size_t band_height = cfg.num_periods * cfg.pixel_scale;
  const std::size_t total_sep = (num_bands - 1) * cfg.separator_px;
  const std::size_t height = num_bands * band_height + total_sep;

  std::vector<Color> pixels(width * height);

  std::size_t y_offset = 0;
  for (std::size_t band = 0; band < num_bands; ++band) {
    if (band > 0) {
      for (std::size_t row = 0; row < cfg.separator_px; ++row)
        for (std::size_t col = 0; col < width; ++col)
          pixels[(y_offset + row) * width + col] = cfg.separator_color;
      y_offset += cfg.separator_px;
    }

    const std::string_view bits = rhythm_bits_list[band];
    const std::size_t steps = bits.size();
    for (std::size_t row = 0; row < band_height; ++row) {
      for (std::size_t col = 0; col < width; ++col) {
        const std::size_t step = (col / cfg.pixel_scale) % steps;
        const bool hit = bits[step] == '1';
        pixels[(y_offset + row) * width + col] =
            hit ? cfg.hit_color : cfg.rest_color;
      }
    }
    y_offset += band_height;
  }

  return {std::move(pixels), width, height};
}

}  // namespace automata
