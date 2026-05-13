#pragma once
#include <cstdint>
#include <ostream>
#include <span>

namespace automata {

struct Color {
  uint8_t r, g, b;
};

inline void write_ppm(std::ostream& out,
                      std::span<const Color> pixels,
                      std::size_t width,
                      std::size_t height) {
  out << "P6\n" << width << " " << height << "\n255\n";
  for (const Color& color : pixels)
    out.write(reinterpret_cast<const char*>(&color), 3);
}

}  // namespace automata
