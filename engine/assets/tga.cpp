// The TGA reader and writer (tga.hpp).
//
// ge:summary Decodes and encodes TGA pictures to and from RGBA8
// ge:keywords assets tga decode_tga encode_tga rle
// ge:skill assets
// ge:test tests/assets_tga_test.cpp
#include "assets/tga.hpp"

#include <array>
#include <cstring>

namespace ge::assets {

namespace {

bool fail(std::string* why, const std::string& text) {
  if (why) *why = "tga: " + text + "; next: save the picture as a 24 or 32-bit TGA, or a PNG";
  return false;
}

uint16_t u16(std::span<const uint8_t> b, size_t at) {
  return static_cast<uint16_t>(b[at] | (b[at + 1] << 8));
}

// One pixel of `depth` bits at `p` as RGBA.
void pixel(const uint8_t* p, uint32_t depth, uint8_t* out) {
  switch (depth) {
    case 8:
      out[0] = out[1] = out[2] = p[0], out[3] = 255;
      break;
    case 16: {
      const uint16_t v = static_cast<uint16_t>(p[0] | (p[1] << 8));
      const auto five = [](uint32_t c) { return static_cast<uint8_t>((c << 3) | (c >> 2)); };
      out[0] = five((v >> 10) & 31), out[1] = five((v >> 5) & 31), out[2] = five(v & 31);
      out[3] = (v & 0x8000) != 0 ? 255 : 0;
      break;
    }
    case 24:
      out[0] = p[2], out[1] = p[1], out[2] = p[0], out[3] = 255;
      break;
    default:
      out[0] = p[2], out[1] = p[1], out[2] = p[0], out[3] = p[3];
      break;  // 32
  }
}

bool decode(std::span<const uint8_t> b, Image& img, std::string* why) {
  if (b.size() < 18)
    return fail(why,
                "the file is " + std::to_string(b.size()) + " bytes, under the 18-byte header");
  const uint8_t id_len = b[0], map_type = b[1], type = b[2];
  const uint16_t map_first = u16(b, 3), map_len = u16(b, 5);
  const uint8_t map_depth = b[7];
  const int w = u16(b, 12), h = u16(b, 14);
  const uint8_t depth = b[16], desc = b[17];
  const bool packed = type >= 9;
  const uint8_t base = packed ? uint8_t(type - 8) : type;
  if (base < 1 || base > 3)
    return fail(why, "image type " + std::to_string(type) + " is not 1, 2, 3, 9, 10 or 11");
  if (w == 0 || h == 0)
    return fail(why, "the picture is " + std::to_string(w) + " x " + std::to_string(h));
  const bool mapped = base == 1, grey = base == 3;
  if (mapped && (depth != 8 || map_type != 1 || (map_depth != 24 && map_depth != 32)))
    return fail(why, "a colour-mapped picture must be 8 bits over a 24 or 32-bit map (it is " +
                         std::to_string(depth) + " over " + std::to_string(map_depth) + ")");
  if (grey && depth != 8)
    return fail(why, "a grey picture must be 8 bits (it is " + std::to_string(depth) + ")");
  if (!mapped && !grey && depth != 16 && depth != 24 && depth != 32)
    return fail(why, "a true-colour picture must be 16, 24 or 32 bits (it is " +
                         std::to_string(depth) + ")");
  size_t at = 18 + size_t(id_len);
  std::vector<uint8_t> map;
  if (map_type == 1) {
    const size_t entry = (size_t(map_depth) + 7) / 8;
    const size_t bytes = entry * map_len;
    if (at + bytes > b.size()) return fail(why, "the colour map runs past the end of the file");
    if (mapped) {
      map.resize(size_t(map_len) * 4);
      for (size_t i = 0; i < map_len; ++i) pixel(&b[at + i * entry], map_depth, &map[i * 4]);
    }
    at += bytes;
  }
  const size_t in_px = (size_t(depth) + 7) / 8;
  const size_t count = size_t(w) * size_t(h);
  std::vector<uint8_t> px(count * 4);
  const auto put = [&](size_t i, const uint8_t* p) -> bool {
    if (!mapped) {
      pixel(p, depth, &px[i * 4]);
      return true;
    }
    const int k = int(p[0]) - int(map_first);
    if (k < 0 || size_t(k) >= map_len) return false;
    std::memcpy(&px[i * 4], &map[size_t(k) * 4], 4);
    return true;
  };
  size_t i = 0;
  while (i < count) {
    if (!packed) {
      if (at + in_px > b.size())
        return fail(why, "the pixels run out at pixel " + std::to_string(i));
      if (!put(i, &b[at]))
        return fail(why, "pixel " + std::to_string(i) + " names a colour past the map");
      at += in_px, ++i;
      continue;
    }
    if (at >= b.size()) return fail(why, "the runs end at pixel " + std::to_string(i));
    const uint8_t head = b[at++];
    const size_t n = size_t(head & 0x7f) + 1;
    if (i + n > count)
      return fail(why, "a run at pixel " + std::to_string(i) + " passes the picture's end");
    if ((head & 0x80) != 0) {
      if (at + in_px > b.size()) return fail(why, "a run's pixel is past the end of the file");
      for (size_t k = 0; k < n; ++k)
        if (!put(i + k, &b[at])) return fail(why, "a run names a colour past the map");
      at += in_px;
    } else {
      if (at + in_px * n > b.size()) return fail(why, "a run's pixels pass the end of the file");
      for (size_t k = 0; k < n; ++k, at += in_px)
        if (!put(i + k, &b[at])) return fail(why, "a run names a colour past the map");
    }
    i += n;
  }
  img.width = w;
  img.height = h;
  img.rgba.resize(px.size());
  const bool from_top = (desc & 0x20) != 0;
  const size_t row = size_t(w) * 4;
  for (int y = 0; y < h; ++y) {
    const int src = from_top ? y : h - 1 - y;
    std::memcpy(&img.rgba[size_t(y) * row], &px[size_t(src) * row], row);
  }
  return true;
}

}  // namespace

std::optional<Image> decode_tga(std::span<const uint8_t> bytes, std::string* why) {
  Image img;
  if (!decode(bytes, img, why)) return std::nullopt;
  return img;
}

std::vector<uint8_t> encode_tga(const Image& image, bool packed) {
  std::vector<uint8_t> out(18, 0);
  out[2] = packed ? 10 : 2;
  out[12] = uint8_t(image.width & 0xff), out[13] = uint8_t(image.width >> 8);
  out[14] = uint8_t(image.height & 0xff), out[15] = uint8_t(image.height >> 8);
  out[16] = 32;
  out[17] = 0x20 | 8;  // from the top, 8 alpha bits
  const size_t count = size_t(image.width) * size_t(image.height);
  const auto bgra = [&](size_t i) {
    const uint8_t* p = &image.rgba[i * 4];
    return std::array<uint8_t, 4>{p[2], p[1], p[0], p[3]};
  };
  size_t i = 0;
  while (i < count) {
    if (!packed) {
      const auto p = bgra(i++);
      out.insert(out.end(), p.begin(), p.end());
      continue;
    }
    size_t n = 1;  // a run of the same pixel, up to 128
    while (i + n < count && n < 128 && bgra(i + n) == bgra(i)) ++n;
    if (n > 1) {
      out.push_back(uint8_t(0x80 | (n - 1)));
      const auto p = bgra(i);
      out.insert(out.end(), p.begin(), p.end());
    } else {  // a raw stretch until the next repeat
      while (i + n < count && n < 128 && !(i + n + 1 < count && bgra(i + n) == bgra(i + n + 1)))
        ++n;
      out.push_back(uint8_t(n - 1));
      for (size_t k = 0; k < n; ++k) {
        const auto p = bgra(i + k);
        out.insert(out.end(), p.begin(), p.end());
      }
    }
    i += n;
  }
  return out;
}

}  // namespace ge::assets
