// LAB `se-hud`: Shooter's Edge's six HUD looks (games/shooters_edge/hud/layout.hpp) drawn by the
// CPU raster over a painted arena at desktop, wide and phone sizes, with the free art painted in
// code:
//
//   - CLASSIC: the status bar and the inventory bar along the bottom centre, scaled by whole
//     numbers and never wider than the screen;
//   - MODERN: health and armour bottom left, the rounds bottom right with the ammunition and the
//     guns over them, the powerups down the right edge, the frags top right;
//   - COMPACT: an arch at the bottom centre, health's arc and armour's, their figures inside it,
//     the rounds by its right foot, the keys and powerups over it, small guns on the right edge;
//   - ARENA, TACTICAL and MINIMAL: three modern layouts (layout.hpp says where each puts what).
//
// Two moments are staged: a fight (a gun just picked up and flashing, the damage boost running,
// a gold key, a frag board of four) and trouble (health, armour and ammunition low, the cloak's
// last seconds, the portrait wincing). Each look must lay out clean at every size: no box off the
// screen, and no two boxes overlapping unless one hangs from the other or both are drawn
// concentric on purpose (the arcs). A close-up of the classic bars shows each moment at the
// bar's own scale. With `--classic=<folder>` every look is drawn again in the player's own
// classic pictures from classic-enhance's local cache (classic_art.hpp): `<cache>/pictures` as
// the pack has them, `<cache>/pictures/enhanced` upscaled (`ce_pictures`). Then every look again
// under each option (`kOptions`: hud_vitals, hud_sbar, hud_guns, hud_guns_all and hud_ammo_all),
// checked at every size and pictured at the first. Last, a PLAYER'S HUD CONFIG (`--cfg=<file>`,
// a sample by default): the modern look before and after it, every element it names moved or gone.
//
// ge:summary Lab: Shooter's Edge's six HUD looks and their options at five screen sizes
// ge:keywords lab hud se-hud shooter status-bar classic modern compact looks viewports
// ge:keywords hud_sbar hud_vitals hud_guns arena tactical minimal
// ge:keywords free-art layout-problems screenshot hud-config player-cfg configured
// ge:skill ui labs
// ge:test tests/shooters_edge_hud_test.cpp
#include <fstream>
#include <initializer_list>
#include <sstream>
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

    ("The classic status bar across the bottom centre, scaled by a whole number of px per art "
     "pixel: armour, the portrait, health and the ammunition on the bar; the four ammunition "
     "counts, the guns (the one in hand lit, a new one flashing), the keys and powerups, the "
     "runes and the frag plates on the inventory bar above it."),
    ("The corners: health and armour bottom left (icon, figure, bar), the keys above them, the "
     "rounds bottom right with the ammunition counts and the guns in pairs over them, the powerups "
     "down the "
     "right edge with their seconds, the frags top right."),
    ("An arch at the bottom centre: health's arc rising from the left and armour's from the right, "
     "their figures inside it either side of the weapon in hand, the rounds by its right foot, the "
     "keys and powerups over it, small guns on the right edge."),
    ("The arena look: health and armour big either side of the portrait at the bottom centre with "
     "thin bars over them, the rounds under the crosshair, the guns and the four ammunition counts "
     "down the right edge, the powerups across the top."),
    ("The tactical look: health beside its bar with the armour as a shield bar over it bottom "
     "left; the rounds big bottom right beside the gun in hand, the ammunition kinds and the gun "
     "slots over them; the powerups bottom centre."),
    ("The minimal look: small faded figures only, health and armour bottom left, the rounds bottom "
     "right, the keys and powerups while held.")};

// An option the player sets over a look, and the line that sets it back.
struct Option {
  const char* id;
  const char* line;
  const char* undo;
  std::string_view looks;  // the looks it is pictured over; empty: every one
  const char* caption;
};

constexpr Option kOptions[] = {
    {"vitals-right", "hud_vitals right", "hud_vitals left", "",
     "hud_vitals right: health and armour on the right, the rest on the left; the classic "
     "bars stay put."},
    {"bare", "hud_sbar 0", "hud_sbar 1", "classic",
     "hud_sbar 0: the classic bars' pieces at their places with neither panel drawn."},
    {"by-ammo", "hud_guns ammo", "hud_guns look", "modern arena tactical compact",
     "hud_guns ammo: the guns two to a line by the ammunition they share (the lightning gun "
     "alone), each line led by its ammunition and count."},
    {"pairs", "hud_guns pairs", "hud_guns look", "modern arena tactical compact",
     "hud_guns pairs: the same 2-2-2-1 lines of guns, the ammunition counts apart."},
    {"row", "hud_guns row", "hud_guns look", "modern arena tactical compact",
     "hud_guns row: the guns in one row, the ammunition counts apart."},
    {"owned", "hud_guns_all 0; hud_ammo_all 0", "hud_guns_all 1; hud_ammo_all 1",
     "modern arena tactical",
     "hud_guns_all 0 and hud_ammo_all 0: only the guns held and the ammunition carried."},
};

// A PLAYER'S HUD CONFIG: the layout is cvars, so a HUD config is a config. This one moves the
// frags to the top left, the powerups to the left edge, shrinks the whole HUD a little, enlarges
// the crosshair and hides the pickup feed, as a player's cfg would.
constexpr std::string_view kPlayerHud =
    "// my hud\n"
    "hud_style modern\n"
    "hud_scale 0.9\n"
    "hud_frags_anchor top_left\n"
    "hud_frags_x 0.02\n"
    "hud_powers_anchor left\n"
    "hud_crosshair_scale 1.5\n"
    "hud_notify_show 0\n";

// Each box by its element's name.
const gh::Placed* placed_named(const std::vector<gh::Placed>& placed, std::string_view name) {
  for (const gh::Placed& p : placed)
    if (p.name == name) return &p;
  return nullptr;
}

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
  // The chat lines on screen (scr_chathud 2: the alternate colour), lower left in every look.
  ge::hud::Feed chat{4, -1, 1, 0.15};
  chat.push("bob: quad in 10", ui::Color{0.45f, 1, 0.45f, 1}, 30);
  chat.push("(eve): rl up, low", ui::Color{0.45f, 1, 0.45f, 1}, 35);
  hud.chat = &chat;
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
                          "boost's seconds; the frag board (classic) or FRAGS 17; two chat lines "
                          "in green at the lower left"
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
            lab.shot({join({look_word, tag, "-", moment, "-closeup"}),
                      join({"THE ", look_word, " BARS CLOSE UP, ", moment}),
                      join({"The bottom of the 1920x1080 frame at the bar's own scale (4 px per "
                            "art pixel), in the ",
                            set_name, " art."}),
                      "each figure in its field; the icons in their wells; the counts over their "
                      "boxes",
                      {{"look", look_word}, {"moment", moment}, {"art", set_name}}},
                     picture_of(std::move(close)));
        }
      }
    }
  }
  // THE OPTIONS: every look again under each option (hud_vitals, hud_guns, hud_guns_all,
  // hud_ammo_all, hud_sbar), checked at every size and pictured at the first, in the free art.
  const se::HudArt& free = sets.front().second;
  const Viewport& shown = viewports.front();
  for (const Option& o : kOptions) {
    for (int li = 0; li < se::kLooks; ++li) {
      const std::string look_word(se::look_name(static_cast<se::Look>(li)));
      if (!o.looks.empty() && o.looks.find(look_word) == std::string_view::npos) continue;
      console.execute("hud_style " + look_word);
      console.execute(std::string(o.line));
      hud.centre.clear();
      for (const Viewport& v : viewports) {
        se::fill_hud(hud, fight(), free);
        const gh::Frame f = hud.layout.frame(v.w, v.h, style);
        ui::DrawList dl;
        dl.begin(v.w, v.h);
        const std::vector<gh::Placed> placed = hud.layout.draw(dl, f, style);
        const std::vector<std::string> found = problems(hud.layout, f, placed);
        total_problems += static_cast<int>(found.size());
        for (const std::string& p : found)
          lab.fail(join({"the ", look_word, " look with `", o.line, "` at ", v.name(), ": ", p}),
                   "move or shrink the element in games/shooters_edge/hud/layout.cpp (table) or "
                   "its cells (fill_hud); rerun `make lab LAB=se-hud`. Skill: ui.");
        if (v.w != shown.w || v.h != shown.h) continue;
        ui::Image img = arena(v.w, v.h);
        ui::render(dl, img);
        lab.shot({join({look_word, "-", o.id, "-", v.name()}),
                  join({"THE ", look_word, " LOOK WITH ", o.line, ", AT ", v.name()}),
                  o.caption,
                  "nothing off the screen or overlapping; the option's change as the caption says",
                  {{"look", look_word},
                   {"option", o.line},
                   {"viewport", v.name()},
                   {"problems", std::to_string(found.size())}}},
                 picture_of(std::move(img)));
      }
      console.execute(std::string(o.undo));
    }
  }
  // THE PLAYER'S CONFIG: the modern look as the game sets it, then after the player's HUD cfg
  // (`--cfg=<file>` for one's own; kPlayerHud by default), at the first size. Each element the
  // cfg names must be drawn somewhere else, or not at all when it hides it.
  {
    std::string cfg(kPlayerHud);
    if (!lab.arg("cfg").empty()) {
      std::ifstream in(lab.arg("cfg"), std::ios::binary);
      if (!in) {
        lab.fail("cannot read the HUD config " + lab.arg("cfg"),
                 "point --cfg at a cfg file of hud_ settings. Skill: ui.");
      } else {
        std::ostringstream text;
        text << in.rdbuf();
        cfg = text.str();
      }
    }
    console.execute("hud_reset; hud_style modern; hud_scale 1");
    hud.centre.clear();
    hud.chat = nullptr;  // the player's cfg puts the powerups where the chat lines stand
    const Viewport& v = shown;
    std::vector<gh::Placed> before_boxes;
    for (int pass = 0; pass < 2; ++pass) {
      if (pass == 1) console.execute(cfg);
      se::fill_hud(hud, fight(), free);
      const gh::Frame f = hud.layout.frame(v.w, v.h, style);
      ui::DrawList dl;
      dl.begin(v.w, v.h);
      const std::vector<gh::Placed> placed = hud.layout.draw(dl, f, style);
      const std::vector<std::string> found = problems(hud.layout, f, placed);
      total_problems += static_cast<int>(found.size());
      for (const std::string& p : found)
        lab.fail(join({"the player's HUD config at ", v.name(), ": ", p}),
                 "move the element in the cfg, or in games/shooters_edge/hud/layout.cpp when the "
                 "game's own place is wrong; rerun `make lab LAB=se-hud`. Skill: ui.");
      int moved = 0, named = 0;
      if (pass == 1) {
        // Every element a line of the cfg names: drawn elsewhere, or gone.
        for (const gh::Placed& b : before_boxes) {
          if (cfg.find("hud_" + b.name + "_") == std::string::npos) continue;
          ++named;
          const gh::Placed* a = placed_named(placed, b.name);
          if (a == nullptr || a->rect.x != b.rect.x || a->rect.y != b.rect.y ||
              a->rect.w != b.rect.w || a->rect.h != b.rect.h) {
            ++moved;
          } else {
            lab.fail("the HUD config names " + b.name + " but it is drawn where it was",
                     "check the cvar's name with `cvarlist hud_" + b.name +
                         "` and that the look draws it. Skill: ui.");
          }
        }
        lab.metric("configured.named", named, "count");
        lab.metric("configured.moved", moved, "count");
      } else {
        before_boxes = placed;
      }
      ui::Image img = arena(v.w, v.h);
      ui::render(dl, img);
      const char* id = pass == 0 ? "configured-before" : "configured-after";
      lab.shot({join({id, "-", v.name()}),
                pass == 0 ? join({"THE MODERN LOOK AS THE GAME SETS IT, AT ", v.name()})
                          : join({"THE SAME AFTER THE PLAYER'S HUD CONFIG, AT ", v.name()}),
                pass == 0 ? "The game's own places, before the player's cfg runs."
                          : "After the cfg: the frags top left, the powerups on the left edge, "
                            "the HUD at 0.9, a larger crosshair, no pickup feed.",
                pass == 0 ? "the modern look's places"
                          : "each element the cfg names somewhere else, or gone; nothing "
                            "overlapping or off the screen",
                {{"viewport", v.name()}, {"problems", std::to_string(found.size())}}},
               picture_of(std::move(img)));
    }
    console.execute("hud_reset; hud_scale 1");
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
    "Do Shooter's Edge's six HUD looks and their options lay out clean and read at desktop, "
    "wide and phone sizes?",
    {{"viewports", "1280x720,1920x1080,2560x1080,844x390,390x844",
      "the screen sizes, comma-separated WxH"},
     {"classic", "",
      "classic-enhance's pictures folder in the local cache: the looks drawn in the player's own "
      "classic pictures too (never committed or published beyond a private page)"},
     {"cfg", "",
      "a player's HUD config (hud_ settings) pictured before and after; the lab's own "
      "sample config by default"}},
    "--viewports=1280x720,390x844",
    run,
};
GE_LAB(kSpec);

}  // namespace
