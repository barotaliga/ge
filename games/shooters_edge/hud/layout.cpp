// Shooter's Edge's HUDs defined: the element table (every look's, each element tagged with the
// looks it belongs to), hud_style, and the frame's fill from the status and the art.
//
// ge:summary Shooter's Edge's HUDs defined: the table, hud_style and the fill
// ge:keywords hud layout table looks hud_style fill_hud classic modern compact status-bar
// ge:keywords inventory-bar face flash low-figures frags notify centre
// ge:skill ui console
// ge:test tests/shooters_edge_hud_test.cpp
#include "shooters_edge/hud/layout.hpp"

#include <algorithm>
#include <array>
#include <utility>
#include <vector>

namespace shooters_edge::hud {

namespace gh = ge::hud;
namespace ui = ge::ui;

namespace {

using gh::Anchor;
using gh::Kind;

constexpr std::array<std::string_view, kLooks> kLookNames = {"classic", "modern", "compact"};
constexpr std::array<std::string_view, kAmmoKinds> kAmmoWords = {"SHELLS", "NAILS", "ROCKETS",
                                                                 "CELLS"};
constexpr std::string_view kFragsWord = "FRAGS";

constexpr uint8_t kClassic = 1, kModern = 2, kCompact = 4, kAll = 7;

uint8_t bit(Look l) { return static_cast<uint8_t>(1 << static_cast<int>(l)); }

// The elements, in table order; `kIds` names each and its looks.
enum Id : uint8_t {
  crosshair,
  sbar,
  sbar_armour_icon,
  sbar_armour,
  sbar_face,
  sbar_health,
  sbar_ammo_icon,
  sbar_ammo,
  ibar,
  ibar_ammo,
  ibar_guns,
  ibar_items,
  ibar_runes,
  ibar_frags,
  health_icon,
  health,
  health_bar,
  armour_icon,
  armour,
  armour_bar,
  keys,
  ammo_icon,
  ammo,
  guns,
  powers,
  ring_health,
  ring_armour,
  ring_health_value,
  ring_armour_value,
  ring_ammo,
  ring_keys,
  ring_powers,
  ring_guns,
  frags,
  notify,
  centre,
  kIdCount,
};

struct Row {
  gh::ElementSpec spec;
  uint8_t looks;
};

gh::StripShape strip(float cw, float ch, float gap = 0) {
  gh::StripShape s;
  s.cell_w = cw, s.cell_h = ch, s.gap = gap;
  return s;
}

std::vector<Row> table() {
  std::vector<Row> t(kIdCount);
  auto put = [&](Id id, gh::ElementSpec spec, uint8_t looks) { t[id] = {spec, looks}; };
  gh::ElementSpec e;

  e = {.id = "crosshair", .kind = Kind::picture, .anchor = Anchor::center, .help = "the crosshair"};
  e.unit = 1.5f;
  put(crosshair, e, kAll);

  // CLASSIC: the bar in art px, its pieces at their places on it.
  e = {.id = "sbar", .kind = Kind::picture, .anchor = Anchor::bottom, .help = "the status bar"};
  e.unit = 3, e.snap = true, e.fit = 1;
  put(sbar, e, kClassic);
  auto on_bar = [&](Id id, std::string_view name, Kind k, float x, std::string_view help,
                    std::string_view parent = "sbar") {
    gh::ElementSpec c{.id = name, .kind = k, .x = x, .help = help};
    c.unit = 3, c.snap = true, c.parent = parent;
    put(id, c, kClassic);
    return c;
  };
  on_bar(sbar_armour_icon, "sbar_armour_icon", Kind::picture, 0, "the bar's armour icon");
  on_bar(sbar_armour, "sbar_armour", Kind::digits, 24, "the bar's armour figure");
  on_bar(sbar_face, "sbar_face", Kind::picture, 112, "the bar's portrait");
  on_bar(sbar_health, "sbar_health", Kind::digits, 136, "the bar's health figure");
  on_bar(sbar_ammo_icon, "sbar_ammo_icon", Kind::picture, 224, "the bar's ammunition icon");
  on_bar(sbar_ammo, "sbar_ammo", Kind::digits, 248, "the bar's ammunition figure");
  {
    gh::ElementSpec c = on_bar(ibar, "ibar", Kind::picture, 0, "the inventory bar");
    c.attach = Anchor::bottom_left;
    put(ibar, c, kClassic);
  }
  {
    gh::ElementSpec c = on_bar(ibar_ammo, "ibar_ammo", Kind::strip, 0,
                               "the inventory bar's four ammunition counts", "ibar");
    c.strip = strip(48, 8);
    c.strip.text_at = Anchor::top_right, c.strip.text_dx = -18;
    put(ibar_ammo, c, kClassic);
  }
  {
    gh::ElementSpec c =
        on_bar(ibar_guns, "ibar_guns", Kind::strip, 0, "the inventory bar's guns", "ibar");
    c.y = 8, c.strip = strip(0, 16);
    put(ibar_guns, c, kClassic);
  }
  {
    gh::ElementSpec c = on_bar(ibar_items, "ibar_items", Kind::strip, 192,
                               "the inventory bar's keys and powerups", "ibar");
    c.y = 8, c.strip = strip(16, 16);
    put(ibar_items, c, kClassic);
  }
  {
    gh::ElementSpec c =
        on_bar(ibar_runes, "ibar_runes", Kind::strip, 288, "the inventory bar's runes", "ibar");
    c.y = 8, c.strip = strip(8, 16);
    put(ibar_runes, c, kClassic);
  }
  {
    gh::ElementSpec c = on_bar(ibar_frags, "ibar_frags", Kind::strip, 194,
                               "the inventory bar's four best frag counts", "ibar");
    c.strip = strip(28, 8, 4);
    c.strip.text_at = Anchor::center, c.strip.plate_h = 7;
    c.strip.frame = ui::hex(0xffffff);
    put(ibar_frags, c, kClassic);
  }

  // MODERN: the corners.
  e = {.id = "health_icon",
       .kind = Kind::picture,
       .anchor = Anchor::bottom_left,
       .help = "the health icon"};
  e.unit = 2;
  put(health_icon, e, kModern);
  auto beside = [](std::string_view name, Kind k, std::string_view parent, Anchor at, Anchor attach,
                   float x, float y, std::string_view help) {
    gh::ElementSpec c{.id = name, .kind = k, .anchor = at, .x = x, .y = y, .help = help};
    c.parent = parent, c.attach = attach;
    return c;
  };
  e = beside("health", Kind::big, "health_icon", Anchor::right, Anchor::left, 8, 0,
             "the health figure");
  e.scale = 0.62;
  put(health, e, kModern);
  e = beside("health_bar", Kind::bar, "health_icon", Anchor::top_left, Anchor::bottom_left, 0, -5,
             "the health bar");
  e.bar = {.w = 190, .h = 6};
  put(health_bar, e, kModern);
  e = {.id = "armour_icon",
       .kind = Kind::picture,
       .anchor = Anchor::bottom_left,
       .y = -0.03,
       .help = "the armour icon"};
  e.unit = 2;
  put(armour_icon, e, kModern);
  e = beside("armour", Kind::big, "armour_icon", Anchor::right, Anchor::left, 8, 0,
             "the armour figure");
  e.scale = 0.62;
  put(armour, e, kModern);
  e = beside("armour_bar", Kind::bar, "armour_icon", Anchor::top_left, Anchor::bottom_left, 0, -5,
             "the armour bar");
  e.bar = {.w = 190, .h = 6};
  put(armour_bar, e, kModern);
  e = beside("keys", Kind::strip, "armour_icon", Anchor::top_left, Anchor::bottom_left, 0, -16,
             "the keys");
  e.unit = 2, e.strip = strip(16, 16, 2);
  put(keys, e, kModern);
  e = {.id = "ammo_icon",
       .kind = Kind::picture,
       .anchor = Anchor::bottom_right,
       .help = "the ammunition icon"};
  e.unit = 2;
  put(ammo_icon, e, kModern);
  e = beside("ammo", Kind::big, "ammo_icon", Anchor::left, Anchor::right, -8, 0,
             "the ammunition figure");
  e.scale = 0.62;
  put(ammo, e, kModern);
  e = {.id = "guns", .kind = Kind::strip, .anchor = Anchor::bottom, .help = "the guns"};
  e.unit = 2, e.fit = 0.45f, e.strip = strip(0, 16, 3);
  e.strip.text_at = Anchor::bottom_right, e.strip.text_size = 7, e.strip.text_dy = 7;
  e.strip.frame = ui::hex(0xffd36a);
  put(guns, e, kModern);
  e = {.id = "powers", .kind = Kind::strip, .anchor = Anchor::right, .help = "the powerups"};
  e.unit = 2, e.strip = strip(16, 16, 4);
  e.strip.vertical = true, e.strip.text_at = Anchor::right, e.strip.text_dx = -19;
  put(powers, e, kModern);

  // COMPACT: round the crosshair.
  e = beside("ring_health", Kind::ring, "crosshair", Anchor::center, Anchor::center, 0, 0,
             "the health arc");
  e.ring = {.radius = 64, .width = 5, .from_deg = 215, .to_deg = 325};
  put(ring_health, e, kCompact);
  e = beside("ring_armour", Kind::ring, "crosshair", Anchor::center, Anchor::center, 0, 0,
             "the armour arc");
  e.ring = {.radius = 64, .width = 5, .from_deg = 35, .to_deg = 145, .reverse = true};
  put(ring_armour, e, kCompact);
  e = beside("ring_health_value", Kind::readout, "ring_health", Anchor::left, Anchor::right, -4, 0,
             "the health figure by its arc");
  e.scale = 0.6;
  put(ring_health_value, e, kCompact);
  e = beside("ring_armour_value", Kind::readout, "ring_health", Anchor::right, Anchor::left, 4, 0,
             "the armour figure by its arc");
  e.scale = 0.6;
  put(ring_armour_value, e, kCompact);
  e = beside("ring_ammo", Kind::readout, "ring_health", Anchor::bottom, Anchor::top, 0, 6,
             "the ammunition under the crosshair");
  e.scale = 0.6;
  put(ring_ammo, e, kCompact);
  e = beside("ring_keys", Kind::strip, "ring_health", Anchor::top, Anchor::bottom, 0, -6,
             "the keys over the crosshair");
  e.unit = 1.5f, e.strip = strip(16, 16, 2);
  put(ring_keys, e, kCompact);
  e = beside("ring_powers", Kind::strip, "ring_ammo", Anchor::bottom, Anchor::top, 0, 4,
             "the powerups under the ammunition");
  // A child takes its parent's scale: undo the ammunition's 0.6 so the icons match the keys.
  e.unit = 1.5f, e.scale = 1 / 0.6, e.strip = strip(16, 16, 12);
  e.strip.text_at = Anchor::right, e.strip.text_dx = 9;
  put(ring_powers, e, kCompact);
  e = {.id = "ring_guns",
       .kind = Kind::strip,
       .anchor = Anchor::bottom,
       .help = "the guns, small, along the bottom"};
  e.unit = 1.5f, e.strip = strip(0, 16, 3);
  e.strip.frame = ui::hex(0xffd36a);
  put(ring_guns, e, kCompact);

  e = {.id = "frags", .kind = Kind::readout, .anchor = Anchor::top_right, .help = "the frags"};
  put(frags, e, kModern | kCompact);
  e = {.id = "notify",
       .kind = Kind::feed,
       .anchor = Anchor::top_left,
       .help = "the pickups and frags feed"};
  e.feed_size = 18, e.feed_w = 420, e.feed_share = 0.6f;
  put(notify, e, kAll);
  e = {.id = "centre",
       .kind = Kind::banner,
       .anchor = Anchor::top,
       .y = 0.12,
       .help = "the centre message"};
  e.under_corners = true;
  put(centre, e, kAll);
  return t;
}

const std::vector<Row>& rows() {
  static const std::vector<Row> kRows = table();
  return kRows;
}

ui::Color low_or(bool low, ui::Color ok, const gh::Style& st) { return low ? st.bad : ok; }

ui::Color armour_colour(ArmourClass c) {
  switch (c) {
    case ArmourClass::light:
      return ui::hex(0x5ccf4a);
    case ArmourClass::medium:
      return ui::hex(0xf0c43a);
    case ArmourClass::heavy:
      return ui::hex(0xff5a40);
    case ArmourClass::none:
      break;
  }
  return ui::hex(0x9aa4ac);
}

// The portrait the status calls for (art.hpp).
const ui::Image* face_of(const Status& s, const HudArt& art) {
  const bool cloak = s.powered(Power::cloak), shield = s.powered(Power::shield);
  if (cloak && shield) return &art.face_cloak_shield;
  if (s.powered(Power::damage)) return &art.face_damage;
  if (cloak) return &art.face_cloak;
  if (shield) return &art.face_shield;
  const int band = s.health >= 100 ? kFaceBands - 1 : std::max(s.health, 0) / 20;
  const bool wince = s.t - s.hurt_at < 0.2;
  return &art.face[static_cast<size_t>(std::min(band, kFaceBands - 1))][wince ? 1 : 0];
}

// A gun's picture on the classic row: lit in hand, flashing for a second after its pickup.
const ui::Image* gun_picture(const Status& s, const HudArt& art, int k) {
  const auto i = static_cast<size_t>(k);
  const double since = s.t - s.picked_at[i];
  if (since >= 0 && since < 1) {
    const int frame = static_cast<int>(since * 10) % kFlashFrames;
    return &art.gun_flash[i][static_cast<size_t>(frame)];
  }
  return static_cast<int>(s.weapon) == k + 1 ? &art.gun_lit[i] : &art.gun[i];
}

gh::Cell cell_of(const ui::Image* image, bool show = true) {
  gh::Cell c;
  c.image = image;
  c.show = show;
  return c;
}

std::string seconds(double s) { return std::to_string(static_cast<int>(s + 0.999)); }

}  // namespace

Ammo ammo_of(Weapon w) {
  switch (w) {
    case Weapon::shotgun:
    case Weapon::double_shotgun:
      return Ammo::shells;
    case Weapon::nailgun:
    case Weapon::heavy_nailgun:
      return Ammo::nails;
    case Weapon::grenade_launcher:
    case Weapon::rocket_launcher:
      return Ammo::rockets;
    case Weapon::beam_gun:
      return Ammo::cells;
    case Weapon::melee:
      break;
  }
  return Ammo::none;
}

std::string_view look_name(Look l) { return kLookNames[static_cast<size_t>(l)]; }

Look look_from(std::string_view word) {
  for (size_t i = 0; i < kLookNames.size(); ++i)
    if (kLookNames[i] == word) return static_cast<Look>(i);
  return Look::classic;
}

gh::Style hud_style() {
  gh::Style st;
  const gh::Face face = gh::Face::bitmap(ge::charset::FontId::square);
  st.value_face = st.caption_face = st.big_face = face;
  st.caption = ui::hex(0xe9d9b8);
  st.shadow = ui::hex(0x0a0604, 0.9f);
  st.plate = ui::hex(0x140c08, 0.6f);
  st.track = ui::hex(0xffffff, 0.22f);
  st.good = ui::hex(0x8ee06a);
  st.bad = ui::hex(0xff4a38);
  st.accent = ui::hex(0xffd36a);
  return st;
}

Hud make_hud() {
  Hud hud;
  for (const Row& r : rows()) hud.layout.add(r.spec);
  return hud;
}

void add_cvars(ge::console::Console& con, Hud& hud) {
  con.cvars().bind({.name = "hud_style",
                    .fallback = std::string(look_name(Look::classic)),
                    .help = "the HUD's look: classic (the status bar), modern (the corners) or "
                            "compact (round the crosshair)",
                    .kind = ge::console::CvarKind::text},
                   &hud.look);
  con.cvars().bind({.name = "viewsize",
                    .fallback = "100",
                    .help = "the status bars: 100 every one, 110 the classic look's status bar "
                            "without its inventory bar, 120 none (the crosshair and the messages "
                            "stay)",
                    .kind = ge::console::CvarKind::integer,
                    .min = 100,
                    .max = 120},
                   &hud.viewsize);
  gh::add_hud_cvars(con, hud.layout);
}

void fill_hud(Hud& hud, const Status& s, const HudArt& art) {
  const gh::Style st = hud_style();
  const Look look = look_from(hud.look);
  const uint8_t mine = bit(look);
  gh::Layout& L = hud.layout;
  for (size_t i = 0; i < L.size(); ++i) {
    gh::Content& c = L[i].content;
    c.on = (rows()[i].looks & mine) != 0;
    c.density = art.density;
    c.color = {1, 1, 1, 1};
    c.alt = false;
  }
  auto at = [&](Id id) -> gh::Content& { return L.content(id); };
  const bool shield = s.powered(Power::shield);
  const Ammo held = ammo_of(s.weapon);
  const int rounds = s.ammo_for(s.weapon);
  const bool low_health = s.health <= 25, low_armour = s.armour <= 25, low_ammo = rounds <= 10;
  const ui::Image* armour_pic = shield ? &art.armour_shield
                                : s.armour_class == ArmourClass::none
                                    ? nullptr
                                    : &art.armour[static_cast<size_t>(s.armour_class) - 1];
  const ui::Image* ammo_pic = held == Ammo::none ? nullptr : &art.ammo[static_cast<size_t>(held)];
  const std::string armour_figure = shield ? "666" : std::to_string(s.armour);
  const ui::Color count_ink = art.glyphs.empty() ? art.count_color : ui::Color{1, 1, 1, 1};

  at(crosshair).image = &art.crosshair;
  at(crosshair).on &= !s.dead;

  // CLASSIC.
  at(sbar).image = &art.bar;
  at(sbar_armour_icon).image = armour_pic;
  at(sbar_armour).sheet = &art.sheet;
  at(sbar_armour).value = armour_figure;
  at(sbar_armour).alt = !shield && low_armour;
  at(sbar_face).image = face_of(s, art);
  at(sbar_health).sheet = &art.sheet;
  at(sbar_health).value = std::to_string(s.health);
  at(sbar_health).alt = low_health;
  at(sbar_ammo_icon).image = ammo_pic;
  at(sbar_ammo).sheet = &art.sheet;
  at(sbar_ammo).value = std::to_string(rounds);
  at(sbar_ammo).alt = low_ammo;
  at(ibar).image = &art.inventory_bar;
  {
    gh::Content& c = at(ibar_ammo);
    c.face = &art.count_face;
    c.cells.assign(kAmmoKinds, {});
    for (size_t k = 0; k < kAmmoKinds; ++k) {
      c.cells[k].text = std::to_string(std::min(s.ammo[k], 999));
      c.cells[k].text_color = count_ink;
    }
  }
  {
    gh::Content& c = at(ibar_guns);
    c.cells.assign(kGuns, {});
    for (int k = 0; k < kGuns; ++k) {
      gh::Cell& cell = c.cells[static_cast<size_t>(k)];
      cell.image = gun_picture(s, art, k);
      cell.show = s.has(static_cast<Weapon>(k + 1));
    }
  }
  {
    gh::Content& c = at(ibar_items);
    c.cells.assign(2 + kPowers, {});
    c.cells[0] = cell_of(&art.key[0], s.key_silver);
    c.cells[1] = cell_of(&art.key[1], s.key_gold);
    for (int k = 0; k < kPowers; ++k)
      c.cells[2 + static_cast<size_t>(k)] =
          cell_of(&art.power[static_cast<size_t>(k)], s.powered(static_cast<Power>(k)));
  }
  {
    gh::Content& c = at(ibar_runes);
    c.cells.assign(4, {});
    for (int k = 0; k < 4; ++k)
      c.cells[static_cast<size_t>(k)] =
          cell_of(&art.rune[static_cast<size_t>(k)], (s.runes >> k & 1) != 0);
  }
  {
    gh::Content& c = at(ibar_frags);
    c.face = &art.count_face;
    c.cells.clear();
    for (size_t k = 0; k < std::min<size_t>(s.board.size(), 4); ++k) {
      const FragRow& r = s.board[k];
      gh::Cell cell;
      cell.plate = r.top, cell.plate_low = r.bottom;
      cell.framed = r.me;
      cell.text = std::to_string(r.frags);
      cell.text_color = {1, 1, 1, 1};
      c.cells.push_back(cell);
    }
    c.on &= !c.cells.empty();
  }

  // MODERN.
  at(health_icon).image = art.health.empty() ? face_of(s, art) : &art.health;
  at(health).value = std::to_string(s.health);
  at(health).color = low_or(low_health, st.ink, st);
  at(health_bar).level = std::clamp(static_cast<float>(s.health) / 100.0f, 0.0f, 1.0f);
  at(health_bar).color = low_or(low_health, st.good, st);
  at(armour_icon).image = armour_pic != nullptr ? armour_pic : &art.armour[0];
  at(armour_icon).color = armour_pic != nullptr ? ui::Color{1, 1, 1, 1} : ui::Color{1, 1, 1, 0.35f};
  at(armour).value = armour_figure;
  at(armour).color = low_or(!shield && low_armour, st.ink, st);
  at(armour_bar).level = std::clamp(static_cast<float>(s.armour) / 200.0f, 0.0f, 1.0f);
  at(armour_bar).color = armour_colour(s.armour_class);
  auto key_cells = [&](gh::Content& c) {
    c.cells.clear();
    if (s.key_silver) c.cells.push_back(cell_of(&art.key[0]));
    if (s.key_gold) c.cells.push_back(cell_of(&art.key[1]));
    c.on &= !c.cells.empty();
  };
  key_cells(at(keys));
  at(ammo_icon).image = ammo_pic;
  at(ammo_icon).on &= ammo_pic != nullptr;
  at(ammo).value = std::to_string(rounds);
  at(ammo).caption = held == Ammo::none ? "" : std::string(kAmmoWords[static_cast<size_t>(held)]);
  at(ammo).color = low_or(low_ammo && held != Ammo::none, st.ink, st);
  at(ammo).on &= held != Ammo::none;
  auto gun_cells = [&](gh::Content& c, bool counts) {
    c.face = &art.count_face;
    c.cells.assign(kGuns, {});
    for (int k = 0; k < kGuns; ++k) {
      const auto w = static_cast<Weapon>(k + 1);
      gh::Cell& cell = c.cells[static_cast<size_t>(k)];
      const bool owned = s.has(w);
      cell.image =
          s.weapon == w ? &art.gun_lit[static_cast<size_t>(k)] : &art.gun[static_cast<size_t>(k)];
      cell.alpha = owned ? 1.0f : 0.2f;
      cell.framed = s.weapon == w;
      if (counts && owned) {
        cell.text = std::to_string(s.ammo_for(w));
        cell.text_color = s.ammo_for(w) <= 10 ? st.bad : count_ink;
      }
    }
  };
  gun_cells(at(guns), true);
  auto power_cells = [&](gh::Content& c) {
    c.face = &art.count_face;
    c.cells.clear();
    for (int k = 0; k < kPowers; ++k) {
      const double left = s.power_s[static_cast<size_t>(k)];
      if (left <= 0) continue;
      gh::Cell cell = cell_of(&art.power[static_cast<size_t>(k)]);
      cell.text = seconds(left);
      cell.text_color = left < 5 ? st.bad : count_ink;
      // Its last five seconds blink.
      if (left < 5 && gh::blink(s.t, 2) == false) cell.alpha = 0.35f;
      c.cells.push_back(cell);
    }
    c.on &= !c.cells.empty();
  };
  power_cells(at(powers));

  // COMPACT.
  at(ring_health).level = std::clamp(static_cast<float>(s.health) / 100.0f, 0.0f, 2.0f);
  at(ring_health).color = low_or(low_health, st.good, st);
  at(ring_health).over = ui::hex(0x6ad0ff);
  at(ring_armour).level = std::clamp(static_cast<float>(s.armour) / 200.0f, 0.0f, 1.0f);
  at(ring_armour).color = armour_colour(s.armour_class);
  at(ring_health_value).value = std::to_string(s.health);
  at(ring_health_value).color = low_or(low_health, st.ink, st);
  at(ring_armour_value).value = armour_figure;
  at(ring_armour_value).color = armour_colour(s.armour_class);
  at(ring_ammo).value = held == Ammo::none ? "-" : std::to_string(rounds);
  at(ring_ammo).caption =
      held == Ammo::none ? "" : std::string(kAmmoWords[static_cast<size_t>(held)]);
  at(ring_ammo).color = low_or(low_ammo && held != Ammo::none, st.ink, st);
  key_cells(at(ring_keys));
  power_cells(at(ring_powers));
  gun_cells(at(ring_guns), false);
  for (const Id id :
       {ring_health, ring_armour, ring_health_value, ring_armour_value, ring_ammo, ring_keys})
    at(id).on &= !s.dead;

  at(frags).value = std::to_string(s.frags);
  at(frags).caption = std::string(kFragsWord);
  hud.notify.expire(s.t);
  at(notify).feed = &hud.notify;
  at(notify).t = s.t;
  at(notify).on &= !hud.notify.lines().empty();
  // viewsize: 110 drops the inventory bar (its pieces hang from it), 120 every status element.
  if (hud.viewsize >= 110)
    for (const Id id : {ibar, ibar_ammo, ibar_guns, ibar_items, ibar_runes, ibar_frags})
      at(id).on = false;
  if (hud.viewsize >= 120)
    for (size_t i = 0; i < L.size(); ++i)
      if (i != crosshair && i != notify && i != centre) L[i].content.on = false;

  at(centre).value = hud.centre;
  at(centre).on &= !hud.centre.empty() && s.t < hud.centre_until;
}

}  // namespace shooters_edge::hud
