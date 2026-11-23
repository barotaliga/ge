// The player page's figure (player_view.hpp).
//
// ge:summary The player page's figure: the model live in skin and colours, turned, armed, firing
// ge:keywords player-page figure turntable swatches vwep attack-frames
// ge:skill ui shooter
#include "shooters_edge/menu/player_view.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>

#include "assets/pcx.hpp"
#include "classic/vweps.hpp"
#include "morph/read.hpp"
#include "overview/mesh_picture.hpp"
#include "shooters_edge/menu/figures.hpp"

namespace shooters_edge::menu {

namespace ui = ge::ui;
namespace gc = ge::classic;

namespace {

// Short enough for a classic row's value (twelve letters).
constexpr std::array<std::string_view, 8> kQuakeWeapons = {"AXE",        "SHOTGUN",    "S.SHOTGUN",
                                                           "NAILGUN",    "S.NAILGUN",  "G.LAUNCHER",
                                                           "R.LAUNCHER", "THUNDERBOLT"};
// Each weapon's stand and attack frames by their names (a name and its number), the attack
// played this many times over.
struct Moves {
  std::string_view stand, attack;
  int repeats;
};
constexpr std::array<Moves, 8> kQuakeMoves = {{{"axstnd", "axatt", 1},
                                               {"stand", "shotatt", 1},
                                               {"stand", "shotatt", 1},
                                               {"stand", "nailatt", 4},
                                               {"stand", "nailatt", 4},
                                               {"stand", "rockatt", 1},
                                               {"stand", "rockatt", 1},
                                               {"stand", "light", 4}}};
constexpr std::array<std::string_view, 11> kQ2Weapons = {
    "BLASTER",    "SHOTGUN",    "S.SHOTGUN",    "MACHINEGUN", "CHAINGUN", "GRENADES",
    "G.LAUNCHER", "R.LAUNCHER", "HYPERBLASTER", "RAILGUN",    "BFG10K"};
constexpr std::array<std::string_view, 11> kQ2Carried = {
    "w_blaster",   "w_shotgun",   "w_sshotgun",     "w_machinegun", "w_chaingun", "a_grenades",
    "w_glauncher", "w_rlauncher", "w_hyperblaster", "w_railgun",    "w_bfg"};

constexpr double kFps = 10;          // frames a second, standing and firing
constexpr double kTurnDegS = 30;     // its own turning: a full turn in 12 s
constexpr double kDragDegPx = 0.5;   // a drag's turn a pixel
constexpr float kClickSlopPx = 4;    // a press that moves less is a click
constexpr double kFigureUnits = 64;  // the height a figure is fitted to (map units)
constexpr double kOriginUp = 0.56;   // the origin's place down the picture
constexpr double kElevation = 8;     // the eye above the figure's middle, degrees

// The frames named `prefix` and a number (`stand1`, `stand01`), in the file's order.
std::vector<int> frames_named(const ge::morph::Model& m, std::string_view prefix) {
  std::vector<int> out;
  for (size_t i = 0; i < m.frames.size(); ++i) {
    const std::string& n = m.frames[i].name;
    if (n.size() <= prefix.size() || n.compare(0, prefix.size(), prefix) != 0) continue;
    if (std::all_of(n.begin() + std::ptrdiff_t(prefix.size()), n.end(),
                    [](char c) { return std::isdigit(static_cast<unsigned char>(c)) != 0; }))
      out.push_back(static_cast<int>(i));
  }
  return out;
}

std::string lower(std::string_view s) {
  std::string out(s);
  for (char& c : out) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  return out;
}

}  // namespace

std::span<const std::string_view> figure_weapons(bool q2) {
  if (q2) return kQ2Weapons;
  return kQuakeWeapons;
}

std::vector<std::string> quake_skins(const ge::vfs::SearchPath& files) {
  std::vector<std::string> out;
  for (const ge::vfs::Listed& f : ge::vfs::list(files)) {
    const std::string_view n = f.name;
    if (!n.starts_with("skins/") || !n.ends_with(".pcx")) continue;
    const std::string_view name = n.substr(6, n.size() - 10);
    if (name.empty() || name.find('/') != std::string_view::npos) continue;
    out.emplace_back(name);
  }
  std::sort(out.begin(), out.end());
  out.erase(std::unique(out.begin(), out.end()), out.end());
  if (const auto base = std::find(out.begin(), out.end(), "base"); base != out.end())
    std::rotate(out.begin(), base, base + 1);
  return out;
}

void PlayerView::set_files(const ge::vfs::SearchPath* files, std::span<const uint8_t> palette) {
  if (files == files_ &&
      std::equal(palette.begin(), palette.end(), palette_.begin(), palette_.end()))
    return;
  files_ = files;
  palette_.assign(palette.begin(), palette.end());
  loaded_ = false;
}

void PlayerView::set_look(const FigureLook& look) {
  if (look == look_) return;
  const bool models = look.q2 != look_.q2 || look.vweps != look_.vweps ||
                      look.weapon != look_.weapon || look.skin != look_.skin;
  look_ = look;
  dirty_ = true;
  if (models) {
    loaded_ = false;
    fire_s_ = -1;
  }
}

void PlayerView::load() {
  if (loaded_) return;
  loaded_ = true;
  have_ = false;
  body_ = {};
  weapon_ = {};
  weapon_wears_skin_ = false;
  idle_.clear();
  attack_.clear();
  dirty_ = true;
  if (files_ == nullptr || palette_.size() < 768) return;
  const ge::vfs::SearchPath& sp = *files_;
  const gc::ReadFile read = [&sp](std::string_view path, std::vector<uint8_t>& store) {
    store.clear();
    const ge::vfs::FileRef* f = ge::vfs::find(sp, path);
    if (f == nullptr) return std::span<const uint8_t>{};
    if (std::span<const uint8_t> v = ge::vfs::view(sp, *f); !v.empty()) return v;
    std::string error;
    if (!ge::vfs::read(sp, *f, &store, &error)) store.clear();
    return std::span<const uint8_t>(store);
  };
  const auto exists = [&sp](const std::string& path) { return ge::vfs::find(sp, path) != nullptr; };
  const auto open = [&read](std::string_view path, ge::morph::Model& out) {
    std::vector<uint8_t> store;
    const std::span<const uint8_t> bytes = read(path, store);
    std::string err;
    return !bytes.empty() && ge::morph::read_model(bytes, std::string(path), &out, &err) &&
           !out.frames.empty();
  };
  const size_t w = static_cast<size_t>(std::max(0, look_.weapon));
  if (look_.q2) {
    auto [model, skin] = split_look(look_.skin);
    model = lower(model);
    skin = lower(skin);
    if (!exists("players/" + model + "/tris.md2")) model = "male";
    std::string skin_path = "players/" + model + "/" + skin + ".pcx";
    for (const char* ext : {".tga", ".png", ".pcx"})
      if (exists("players/" + model + "/" + skin + ext)) {
        skin_path = "players/" + model + "/" + skin + ext;
        break;
      }
    if (!gc::read_model_art(read, palette_, "players/" + model + "/tris.md2", &body_, skin_path))
      return;
    std::vector<std::string> tries;
    if (look_.vweps && w < kQ2Carried.size()) {
      tries.push_back("players/" + model + "/" + std::string(kQ2Carried[w]) + ".md2");
      tries.push_back("players/" + model + "/weapon.md2");
      tries.push_back("players/male/" + std::string(kQ2Carried[w]) + ".md2");
    } else {
      tries.push_back("players/" + model + "/weapon.md2");
    }
    tries.push_back("players/male/weapon.md2");
    for (const std::string& t : tries)
      if (gc::read_model_art(read, palette_, t, &weapon_)) break;
    idle_ = frames_named(body_.model, "stand");
    attack_ = frames_named(body_.model, "attack");
  } else {
    if (!gc::read_model_art(read, palette_, "progs/player.mdl", &body_)) return;
    const Moves mv = kQuakeMoves[std::min(w, kQuakeMoves.size() - 1)];
    if (look_.vweps) {
      gc::FigureParts parts;
      const bool cut = gc::figure_parts(body_.model, parts);
      const auto stand_in = [&](const std::string& path, gc::ModelArt& into, bool* wears) {
        if (gc::read_model_art(read, palette_, path, &into)) return true;
        if (!cut) return false;
        ge::morph::Model made;
        std::string skin_of;
        if (!gc::vwep_stand_in(path, body_.model, parts, open, made, &skin_of)) return false;
        into = {};
        into.model = std::move(made);
        into.palette = palette_;
        into.ok = true;
        if (wears != nullptr) *wears = skin_of == "progs/player.mdl";
        return true;
      };
      gc::ModelArt plain;
      if (stand_in("progs/vwplayer.mdl", plain, nullptr)) body_ = std::move(plain);
      const std::string listed(gc::kFixedVweps[std::min(w + 1, gc::kFixedVweps.size() - 1)]);
      stand_in("progs/" + listed + ".mdl", weapon_, &weapon_wears_skin_);
    }
    // The skin worn: skins/<name>.pcx, else base's, else the model's own.
    const std::string name = lower(look_.skin.empty() ? "base" : look_.skin);
    for (const std::string& path : {"skins/" + name + ".pcx", std::string("skins/base.pcx")}) {
      std::vector<uint8_t> store;
      const std::span<const uint8_t> bytes = read(path, store);
      ge::assets::IndexedImage pic;
      std::string err;
      if (bytes.empty() || !ge::assets::decode_pcx(bytes, path, &pic, &err)) continue;
      if (!gc::wear_indexed_skin(body_, pic.indices, pic.width, pic.height)) continue;
      if (weapon_wears_skin_) gc::wear_indexed_skin(weapon_, pic.indices, pic.width, pic.height);
      break;
    }
    idle_ = frames_named(body_.model, mv.stand);
    const std::vector<int> once = frames_named(body_.model, mv.attack);
    for (int r = 0; r < mv.repeats; ++r) attack_.insert(attack_.end(), once.begin(), once.end());
  }
  if (idle_.empty()) idle_.push_back(0);
  have_ = true;
}

bool PlayerView::ready() {
  load();
  return have_;
}

void PlayerView::fire() {
  if (!attack_.empty()) fire_s_ = 0;
}

void PlayerView::hold(double yaw_deg, double time_s) {
  yaw_ = yaw_deg;
  t_ = time_s;
  spin_ = false;
}

void PlayerView::update(const Pointer& p, const ui::Rect& area, double dt) {
  t_ += dt;
  if (fire_s_ >= 0) {
    fire_s_ += dt;
    if (fire_s_ * kFps >= static_cast<double>(attack_.size())) fire_s_ = -1;
  }
  const bool over = area.contains(p.x, p.y);
  if (p.held && !held_ && over) {  // pressed on the figure: a drag or a click begins
    dragging_ = true;
    press_x_ = p.x;
    press_y_ = p.y;
    last_x_ = p.x;
  }
  held_ = p.held;
  if (dragging_) {
    yaw_ += static_cast<double>(p.x - last_x_) * kDragDegPx;
    last_x_ = p.x;
    if (!p.held) {
      dragging_ = false;
      if (std::fabs(p.x - press_x_) < kClickSlopPx && std::fabs(p.y - press_y_) < kClickSlopPx)
        fire();
    }
  } else if (p.click && over) {
    fire();
  }
  if (spin_ && !dragging_) yaw_ += kTurnDegS * dt;
  yaw_ = std::fmod(yaw_, 360.0);
}

int PlayerView::pick_frame() const {
  if (fire_s_ >= 0 && !attack_.empty()) {
    const auto i = static_cast<size_t>(fire_s_ * kFps);
    return attack_[std::min(i, attack_.size() - 1)];
  }
  const auto i = static_cast<size_t>(t_ * kFps) % idle_.size();
  return idle_[i];
}

const ui::Image* PlayerView::picture(int w, int h) {
  if (!ready() || w <= 0 || h <= 0) return nullptr;
  frame_ = pick_frame();
  if (!dirty_ && w == drawn_w_ && h == drawn_h_ && frame_ == drawn_frame_ &&
      std::fabs(yaw_ - drawn_yaw_) < 0.5)
    return &picture_;
  ge::overview::Mesh mesh = gc::art_mesh(body_, frame_, 0, look_.top, look_.bottom);
  if (weapon_.ok) {
    const int wf = std::min(frame_, static_cast<int>(weapon_.model.frames.size()) - 1);
    gc::join_meshes(mesh, gc::art_mesh(weapon_, wf, 0, look_.top, look_.bottom));
  }
  ge::overview::TurnView v;
  v.width = w;
  v.height = h;
  v.yaw_deg = yaw_ - 90;  // the figure faces +x; at 0 it faces the viewer
  v.elevation_deg = kElevation;
  v.px_per_unit = static_cast<double>(h) / kFigureUnits;
  v.origin_y = kOriginUp;
  const uint64_t revision = picture_.revision + 1;  // a GPU copy is uploaded again
  picture_ = ge::overview::draw_turned(mesh, v);
  picture_.revision = revision;
  drawn_w_ = w;
  drawn_h_ = h;
  drawn_frame_ = frame_;
  drawn_yaw_ = yaw_;
  dirty_ = false;
  return &picture_;
}

ui::Rect swatch_rect(const ui::Rect& area, int row, int colour) {
  const float cell = area.w / 14.0f;
  return {area.x + cell * static_cast<float>(colour), area.y + cell * static_cast<float>(row), cell,
          cell};
}

int swatch_hit(const ui::Rect& area, float x, float y, int* row) {
  for (int r = 0; r < 2; ++r)
    for (int c = 0; c < 14; ++c)
      if (swatch_rect(area, r, c).contains(x, y)) {
        if (row != nullptr) *row = r;
        return c;
      }
  return -1;
}

}  // namespace shooters_edge::menu
