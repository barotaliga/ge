// The pack's pictures (pictures.hpp).
//
// ge:summary Reads the pack's wad pictures and gfx/*.lmp files and colours them by the palette
// ge:keywords classicenhance pictures wad lmp conchars palette transparent rgba
// ge:skill classicenhance
// ge:test tests/classicenhance_pipeline_test.cpp
#include "classicenhance/pictures.hpp"

#include <algorithm>
#include <cstring>

#include "vfs/pak.hpp"

namespace ge::classicenhance {

namespace {

constexpr int kSheetSide = 128;  // the font sheet: 16 x 16 cells of 8 x 8

std::string lower(std::string s) {
  for (char& c : s) c = (c >= 'A' && c <= 'Z') ? char(c - 'A' + 'a') : c;
  return s;
}

}  // namespace

bool pack_pictures(std::span<const uint8_t> pack_bytes, Palette* palette,
                   std::vector<PackPicture>* out, std::string* error) {
  out->clear();
  vfs::Pak pak;
  if (!vfs::read_pak(pack_bytes, &pak, error)) return false;
  const vfs::PakEntry* pal = vfs::pak_find(pak, "gfx/palette.lmp");
  if (pal == nullptr || pal->size < 768) {
    *error = "the pack has no gfx/palette.lmp of 768 bytes; it is not the classic pack";
    return false;
  }
  std::memcpy(palette->rgb.data(), vfs::pak_file(pack_bytes, *pal).data(), 768);
  const vfs::PakEntry* wad_entry = vfs::pak_find(pak, "gfx.wad");
  if (wad_entry == nullptr) {
    *error =
        "the pack has no gfx.wad, where the status bar's pictures are; it is not the "
        "classic pack";
    return false;
  }
  const std::span<const uint8_t> wad_bytes = vfs::pak_file(pack_bytes, *wad_entry);
  vfs::Wad wad;
  if (!vfs::read_wad(wad_bytes, &wad, error)) {
    *error = "gfx.wad: " + *error;
    return false;
  }
  for (const vfs::WadEntry& e : wad.entries) {
    PackPicture p;
    p.name = lower(e.name);
    p.from = "gfx.wad";
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
      *error = "gfx.wad lump '" + e.name + "': " + *error;
      return false;
    }
    out->push_back(std::move(p));
  }
  std::vector<const vfs::PakEntry*> files;
  for (const vfs::PakEntry& e : pak.entries) {
    if (e.path.rfind("gfx/", 0) == 0 && e.path.ends_with(".lmp")) files.push_back(&e);
  }
  std::sort(files.begin(), files.end(), [](auto* a, auto* b) { return a->path < b->path; });
  for (const vfs::PakEntry* e : files) {
    PackPicture p;
    std::string ignored;
    // The palette and the colour map are tables, not pictures: their sizes do not add up.
    if (!vfs::read_picture(vfs::pak_file(pack_bytes, *e), &p.picture, &ignored)) continue;
    if (size_t(8) + p.picture.indices.size() != e->size) continue;
    p.name = lower(e->path.substr(0, e->path.size() - 4));
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

}  // namespace ge::classicenhance
