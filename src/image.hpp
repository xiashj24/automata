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

// BMP is little-endian; write helpers emit bytes in the correct order.
inline void write_bmp(std::ostream& out,
                      std::span<const Color> pixels,
                      std::size_t width,
                      std::size_t height) {
  const std::size_t row_stride = (width * 3 + 3) & ~3u;
  const std::size_t pixel_bytes = row_stride * height;
  const std::size_t file_size = 54 + pixel_bytes;

  auto le16 = [&](uint16_t v) {
    out.put(static_cast<char>(v & 0xFF));
    out.put(static_cast<char>(v >> 8));
  };
  auto le32 = [&](uint32_t v) {
    out.put(static_cast<char>(v & 0xFF));
    out.put(static_cast<char>((v >> 8) & 0xFF));
    out.put(static_cast<char>((v >> 16) & 0xFF));
    out.put(static_cast<char>(v >> 24));
  };

  // File header
  out.write("BM", 2);
  le32(static_cast<uint32_t>(file_size));
  le32(0);   // reserved
  le32(54);  // pixel data offset

  // DIB header (BITMAPINFOHEADER)
  le32(40);  // header size
  le32(static_cast<uint32_t>(width));
  le32(static_cast<uint32_t>(
      -static_cast<int32_t>(height)));  // negative = top-down
  le16(1);                              // color planes
  le16(24);                             // bits per pixel
  le32(0);                              // compression (BI_RGB)
  le32(static_cast<uint32_t>(pixel_bytes));
  le32(2835);  // ~72 DPI
  le32(2835);
  le32(0);  // colors in table
  le32(0);  // important colors

  // Pixel rows: BGR order, padded to 4-byte boundary
  const char pad[3] = {};
  const std::size_t padding = row_stride - width * 3;
  for (std::size_t row = 0; row < height; ++row) {
    for (std::size_t col = 0; col < width; ++col) {
      const Color& c = pixels[row * width + col];
      out.put(static_cast<char>(c.b));
      out.put(static_cast<char>(c.g));
      out.put(static_cast<char>(c.r));
    }
    out.write(pad, static_cast<std::streamsize>(padding));
  }
}

}  // namespace automata
