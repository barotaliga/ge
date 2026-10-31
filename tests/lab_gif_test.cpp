// The review page's pictures (engine/lab/gif.hpp): the shrink, the palette and the LZW stream,
// read back by a decoder written here from the format's rules, so a code-size slip on either
// side shows as a wrong pixel.
//
// ge:summary Test: a lab picture shrunk and written as a GIF reads back pixel for pixel
// ge:keywords test gif lzw palette shrink decode round-trip animated-gif frames loop
// ge:skill testing labs
#include <algorithm>
#include <cstdint>
#include <map>
#include <vector>

#include "check.hpp"
#include "lab/gif.hpp"

using namespace ge::lab;

namespace {

struct Decoded {
  int width = 0, height = 0;
  std::vector<uint32_t> rgb;  // 0xRRGGBB a pixel
  bool ok = false;
};

int u16(const std::vector<uint8_t>& b, size_t at) { return b[at] | b[at + 1] << 8; }

// Every frame of a file with a global table, extensions skipped (what encode_gif and
// encode_gif_frames write), and each frame's delay in hundredths of a second.
struct Frames {
  std::vector<Decoded> frames;
  std::vector<int> delays;
  bool loops = false;
};

Frames decode_all(const std::vector<uint8_t>& b) {
  Frames all;
  if (b.size() < 13 + 768 + 11 || b[0] != 'G' || b[5] != 'a') return all;
  const int width = u16(b, 6), height = u16(b, 8);
  const size_t table = 13;
  size_t at = 13 + 768;
  while (at < b.size() && b[at] != 0x3B) {
    if (b[at] == 0x21) {  // an extension: its label, then sub-blocks
      const uint8_t label = b[at + 1];
      if (label == 0xF9) all.delays.push_back(u16(b, at + 4));
      if (label == 0xFF && b.size() > at + 14 && b[at + 3] == 'N') all.loops = true;
      at += 2;
      while (b[at] != 0) at += size_t{b[at]} + 1;
      ++at;
      continue;
    }
    if (b[at] != 0x2C) return all;
    Decoded d;
    d.width = width;
    d.height = height;
    at += 10;
    const int min = b[at++];
    std::vector<uint8_t> data;
    while (b[at] != 0) {
      const size_t len = b[at++];
      data.insert(data.end(), b.begin() + static_cast<long>(at),
                  b.begin() + static_cast<long>(at + len));
      at += len;
    }
    ++at;
    const uint32_t clear = 1u << min, end = clear + 1;
    std::vector<std::vector<uint8_t>> dict;
    const auto reset = [&] {
      dict.assign(end + 1, {});
      for (uint32_t i = 0; i < clear; ++i) dict[i] = {static_cast<uint8_t>(i)};
    };
    reset();
    int size = min + 1;
    size_t bit = 0;
    std::vector<uint8_t> idx, prev;
    for (;;) {
      if (bit + static_cast<size_t>(size) > data.size() * 8) return all;
      uint32_t code = 0;
      for (int i = 0; i < size; ++i, ++bit) code |= ((data[bit / 8] >> (bit % 8)) & 1u) << i;
      if (code == clear) {
        reset();
        size = min + 1;
        prev.clear();
        continue;
      }
      if (code == end) break;
      std::vector<uint8_t> entry;
      if (code < dict.size()) {
        entry = dict[code];
      } else if (code == dict.size() && !prev.empty()) {
        entry = prev;
        entry.push_back(prev[0]);
      } else {
        return all;
      }
      idx.insert(idx.end(), entry.begin(), entry.end());
      if (!prev.empty() && dict.size() < 4096) {
        std::vector<uint8_t> add = prev;
        add.push_back(entry[0]);
        dict.push_back(add);
        if (dict.size() == (size_t{1} << size) && size < 12) ++size;
      }
      prev = entry;
    }
    if (idx.size() != static_cast<size_t>(d.width) * d.height) return all;
    for (const uint8_t k : idx) {
      const size_t e = table + size_t{k} * 3;
      d.rgb.push_back(static_cast<uint32_t>(b[e]) << 16 | b[e + 1] << 8 | b[e + 2]);
    }
    d.ok = true;
    all.frames.push_back(std::move(d));
  }
  return all;
}

Decoded decode(const std::vector<uint8_t>& b) {
  Frames all = decode_all(b);
  return all.frames.size() == 1 ? all.frames[0] : Decoded{};
}

uint32_t rgb_at(const Picture& p, size_t i) {
  return static_cast<uint32_t>(p.rgba[i * 4]) << 16 | p.rgba[i * 4 + 1] << 8 | p.rgba[i * 4 + 2];
}

}  // namespace

TEST(a_picture_of_few_colours_reads_back_exactly) {
  // Long runs and a pattern that fills the LZW table past every code-size step and a reset.
  Picture p = create_picture(300, 200, {255, 255, 255});
  for (int y = 0; y < 200; ++y)
    for (int x = 0; x < 300; ++x)
      set_pixel(p, x, y,
                {static_cast<uint8_t>((x * 7 + y * 13) % 10 * 20), static_cast<uint8_t>(x % 3 * 90),
                 static_cast<uint8_t>((x ^ y) % 5 * 50)});
  const Decoded d = decode(encode_gif(p));
  CHECK(d.ok && d.width == 300 && d.height == 200);
  std::map<uint32_t, int> colours;
  bool same = d.ok;
  for (size_t i = 0; d.ok && i < d.rgb.size(); ++i) {
    colours[rgb_at(p, i)]++;
    same = same && d.rgb[i] == rgb_at(p, i);
  }
  CHECK(colours.size() <= 256);
  CHECK(same);
}

TEST(a_picture_of_many_colours_keeps_each_pixel_near) {
  Picture p = create_picture(256, 256, {0, 0, 0});
  for (int y = 0; y < 256; ++y)
    for (int x = 0; x < 256; ++x)
      set_pixel(p, x, y,
                {static_cast<uint8_t>(x), static_cast<uint8_t>(y), static_cast<uint8_t>(255 - x)});
  const Decoded d = decode(encode_gif(p));
  CHECK(d.ok);
  int worst = 0;
  for (size_t i = 0; d.ok && i < d.rgb.size(); ++i) {
    for (int s = 0; s < 24; s += 8) {
      const int a = static_cast<int>((d.rgb[i] >> s) & 255),
                b = static_cast<int>((rgb_at(p, i) >> s) & 255);
      worst = std::max(worst, a > b ? a - b : b - a);
    }
  }
  CHECK(worst <= 40);
}

TEST(alpha_is_laid_over_white_and_a_flat_picture_is_small) {
  const Picture p = create_picture(640, 360, {0, 0, 0, 0});
  const std::vector<uint8_t> gif = encode_gif(p);
  const Decoded d = decode(gif);
  CHECK(d.ok && d.rgb[0] == 0xFFFFFFu);
  CHECK(gif.size() < 640u * 360u / 20u);
}

TEST(a_shrink_averages_what_each_pixel_covers) {
  Picture p = create_picture(4, 2, {0, 0, 0});
  set_pixel(p, 0, 0, {200, 0, 0});
  set_pixel(p, 1, 1, {200, 0, 0});
  const Picture s = shrink(p, 2);
  CHECK(s.width == 2 && s.height == 1);
  CHECK(s.rgba[0] == 100 && s.rgba[4] == 0);
  CHECK(shrink(p, 8).width == 4);
}

TEST(an_animation_reads_back_frame_for_frame_and_loops) {
  // Three frames of a box moving across, on a ramp of grey: one palette for all of them.
  std::vector<Picture> frames;
  for (int k = 0; k < 3; ++k) {
    Picture p = create_picture(64, 32, {20, 20, 20});
    for (int x = 0; x < 64; ++x) set_pixel(p, x, 31, {static_cast<uint8_t>(x * 4), 0, 0});
    fill_rect(p, 10 + k * 16, 8, 12, 12, {240, 200, 40});
    frames.push_back(p);
  }
  const Frames all = decode_all(encode_gif_frames(frames, 7));
  CHECK(all.loops);
  CHECK(all.frames.size() == 3 && all.delays.size() == 3);
  for (size_t k = 0; k < all.frames.size(); ++k) {
    CHECK(all.frames[k].ok && all.delays[k] == 7);
    bool same = true;
    for (size_t i = 0; i < all.frames[k].rgb.size(); ++i)
      same = same && all.frames[k].rgb[i] == rgb_at(frames[k], i);
    CHECK(same);
  }
  CHECK(encode_gif_frames({}, 5).empty());
}
