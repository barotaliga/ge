// games/shooters_edge/hud: the free set has every picture; hud_style picks a look and only its
// elements show; the classic bar's figures turn to the alarm sheet when low and the shield shows
// 666; the portrait follows the health bands, winces when hurt and takes a powerup's look; a gun
// picked up flashes for a second; every look lays out clean at a desktop and an upright phone;
// the classic set loads from a folder of PNGs by the pack's names and a missing one is named.
//
// ge:summary Tests Shooter's Edge's HUDs: looks, the classic bar's figures, portrait, flash, art
// ge:keywords test hud shooter looks hud_style classic modern compact free-art classic-art
// ge:keywords portrait flash low-figures shield layout-problems
// ge:skill ui testing
#include <filesystem>
#include <string>
#include <vector>

#include "check.hpp"
#include "console/console.hpp"
#include "hud/layout.hpp"
#include "lab/png.hpp"
#include "shooters_edge/hud/art.hpp"
#include "shooters_edge/hud/classic_art.hpp"
#include "shooters_edge/hud/layout.hpp"

namespace se = shooters_edge::hud;
namespace gh = ge::hud;
namespace ui = ge::ui;

namespace {

const se::HudArt& art() {
  static const se::HudArt kArt = [] {
    se::HudArt a = se::free_art();
    a.finish();
    return a;
  }();
  return kArt;
}

const gh::Content& content(se::Hud& hud, const char* id) {
  return hud.layout.content(hud.layout.find(id));
}

std::vector<gh::Placed> draw(se::Hud& hud, int w, int h) {
  const gh::Style st = se::hud_style();
  ui::DrawList dl;
  dl.begin(w, h);
  return hud.layout.draw(dl, hud.layout.frame(w, h, st), st);
}

se::Status armed() {
  se::Status s;
  s.t = 10;
  s.health = 64, s.armour = 87, s.armour_class = se::ArmourClass::medium;
  s.ammo = {34, 112, 23, 45};
  s.weapon = se::Weapon::rocket_launcher;
  s.owned = 0x7f;
  s.key_gold = true;
  s.power_s[3] = 12;
  s.frags = 3;
  return s;
}

}  // namespace

TEST(the_free_set_has_every_picture) {
  const se::HudArt& a = art();
  CHECK(a.density == 4);
  CHECK(a.bar.width == 320 * 4 && a.bar.height == 24 * 4);
  CHECK(a.gun[6].width == 48 * 4);  // the beam gun is the wide one
  for (const auto& band : a.face)
    for (const ui::Image& f : band) CHECK(f.width == 24 * 4);
  for (size_t k = 0; k < 11; ++k) {
    CHECK(a.sheet.glyph[k] == &a.digit[k]);
    CHECK(a.sheet.alt[k] == &a.digit_low[k]);
  }
  CHECK(!a.crosshair.empty() && !a.health.empty());
}

TEST(hud_style_picks_a_look_and_only_its_elements_show) {
  se::Hud hud = se::make_hud();
  ge::console::Console con;
  se::add_cvars(con, hud);
  se::fill_hud(hud, armed(), art());
  CHECK(content(hud, "sbar").on && !content(hud, "health").on && !content(hud, "ring_health").on);
  con.execute("hud_style modern");
  se::fill_hud(hud, armed(), art());
  CHECK(!content(hud, "sbar").on && content(hud, "health").on && !content(hud, "ring_health").on);
  con.execute("hud_style compact");
  se::fill_hud(hud, armed(), art());
  CHECK(!content(hud, "sbar").on && !content(hud, "health").on && content(hud, "ring_health").on);
  con.execute("hud_style nonsense");  // an unknown word is the classic look
  se::fill_hud(hud, armed(), art());
  CHECK(content(hud, "sbar").on);
}

TEST(the_classic_figures_turn_to_the_alarm_sheet_when_low_and_the_shield_shows_666) {
  se::Hud hud = se::make_hud();
  se::Status s = armed();
  se::fill_hud(hud, s, art());
  CHECK(!content(hud, "sbar_health").alt && !content(hud, "sbar_ammo").alt);
  s.health = 25, s.armour = 10, s.ammo[2] = 10;
  se::fill_hud(hud, s, art());
  CHECK(content(hud, "sbar_health").alt && content(hud, "sbar_armour").alt);
  CHECK(content(hud, "sbar_ammo").alt);
  s.power_s[1] = 20;  // the shield
  se::fill_hud(hud, s, art());
  CHECK(content(hud, "sbar_armour").value == "666" && !content(hud, "sbar_armour").alt);
  CHECK(content(hud, "sbar_armour_icon").image == &art().armour_shield);
}

TEST(the_portrait_follows_the_health_bands_winces_and_takes_a_powerups_look) {
  se::Hud hud = se::make_hud();
  se::Status s = armed();
  s.power_s = {};
  s.health = 100;
  se::fill_hud(hud, s, art());
  CHECK(content(hud, "sbar_face").image == &art().face[4][0]);
  s.health = 19;
  s.hurt_at = s.t - 0.1;
  se::fill_hud(hud, s, art());
  CHECK(content(hud, "sbar_face").image == &art().face[0][1]);
  s.power_s[0] = 5;  // the cloak
  se::fill_hud(hud, s, art());
  CHECK(content(hud, "sbar_face").image == &art().face_cloak);
}

TEST(a_gun_picked_up_flashes_for_a_second) {
  se::Hud hud = se::make_hud();
  se::Status s = armed();
  s.picked_at[2] = s.t - 0.25;
  se::fill_hud(hud, s, art());
  CHECK(content(hud, "ibar_guns").cells[2].image == &art().gun_flash[2][2]);
  CHECK(content(hud, "ibar_guns").cells[5].image == &art().gun_lit[5]);  // the one in hand
  s.picked_at[2] = s.t - 1.5;
  se::fill_hud(hud, s, art());
  CHECK(content(hud, "ibar_guns").cells[2].image == &art().gun[2]);
}

TEST(every_look_lays_out_on_the_screen_at_a_desktop_and_an_upright_phone) {
  for (const char* look : {"classic", "modern", "compact"}) {
    se::Hud hud = se::make_hud();
    ge::console::Console con;
    se::add_cvars(con, hud);
    con.execute(std::string("hud_style ") + look);
    se::fill_hud(hud, armed(), art());
    for (const auto& [w, h] : {std::pair{1280, 720}, std::pair{390, 844}}) {
      const auto placed = draw(hud, w, h);
      CHECK(!placed.empty());
      const gh::Frame f = hud.layout.frame(w, h, se::hud_style());
      for (const gh::Placed& p : placed) CHECK(gh::layout_problems(f, {p}).empty());
    }
  }
}

TEST(the_classic_set_loads_by_the_packs_names_and_a_missing_one_is_named) {
  const std::filesystem::path dir =
      std::filesystem::temp_directory_path() / "ge_se_hud_classic_art_test";
  std::filesystem::remove_all(dir);
  std::filesystem::create_directories(dir);
  ge::lab::Picture tiny;
  tiny.width = 24, tiny.height = 24;
  tiny.rgba.assign(size_t{24} * 24 * 4, 200);
  std::vector<std::string> names = {"sbar",        "ibar",      "disc",      "face_invis",
                                    "face_invul2", "face_inv2", "face_quad", "conchars",
                                    "sb_shells",   "sb_nails",  "sb_rocket", "sb_cells"};
  for (int k = 1; k <= 3; ++k) names.push_back("sb_armor" + std::to_string(k));
  for (int k = 1; k <= 5; ++k) {
    names.push_back("face" + std::to_string(k));
    names.push_back("face_p" + std::to_string(k));
  }
  for (const char* g :
       {"shotgun", "sshotgun", "nailgun", "snailgun", "rlaunch", "srlaunch", "lightng"}) {
    names.push_back(std::string("inv_") + g);
    names.push_back(std::string("inv2_") + g);
    for (int f = 1; f <= 5; ++f) names.push_back("inva" + std::to_string(f) + "_" + g);
  }
  for (const char* n : {"sb_key1", "sb_key2", "sb_invis", "sb_invuln", "sb_suit", "sb_quad"})
    names.emplace_back(n);
  for (int k = 1; k <= 4; ++k) names.push_back("sb_sigil" + std::to_string(k));
  for (const char* d : {"0", "1", "2", "3", "4", "5", "6", "7", "8", "9", "minus"}) {
    names.push_back(std::string("num_") + d);
    names.push_back(std::string("anum_") + d);
  }
  for (const std::string& n : names) {
    ge::lab::Picture p = tiny;
    if (n == "conchars") {
      p.width = p.height = 128;
      p.rgba.assign(size_t{128} * 128 * 4, 255);
    }
    CHECK(ge::lab::write_png((dir / (n + ".png")).string(), p));
  }
  se::HudArt a;
  std::string error;
  CHECK(se::classic_art(dir, &a, &error));
  CHECK(a.density == 1 && a.digits_at == 18);
  CHECK(a.crosshair.width == 8 && a.crosshair.height == 8);  // the sheet's '+' cell
  CHECK(a.count_face.sheet == &a.glyphs);
  std::filesystem::remove(dir / "face_p3.png");
  CHECK(!se::classic_art(dir, &a, &error));
  CHECK(error.find("face_p3.png") != std::string::npos);
  std::filesystem::remove_all(dir);
}
