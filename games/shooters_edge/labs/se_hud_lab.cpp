// LAB `se-hud`: Shooter's Edge's three HUD looks (games/shooters_edge/hud/layout.hpp) drawn by the
// CPU raster over a painted arena at desktop, wide and phone sizes, with the free art painted in
// code:
//
//   - CLASSIC: the status bar and the inventory bar along the bottom centre, scaled by whole
//     numbers and never wider than the screen;
//   - MODERN: health and armour bottom left, the ammunition bottom right, the guns along the
//     bottom, the powerups down the right edge, the frags top right;
//   - COMPACT: two arcs round the crosshair with their figures, the ammunition and the powerups
//     under it, the keys over it, small guns along the bottom.
//
// Two moments are staged: a fight (a gun just picked up and flashing, the damage boost running,
// a gold key, a frag board of four) and trouble (health, armour and ammunition low, the cloak's
// last seconds, the portrait wincing). Each look must lay out clean at every size: no box off the
// screen, and no two boxes overlapping unless one hangs from the other or both are drawn
// concentric on purpose (the arcs). A close-up of the classic bars shows each moment at the
// bar's own scale. With `--classic=<folder>` every look is drawn again in the player's own
// classic pictures from classic-enhance's local cache (classic_art.hpp): `<cache>/pictures` as
// the pack has them, `<cache>/pictures/enhanced` upscaled (`ce_pictures`).
//
// ge:summary Lab: Shooter's Edge's classic, modern and compact HUDs at five screen sizes
// ge:keywords lab hud se-hud shooter status-bar classic modern compact looks viewports
// ge:keywords free-art layout-problems screenshot
// ge:skill ui labs
// ge:test tests/shooters_edge_hud_test.cpp
#include <initializer_list>
#include <string>
#include <string_view>
#include <vector>

#include "console/console.hpp"
#include "core/num_text.hpp"
#include "hud/layout.hpp"
#include "lab/lab.hpp"
#include "shooters_edge/hud/art.hpp"
#include "shooters_edge/hud/classic_art.hpp"
#include "shooters_edge/hud/layout.hpp"
#include "ui/raster.hpp"

namespace {

// The parts in one string, appended into one buffer.
std::string join(std::initializer_list<std::string_view> parts) {
  std::string out;
  for (std::string_view p : parts) out += p;
  return out;
}

namespace se = shooters_edge::hud;
namespace gh = ge::hud;
namespace ui = ge::ui;

struct Viewport {
  int w, h;
  std::string name() const { return std::to_string(w) + "x" + std::to_string(h); }
};

std::vector<Viewport> parse_viewports(const std::string& text) {
  std::vector<Viewport> out;
  size_t at = 0;
  while (at < text.size()) {
    const size_t comma = text.find(',', at);
    const std::string item =
        text.substr(at, comma == std::string::npos ? std::string::npos : comma - at);
    const size_t x = item.find('x');
    if (x != std::string::npos)
      out.push_back({std::stoi(item.substr(0, x)), std::stoi(item.substr(x + 1))});
    if (comma == std::string::npos) break;
    at = comma + 1;
  }
  return out;
}

// A dim stone arena: a wall, a floor, a doorway's light. The HUD's figures must read over both
// the dark and the lit parts.
ui::Image arena(int w, int h) {
  ui::DrawList dl;
  dl.begin(w, h);
  const float fw = static_cast<float>(w), fh = static_cast<float>(h);
  ui::Box b;
  const ui::Stop wall[2] = {{0, ui::hex(0x2a2420)}, {1, ui::hex(0x4a3e34)}};
  b.rect = {0, 0, fw, fh * 0.6f};
  b.fill = ui::linear(0, 0, 0, b.rect.h, wall, 2);
  dl.box(b);
  const ui::Stop floor[2] = {{0, ui::hex(0x3a3028)}, {1, ui::hex(0x16120e)}};
  b.rect = {0, fh * 0.6f, fw, fh * 0.4f};
  b.fill = ui::linear(0, 0, 0, b.rect.h, floor, 2);
  dl.box(b);
  for (int k = 0; k < 12; ++k)
    dl.rect({0, fh * 0.05f * static_cast<float>(k), fw, 1.5f}, ui::hex(0x000000, 0.25f));
  const ui::Stop door[2] = {{0, ui::hex(0xffd9a0)}, {1, ui::hex(0xa86a30)}};
  b.rect = {fw * 0.58f, fh * 0.22f, fw * 0.12f, fh * 0.38f};
  b.fill = ui::linear(0, 0, 0, b.rect.h, door, 2);
  b.radius = fw * 0.02f;
  dl.box(b);
  ui::Image img = ui::make_image(w, h, ui::hex(0x000000));
  ui::render(dl, img);
  return img;
}

se::Status fight() {
  se::Status s;
  s.t = 40;
  s.health = 64, s.armour = 87, s.armour_class = se::ArmourClass::medium;
  s.ammo = {34, 112, 23, 45};
  s.weapon = se::Weapon::rocket_launcher;
  s.owned = 0x7f;
  s.picked_at[2] = 39.73;  // the nailgun, a third of a second ago: flashing
  s.key_gold = true;
  s.power_s[static_cast<size_t>(se::Power::damage)] = 12.4;
  s.runes = 0x5;
  s.frags = 17;
  s.board = {{21, ui::hex(0x3a6ad0), ui::hex(0x3a6ad0), false},
             {17, ui::hex(0xd04a2a), ui::hex(0xe8c03a), true},
             {9, ui::hex(0x4ab04a), ui::hex(0x2a2a2a), false},
             {4, ui::hex(0xa040c0), ui::hex(0xd0d0d0), false}};
  return s;
}

se::Status trouble() {
  se::Status s;
  s.t = 80;
  s.health = 18, s.armour = 9, s.armour_class = se::ArmourClass::light;
  s.ammo = {6, 0, 0, 0};
  s.weapon = se::Weapon::double_shotgun;
  s.owned = 0x03;
  s.hurt_at = 79.9;  // hit a tenth of a second ago: the portrait winces
  s.key_silver = true;
  s.power_s[static_cast<size_t>(se::Power::cloak)] = 3.2;
  s.frags = 2;
  s.board = {{11, ui::hex(0x3a6ad0), ui::hex(0x3a6ad0), false},
             {2, ui::hex(0xd04a2a), ui::hex(0xe8c03a), true}};
  return s;
}

bool same_box(const ui::Rect& a, const ui::Rect& b) {
  return a.x == b.x && a.y == b.y && a.w == b.w && a.h == b.h;
}

// Whether element `a` hangs from `b`, at any depth.
bool hangs_from(const gh::Layout& L, int a, int b) {
  for (int at = a, guard = 0; at >= 0 && guard < 16; ++guard) {
    const int p = L[static_cast<size_t>(at)].place.parent.empty()
                      ? -1
                      : L.find(L[static_cast<size_t>(at)].place.parent);
    if (p == b) return true;
    at = p;
  }
  return false;
}

// The layout check, less what is drawn on purpose: a child over its parent, concentric boxes.
std::vector<std::string> problems(const gh::Layout& L, const gh::Frame& f,
                                  const std::vector<gh::Placed>& placed) {
  std::vector<std::string> out;
  for (size_t i = 0; i < placed.size(); ++i) {
    const std::vector<std::string> one = gh::layout_problems(f, {placed[i]});
    out.insert(out.end(), one.begin(), one.end());
    for (size_t j = i + 1; j < placed.size(); ++j) {
      const int a = L.find(placed[i].name), b = L.find(placed[j].name);
      if (hangs_from(L, a, b) || hangs_from(L, b, a) || same_box(placed[i].rect, placed[j].rect))
        continue;
      const std::vector<std::string> two = gh::layout_problems(f, {placed[i], placed[j]});
      out.insert(out.end(), two.begin(), two.end());
    }
  }
  return out;
}

ge::lab::Picture picture_of(ui::Image img) {
  ge::lab::Picture pic;
  pic.width = img.width, pic.height = img.height, pic.rgba = std::move(img.rgba);
  return pic;
}

// The bottom `h` rows of `img`, `w` wide about its middle.
ui::Image crop_bottom(const ui::Image& img, int w, int h) {
  ui::Image out = ui::make_image(w, h, ui::hex(0x000000));
  const int x0 = (img.width - w) / 2, y0 = img.height - h;
  for (int y = 0; y < h; ++y)
    for (int x = 0; x < w; ++x)
      for (int k = 0; k < 4; ++k)
        out.rgba[(static_cast<size_t>(y) * w + x) * 4 + k] =
            img.rgba[(static_cast<size_t>(y0 + y) * img.width + x0 + x) * 4 + k];
  return out;
}

constexpr const char* kLookCaption[se::kLooks] = {
    "The classic status bar across the bottom centre, scaled by a whole number of px per art "
    "pixel: armour, the portrait, health and the ammunition on the bar; the four ammunition "
    "counts, the guns (the one in hand lit, a new one flashing), the keys and powerups, the "
    "runes and the frag plates on the inventory bar above it.",
    "The corners: health and armour bottom left (icon, figure, bar), the keys above them, the "
    "ammunition bottom right, the guns along the bottom with their rounds, the powerups down the "
    "right edge with their seconds, the frags top right.",
    "Round the crosshair: health's arc on the left and armour's on the right with their figures, "
    "the ammunition under it and the powerups with their seconds under that, the keys over it, "
    "small guns along the bottom."};

void run(ge::lab::Lab& lab) {
  const gh::Style style = se::hud_style();
  // The free set always; the player's classic pictures too when the flag names their folder.
  std::vector<std::pair<std::string, se::HudArt>> sets;
  sets.emplace_back("free", se::free_art());
  if (!lab.arg("classic").empty()) {
    se::HudArt classic;
    std::string error;
    if (se::classic_art(lab.arg("classic"), &classic, &error))
      sets.emplace_back("classic", std::move(classic));
    else
      lab.fail(error, "point --classic at classic-enhance's cache folder `pictures/`. Skill: ui.");
  }
  for (auto& [name, art] : sets) art.finish();  // moved: point the sheet at its new home
  se::Hud hud = se::make_hud();
  ge::console::Console console;
  se::add_cvars(console, hud);
  hud.notify.push("you got the nailgun", style.ink, 39.7);
  hud.notify.push("you got 25 health", style.good, 39.2);
  const std::vector<Viewport> viewports = parse_viewports(lab.arg("viewports"));
  const struct {
    const char* id;
    se::Status status;
  } moments[2] = {{"fight", fight()}, {"trouble", trouble()}};

  int total_problems = 0;
  for (const auto& [set_name, art] : sets) {
    const std::string tag = set_name == "free" ? "" : "-" + set_name;
    for (int li = 0; li < se::kLooks; ++li) {
      const auto look = static_cast<se::Look>(li);
      const std::string look_word(se::look_name(look));
      console.execute("hud_style " + look_word);
      for (const auto& m : moments) {
        const std::string moment = m.id;
        if (moment == "trouble") {
          hud.centre = "the cloak is fading";
          hud.centre_until = m.status.t + 2;
        } else {
          hud.centre.clear();
        }
        for (const Viewport& v : viewports) {
          se::fill_hud(hud, m.status, art);
          const gh::Frame f = hud.layout.frame(v.w, v.h, style);
          ui::DrawList dl;
          dl.begin(v.w, v.h);
          const std::vector<gh::Placed> placed = hud.layout.draw(dl, f, style);
          const std::vector<std::string> found = problems(hud.layout, f, placed);
          total_problems += static_cast<int>(found.size());
          for (const std::string& p : found)
            lab.fail(join({"the ", look_word, " look at ", v.name(), " (", moment, ", ", set_name,
                           " art): ", p}),
                     "move or shrink the element in games/shooters_edge/hud/layout.cpp (table); "
                     "rerun `make lab LAB=se-hud`. Skill: ui.");
          lab.metric(join({look_word, tag, ".", moment, ".", v.name(), ".boxes"}),
                     static_cast<double>(placed.size()), "count");
          // The classic bar's whole-number scale, px per art pixel.
          if (look == se::Look::classic) {
            const int bar = hud.layout.find("sbar");
            lab.metric(join({"classic", tag, ".", v.name(), ".bar-unit"}),
                       hud.layout[static_cast<size_t>(bar)].drawn_unit, "px");
          }
          ui::Image img = arena(v.w, v.h);
          ui::render(dl, img);
          const bool closeup = look == se::Look::classic && v.w == 1920 && v.h == 1080;
          ui::Image close = closeup ? crop_bottom(img, 1400, 230) : ui::Image{};
          lab.shot({join({look_word, tag, "-", moment, "-", v.name()}),
                    join({"THE ", look_word, " LOOK, ", moment, ", AT ", v.name(),
                          v.h > v.w ? " (UPRIGHT)" : ""}),
                    kLookCaption[li],
                    moment == "fight"
                        ? "nothing off the screen or overlapping; the nailgun flashing; the damage "
                          "boost's seconds; the frag board (classic) or FRAGS 17"
                        : "health 18, armour 9 and the 6 shells in the alarm colour; the portrait "
                          "wincing; the cloak's seconds; the centre message under the corners",
                    {{"look", look_word},
                     {"moment", moment},
                     {"viewport", v.name()},
                     {"art", set_name},
                     {"scale", ge::core::num::to_fixed(f.scale, 2)},
                     {"problems", std::to_string(found.size())}}},
                   picture_of(std::move(img)));
          if (closeup)
            lab.shot({join({"classic", tag, "-", moment, "-closeup"}),
                      "THE CLASSIC BARS CLOSE UP, " + moment,
                      join({"The bottom of the 1920x1080 frame at the bar's own scale (4 px per "
                            "art pixel), in the ",
                            set_name, " art."}),
                      "each figure in its field; the icons in their wells; the counts over their "
                      "boxes",
                      {{"look", "classic"}, {"moment", moment}, {"art", set_name}}},
                     picture_of(std::move(close)));
        }
      }
    }
  }
  lab.metric("problems", total_problems, "count");
  lab.metric("elements", static_cast<double>(hud.layout.size()), "count");
}

const ge::lab::LabSpec kSpec{
    "se-hud",
    ge::lab::LabKind::picture,
    "game:shooters_edge",
    "games/shooters_edge/hud (the looks, the free art) over engine/hud's layout and pictures",
    "ui",
    "Do Shooter's Edge's classic, modern and compact HUDs lay out clean and read at desktop, "
    "wide and phone sizes?",
    {{"viewports", "1280x720,1920x1080,2560x1080,844x390,390x844",
      "the screen sizes, comma-separated WxH"},
     {"classic", "",
      "classic-enhance's pictures folder in the local cache: the looks drawn in the player's own "
      "classic pictures too (never committed or published beyond a private page)"}},
    "--viewports=1280x720,390x844",
    run,
};
GE_LAB(kSpec);

}  // namespace
