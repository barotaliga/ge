// The classic pictures read: each slot of the HUD's set by the pack's picture name, decoded from
// the cache's PNG into the 2D layer's image.
//
// ge:summary The classic HUD pictures read from the cache's PNGs by the pack's names
// ge:keywords hud classic-art png decode names slots cell-of
// ge:skill ui
// ge:test tests/shooters_edge_hud_test.cpp
#include "shooters_edge/hud/classic_art.hpp"

#include <array>
#include <fstream>
#include <iterator>
#include <string_view>
#include <vector>

#include "assets/image.hpp"

namespace shooters_edge::hud {

namespace {

namespace ui = ge::ui;

bool load(const std::filesystem::path& dir, std::string_view name, ui::Image* out,
          std::string* error) {
  const std::filesystem::path path = dir / (std::string(name) + ".png");
  std::ifstream in(path, std::ios::binary);
  const std::vector<uint8_t> bytes((std::istreambuf_iterator<char>(in)),
                                   std::istreambuf_iterator<char>());
  const std::optional<ge::assets::Image> img = ge::assets::decode_png(bytes);
  if (!img) {
    if (error != nullptr)
      *error = "classic_art: " + path.string() +
               " is missing or not a PNG; run classic-enhance's pictures stage on the pack "
               "(ge tool classicenhance -- run <pack>)";
    return false;
  }
  out->width = img->width;
  out->height = img->height;
  out->rgba = img->rgba;
  return true;
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

bool classic_art(const std::filesystem::path& dir, HudArt* out, std::string* error) {
  HudArt a;
  a.density = 1;
  bool ok =
      load(dir, "sbar", &a.bar, error) && load(dir, "ibar", &a.inventory_bar, error) &&
      load(dir, "disc", &a.armour_shield, error) && load(dir, "face_invis", &a.face_cloak, error) &&
      load(dir, "face_invul2", &a.face_shield, error) &&
      load(dir, "face_inv2", &a.face_cloak_shield, error) &&
      load(dir, "face_quad", &a.face_damage, error) && load(dir, "conchars", &a.glyphs, error);
  for (size_t k = 0; ok && k < 3; ++k)
    ok = load(dir, "sb_armor" + std::to_string(k + 1), &a.armour[k], error);
  constexpr std::array<std::string_view, kAmmoKinds> kAmmo = {"sb_shells", "sb_nails", "sb_rocket",
                                                              "sb_cells"};
  for (size_t k = 0; ok && k < kAmmo.size(); ++k) ok = load(dir, kAmmo[k], &a.ammo[k], error);
  // Band 0 is the nearest death: the pack numbers its portraits from the healthiest.
  for (size_t b = 0; ok && b < kFaceBands; ++b) {
    const std::string n = std::to_string(kFaceBands - b);
    ok = load(dir, "face" + n, &a.face[b][0], error) &&
         load(dir, "face_p" + n, &a.face[b][1], error);
  }
  for (size_t k = 0; ok && k < kGuns; ++k) {
    const std::string g(kGunNames[k]);
    ok = load(dir, "inv_" + g, &a.gun[k], error) && load(dir, "inv2_" + g, &a.gun_lit[k], error);
    for (size_t f = 0; ok && f < kFlashFrames; ++f)
      ok = load(dir, "inva" + std::to_string(f + 1) + "_" + g, &a.gun_flash[k][f], error);
  }
  for (size_t k = 0; ok && k < 2; ++k)
    ok = load(dir, "sb_key" + std::to_string(k + 1), &a.key[k], error);
  constexpr std::array<std::string_view, kPowers> kPower = {"sb_invis", "sb_invuln", "sb_suit",
                                                            "sb_quad"};
  for (size_t k = 0; ok && k < kPower.size(); ++k) ok = load(dir, kPower[k], &a.power[k], error);
  for (size_t k = 0; ok && k < 4; ++k)
    ok = load(dir, "sb_sigil" + std::to_string(k + 1), &a.rune[k], error);
  for (size_t k = 0; ok && k < 11; ++k) {
    const std::string d = k < 10 ? std::to_string(k) : "minus";
    ok =
        load(dir, "num_" + d, &a.digit[k], error) && load(dir, "anum_" + d, &a.digit_low[k], error);
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

}  // namespace shooters_edge::hud
