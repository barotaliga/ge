// The classic pictures read: each slot of the HUD's set by the pack's picture name, from the
// cache's PNGs or straight from the packs' graphics archive, and `hud_art`'s pick between them and
// the free set.
//
// ge:summary The classic HUD pictures read by the pack's names from the cache or the packs
// ge:keywords hud classic-art png decode names slots cell-of gfx-wad palette pick_art hud_art
// ge:skill ui
// ge:test tests/shooters_edge_hud_test.cpp
#include "shooters_edge/hud/classic_art.hpp"

#include <array>
#include <fstream>
#include <iterator>
#include <optional>
#include <string_view>
#include <vector>

#include "assets/image.hpp"
#include "vfs/wad.hpp"

namespace shooters_edge::hud {

namespace {

namespace ui = ge::ui;

// A PNG of the cache's into the 2D layer's image; false when it is missing or not a PNG.
bool load_png(const std::filesystem::path& path, ui::Image* out) {
  std::ifstream in(path, std::ios::binary);
  const std::vector<uint8_t> bytes((std::istreambuf_iterator<char>(in)),
                                   std::istreambuf_iterator<char>());
  const std::optional<ge::assets::Image> img = ge::assets::decode_png(bytes);
  if (!img) return false;
  out->width = img->width;
  out->height = img->height;
  out->rgba = img->rgba;
  return true;
}

// A palette picture in the palette's colours, `clear` drawn see-through.
ui::Image colour(const ge::vfs::IndexedPicture& p, std::span<const uint8_t> palette, int clear) {
  ui::Image img;
  img.width = p.width;
  img.height = p.height;
  img.rgba.resize(static_cast<size_t>(p.width) * static_cast<size_t>(p.height) * 4);
  for (size_t i = 0; i < p.indices.size(); i++) {
    const uint8_t k = p.indices[i];
    uint8_t* px = &img.rgba[i * 4];
    const size_t at = size_t{k} * 3;
    px[0] = palette[at];
    px[1] = palette[at + 1];
    px[2] = palette[at + 2];
    px[3] = k == clear ? 0 : 255;
  }
  return img;
}

// The pack's names for the guns on the inventory bar, in the HUD's order.
constexpr std::array<std::string_view, kGuns> kGunNames = {
    "shotgun", "sshotgun", "nailgun", "snailgun", "rlaunch", "srlaunch", "lightng"};

// One cell of a 16 x 16 glyph sheet.
ui::Image cell_of(const ui::Image& sheet, uint8_t code) {
  const int cw = sheet.width / 16, ch = sheet.height / 16;
  ui::Image out = ui::make_image(cw, ch, {0, 0, 0, 0});
  const int x0 = (code % 16) * cw, y0 = (code / 16) * ch;
  for (int y = 0; y < ch; ++y)
    for (int x = 0; x < cw; ++x)
      for (int k = 0; k < 4; ++k)
        out.rgba[(static_cast<size_t>(y) * cw + x) * 4 + k] =
            sheet.rgba[(static_cast<size_t>(y0 + y) * sheet.width + x0 + x) * 4 + k];
  return out;
}

}  // namespace

bool classic_art(const PictureFind& find, std::string_view where, HudArt* out, std::string* error) {
  const auto load = [&](std::string_view name, ui::Image* img) {
    if (find(name, img)) return true;
    if (error != nullptr)
      *error = "classic_art: " + std::string(where) + " has no picture `" + std::string(name) +
               "`; the HUD draws the free art";
    return false;
  };
  HudArt a;
  a.density = 1;
  bool ok = load("sbar", &a.bar) && load("ibar", &a.inventory_bar) &&
            load("disc", &a.armour_shield) && load("face_invis", &a.face_cloak) &&
            load("face_invul2", &a.face_shield) && load("face_inv2", &a.face_cloak_shield) &&
            load("face_quad", &a.face_damage) && load("conchars", &a.glyphs);
  for (size_t k = 0; ok && k < 3; ++k) ok = load("sb_armor" + std::to_string(k + 1), &a.armour[k]);
  constexpr std::array<std::string_view, kAmmoKinds> kAmmo = {"sb_shells", "sb_nails", "sb_rocket",
                                                              "sb_cells"};
  for (size_t k = 0; ok && k < kAmmo.size(); ++k) ok = load(kAmmo[k], &a.ammo[k]);
  // Band 0 is the nearest death: the pack numbers its portraits from the healthiest.
  for (size_t b = 0; ok && b < kFaceBands; ++b) {
    const std::string n = std::to_string(kFaceBands - b);
    ok = load("face" + n, &a.face[b][0]) && load("face_p" + n, &a.face[b][1]);
  }
  for (size_t k = 0; ok && k < kGuns; ++k) {
    const std::string g(kGunNames[k]);
    ok = load("inv_" + g, &a.gun[k]) && load("inv2_" + g, &a.gun_lit[k]);
    for (size_t f = 0; ok && f < kFlashFrames; ++f)
      ok = load("inva" + std::to_string(f + 1) + "_" + g, &a.gun_flash[k][f]);
  }
  for (size_t k = 0; ok && k < 2; ++k) ok = load("sb_key" + std::to_string(k + 1), &a.key[k]);
  constexpr std::array<std::string_view, kPowers> kPower = {"sb_invis", "sb_invuln", "sb_suit",
                                                            "sb_quad"};
  for (size_t k = 0; ok && k < kPower.size(); ++k) ok = load(kPower[k], &a.power[k]);
  for (size_t k = 0; ok && k < 4; ++k) ok = load("sb_sigil" + std::to_string(k + 1), &a.rune[k]);
  for (size_t k = 0; ok && k < 11; ++k) {
    const std::string d = k < 10 ? std::to_string(k) : "minus";
    ok = load("num_" + d, &a.digit[k]) && load("anum_" + d, &a.digit_low[k]);
  }
  if (!ok) return false;
  // The sheet's gold figures are its cells 18 to 27. The crosshair is the sheet's '+' cell, as
  // the classic game drew it; there is no health icon (the modern looks show the portrait).
  a.digits_at = 18;
  a.crosshair = cell_of(a.glyphs, '+');
  a.count_color = {1, 1, 1, 1};
  *out = std::move(a);
  out->finish();
  return true;
}

bool classic_art(const std::filesystem::path& dir, HudArt* out, std::string* error) {
  const PictureFind find = [&dir](std::string_view name, ui::Image* img) {
    return load_png(dir / (std::string(name) + ".png"), img);
  };
  if (classic_art(find, dir.string(), out, error)) {
    // The cache's enhanced folder (`pictures/enhanced/`) holds the same pictures at a whole
    // multiple of their size: the bar (320 art pixels wide) says which.
    if (out->bar.width >= 640 && out->bar.width % 320 == 0) {
      out->density = static_cast<float>(out->bar.width) / 320.0f;
      out->finish();
    }
    return true;
  }
  if (error != nullptr)
    *error +=
        " (run classic-enhance's pictures stage on the pack: ge tool classicenhance -- run "
        "<pack>)";
  return false;
}

bool classic_art_from_packs(const FileFind& find, HudArt* out, std::string* error,
                            const PictureUpgrade& upgrade) {
  std::vector<uint8_t> pal_store, wad_store;
  const std::span<const uint8_t> palette = find("gfx/palette.lmp", pal_store);
  if (palette.size() != 768) {
    if (error != nullptr) *error = "classic_art: the packs have no gfx/palette.lmp of 768 bytes";
    return false;
  }
  const std::span<const uint8_t> wad_bytes = find("gfx.wad", wad_store);
  ge::vfs::Wad wad;
  std::string why;
  if (wad_bytes.empty() || !ge::vfs::read_wad(wad_bytes, &wad, &why)) {
    if (error != nullptr)
      *error = "classic_art: the packs have no readable gfx.wad" + (why.empty() ? "" : ": " + why);
    return false;
  }
  // The set's density: the first picture's enhanced scale (the bar's), 1 with no upgrade.
  uint32_t density = 0;
  const PictureFind pictures = [&](std::string_view name, ui::Image* img) {
    const ge::vfs::WadEntry* e = ge::vfs::wad_find(wad, name);
    if (e == nullptr) return false;
    ge::vfs::IndexedPicture p;
    std::string bad;
    // The glyph sheet is a raw 128 x 128 lump with index 0 see-through; every other a picture.
    const bool sheet = name == "conchars";
    if (sheet ? !ge::vfs::read_raw_picture(ge::vfs::wad_lump(wad_bytes, *e), 128, 128, &p, &bad)
              : !ge::vfs::read_picture(ge::vfs::wad_lump(wad_bytes, *e), &p, &bad))
      return false;
    *img = colour(p, palette, sheet ? 0 : 255);
    ui::Image big;
    const uint32_t k = upgrade ? upgrade(name, p.width, p.height, &big) : 0;
    if (density == 0) density = k == 0 ? 1 : k;
    if (k == density && k > 1)
      *img = std::move(big);
    else
      *img = ui::scaled_nearest(*img, static_cast<int>(density));
    return true;
  };
  if (!classic_art(pictures, "the packs' gfx.wad", out, error)) return false;
  out->density = static_cast<float>(density);
  out->finish();
  return true;
}

const HudArt& pick_art(std::string_view hud_art, const HudArt& classic, const HudArt& free) {
  if (hud_art == "free" || classic.bar.empty()) return free;
  return classic;
}

void add_art_cvar(ge::console::Console& con, std::string* value) {
  con.cvars().bind({.name = "hud_art",
                    .fallback = "auto",
                    .help = "the HUD's pictures: auto (the packs' own status bar pictures when "
                            "they carry them, else the game's), classic (the packs') or free "
                            "(the game's own)",
                    .kind = ge::console::CvarKind::text},
                   value);
}

}  // namespace shooters_edge::hud
