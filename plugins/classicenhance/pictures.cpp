// The pack's pictures (pictures.hpp).
//
// ge:summary Reads the pack's wad pictures and gfx/*.lmp files, colours them and upscales them
// ge:keywords classicenhance pictures wad lmp conchars palette transparent rgba enhance_picture
// ge:keywords bleed pad cell alpha steepen
// ge:skill classicenhance
// ge:test tests/classicenhance_pictures_test.cpp
#include "classicenhance/pictures.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>

#include "vfs/pak.hpp"

namespace ge::classicenhance {

namespace {

constexpr int kSheetSide = 128;  // the font sheet: 16 x 16 cells of 8 x 8

std::string lower(std::string s) {
  for (char& c : s) c = (c >= 'A' && c <= 'Z') ? char(c - 'A' + 'a') : c;
  return s;
}

// Texels of padding round a picture (or a cell) before the upscale: past the kernel's reach, and
// context for the network.
constexpr int kPad = 4;

// One cell of `img` (the whole picture, or a glyph's cell) upscaled into `out` at (x0, y0) × scale.
void enhance_cell(const assets::Image& img, int x0, int y0, int cw, int ch, std::string_view name,
                  const EnhanceOptions& o, int scale, assets::Image* out) {
  const auto at = [&](int x, int y) {
    return (size_t(y0 + y) * size_t(img.width) + size_t(x0 + x)) * 4;
  };
  // 1. The opaque texels' colours spread into the see-through ones, a ring at a time.
  const size_t n = size_t(cw) * size_t(ch);
  std::vector<float> rgb(n * 3);
  std::vector<uint8_t> filled(n), alpha(n);
  for (int y = 0; y < ch; y++) {
    for (int x = 0; x < cw; x++) {
      const size_t i = size_t(y) * size_t(cw) + size_t(x);
      for (int c = 0; c < 3; c++) rgb[i * 3 + size_t(c)] = img.rgba[at(x, y) + size_t(c)];
      filled[i] = alpha[i] = img.rgba[at(x, y) + 3] != 0 ? 1 : 0;
    }
  }
  if (std::find(alpha.begin(), alpha.end(), 1) == alpha.end()) return;  // all see-through
  for (bool grew = true; grew;) {
    grew = false;
    std::vector<uint8_t> next = filled;
    for (int y = 0; y < ch; y++) {
      for (int x = 0; x < cw; x++) {
        const size_t i = size_t(y) * size_t(cw) + size_t(x);
        if (filled[i]) continue;
        float sum[3] = {0, 0, 0};
        int count = 0;
        for (int dy = -1; dy <= 1; dy++) {
          for (int dx = -1; dx <= 1; dx++) {
            const int nx = x + dx, ny = y + dy;
            if (nx < 0 || ny < 0 || nx >= cw || ny >= ch) continue;
            const size_t j = size_t(ny) * size_t(cw) + size_t(nx);
            if (!filled[j]) continue;
            for (int c = 0; c < 3; c++) sum[c] += rgb[j * 3 + size_t(c)];
            count++;
          }
        }
        if (count == 0) continue;
        for (int c = 0; c < 3; c++) rgb[i * 3 + size_t(c)] = sum[c] / float(count);
        next[i] = 1;
        grew = true;
      }
    }
    filled.swap(next);
  }
  // 2. Padded by its own edge texels, upscaled, the padding cropped off.
  const int pw = cw + 2 * kPad, ph = ch + 2 * kPad;
  assets::Image padded{pw, ph, std::vector<uint8_t>(size_t(pw) * size_t(ph) * 4, 255)};
  for (int y = 0; y < ph; y++) {
    for (int x = 0; x < pw; x++) {
      const int sx = std::clamp(x - kPad, 0, cw - 1), sy = std::clamp(y - kPad, 0, ch - 1);
      const size_t i = size_t(sy) * size_t(cw) + size_t(sx);
      for (int c = 0; c < 3; c++)
        padded.rgba[(size_t(y) * size_t(pw) + size_t(x)) * 4 + size_t(c)] =
            uint8_t(std::lround(std::clamp(rgb[i * 3 + size_t(c)], 0.0f, 255.0f)));
    }
  }
  const assets::Image big = upscale_albedo(padded, name, o);
  // 3. The mask bilinear at the new size, steepened about one half.
  const float steep = std::max(1.0f, float(scale) / 1.25f);
  const auto mask = [&](int x, int y) {
    x = std::clamp(x, 0, cw - 1);
    y = std::clamp(y, 0, ch - 1);
    return float(alpha[size_t(y) * size_t(cw) + size_t(x)]);
  };
  for (int oy = 0; oy < ch * scale; oy++) {
    const float sy = (float(oy) + 0.5f) / float(scale) - 0.5f;
    const int iy = int(std::floor(sy));
    const float fy = sy - float(iy);
    for (int ox = 0; ox < cw * scale; ox++) {
      const float sx = (float(ox) + 0.5f) / float(scale) - 0.5f;
      const int ix = int(std::floor(sx));
      const float fx = sx - float(ix);
      const float a = (1 - fy) * ((1 - fx) * mask(ix, iy) + fx * mask(ix + 1, iy)) +
                      fy * ((1 - fx) * mask(ix, iy + 1) + fx * mask(ix + 1, iy + 1));
      const float s = std::clamp((a - 0.5f) * steep + 0.5f, 0.0f, 1.0f);
      const size_t from =
          (size_t(oy + kPad * scale) * size_t(big.width) + size_t(ox + kPad * scale)) * 4;
      const size_t to =
          (size_t(y0 * scale + oy) * size_t(out->width) + size_t(x0 * scale + ox)) * 4;
      std::memcpy(&out->rgba[to], &big.rgba[from], 3);
      out->rgba[to + 3] = uint8_t(std::lround(s * 255.0f));
    }
  }
}

}  // namespace

bool pack_pictures(std::span<const uint8_t> pack_bytes, Palette* palette,
                   std::vector<PackPicture>* out, std::string* error, const Profile& profile) {
  out->clear();
  vfs::Pak pak;
  if (!vfs::read_pak(pack_bytes, &pak, error)) return false;
  if (!read_palette(profile, pak, pack_bytes, palette, error)) return false;
  const std::string wad_name(profile.wad);
  const vfs::PakEntry* wad_entry = vfs::pak_find(pak, wad_name);
  if (wad_entry == nullptr) {
    *error = "the pack has no " + wad_name + ", where the status bar's pictures are; it is not " +
             std::string(profile.what) + "'s pack (profile " + std::string(profile.id) + ")";
    return false;
  }
  const std::span<const uint8_t> wad_bytes = vfs::pak_file(pack_bytes, *wad_entry);
  vfs::Wad wad;
  if (!vfs::read_wad(wad_bytes, &wad, error)) {
    *error = wad_name + ": " + *error;
    return false;
  }
  for (const vfs::WadEntry& e : wad.entries) {
    PackPicture p;
    p.name = lower(e.name);
    p.from = wad_name;
    bool ok = false;
    if (e.compressed) continue;
    if (e.type == vfs::kPictureLump) {
      ok = vfs::read_picture(vfs::wad_lump(wad_bytes, e), &p.picture, error);
    } else if (p.name == "conchars") {
      ok = vfs::read_raw_picture(vfs::wad_lump(wad_bytes, e), kSheetSide, kSheetSide, &p.picture,
                                 error);
      p.transparent = 0;
    } else {
      continue;
    }
    if (!ok) {
      *error = wad_name + " lump '" + e.name + "': " + *error;
      return false;
    }
    out->push_back(std::move(p));
  }
  for (const vfs::PakEntry* e : assets_of(pak, profile.pictures)) {
    PackPicture p;
    std::string ignored;
    // The palette and the colour map are tables, not pictures: their sizes do not add up.
    if (!vfs::read_picture(vfs::pak_file(pack_bytes, *e), &p.picture, &ignored)) continue;
    if (size_t(8) + p.picture.indices.size() != e->size) continue;
    p.name = lower(e->path.substr(0, e->path.size() - profile.pictures.ext.size()));
    p.from = e->path;
    out->push_back(std::move(p));
  }
  return true;
}

assets::Image picture_rgba(const vfs::IndexedPicture& picture, const Palette& palette,
                           int transparent) {
  assets::Image img;
  img.width = picture.width;
  img.height = picture.height;
  img.rgba.resize(picture.indices.size() * 4);
  for (size_t i = 0; i < picture.indices.size(); ++i) {
    const uint8_t k = picture.indices[i];
    std::memcpy(&img.rgba[i * 4], &palette.rgb[size_t(k) * 3], 3);
    img.rgba[i * 4 + 3] = k == transparent ? 0 : 255;
  }
  return img;
}

assets::Image enhance_picture(const PackPicture& p, const Palette& palette,
                              const EnhanceOptions& o) {
  const int scale = std::clamp(o.scale, 1, 8);
  const assets::Image img = picture_rgba(p.picture, palette, p.transparent);
  if (img.width <= 0 || img.height <= 0) return {};
  assets::Image out{img.width * scale, img.height * scale,
                    std::vector<uint8_t>(img.rgba.size() * size_t(scale) * size_t(scale), 0)};
  // The font sheet a glyph at a time; any other picture whole.
  const bool sheet = p.name == "conchars" && img.width == kSheetSide && img.height == kSheetSide;
  const int cw = sheet ? kSheetSide / 16 : img.width, ch = sheet ? kSheetSide / 16 : img.height;
  for (int y = 0; y + ch <= img.height; y += ch) {
    for (int x = 0; x + cw <= img.width; x += cw)
      enhance_cell(img, x, y, cw, ch, p.name, o, scale, &out);
  }
  return out;
}

}  // namespace ge::classicenhance
