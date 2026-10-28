// LAB `classic-weapons`: the classic pack's held weapons in first person, inside a map of the
// pack, lit by the map. Each weapon read by the classic model reader, put into the view-model
// layer (classicenhance/weapons.hpp), placed by the classic row and drawn over the map as the
// player sees it from where they stand; the light is the lightmap under the player, the classic
// floor, a shot's flash and (for the enhanced mode) the map's torches as live lights.
//
// With `--source` (the release zip, an installed folder or the pack) it runs on the player's own
// pack: these pictures are derived from it and stay on the machine (a private page at most).
// Without it, the lab's fixtures stand in: brush's fixture map, the lab's palette and a held
// weapon made of boxes written as a classic model file and read back, so the pictures and the
// digest are the same everywhere and nothing of the pack is in the tree.
//
// ge:summary Lab: the classic held weapons drawn over a map in first person, lit by the map
// ge:keywords lab classicenhance viewmodel held-weapon first-person mdl lightmap probe floor
// ge:keywords muzzle-flash torch lineup fire spots
// ge:skill classicenhance labs
// ge:test tests/classicenhance_weapons_test.cpp
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

#include "brush/labs/textured_view.hpp"
#include "brush/read.hpp"
#include "brush/write.hpp"
#include "camera/pose.hpp"
#include "classicenhance/source.hpp"
#include "classicenhance/weapons.hpp"
#include "core/libm/libm.hpp"
#include "core/math.hpp"
#include "fixtures.hpp"
#include "lab/lab.hpp"
#include "lab/raster.hpp"
#include "morph/read.hpp"
#include "morph/write.hpp"
#include "vfs/pak.hpp"
#include "viewmodel/raster.hpp"

namespace {

namespace ce = ge::classicenhance;
namespace br = ge::brush;
namespace mo = ge::morph;
namespace vm = ge::viewmodel;
using ge::lab::Picture;

constexpr double kEyeAboveFloor = 46;  // map units: the hull's 24 under the origin, the eye's 22
constexpr double kFovH = 90;           // the classic lens, degrees across a 4:3 picture

struct Weapon {
  std::string name;
  vm::HeldModel held;
};

struct Spot {
  float eye[3];  // map units
  double yaw_deg = 0;
  vm::Probe probe;
  double level = 0;  // the probe's grey, light units
  std::string from;  // the entity it was taken from
};

// A box from lo to hi (file units, the eye frame: +x forward, +y left, +z up) on texel `tx`
// of an 8 x 8 skin, its faces wound counter-clockwise seen from outside.
void box(mo::Model& m, mo::Frame& f, const float lo[3], const float hi[3], int tx, int ty) {
  const float c[3] = {(lo[0] + hi[0]) / 2, (lo[1] + hi[1]) / 2, (lo[2] + hi[2]) / 2};
  for (int axis = 0; axis < 3; axis++) {
    for (int side = 0; side < 2; side++) {
      const int u = (axis + 1) % 3, v = (axis + 2) % 3;
      float q[4][3];
      for (int k = 0; k < 4; k++) {
        q[k][axis] = side ? hi[axis] : lo[axis];
        q[k][u] = (k == 1 || k == 2) ? hi[u] : lo[u];
        q[k][v] = (k >= 2) ? hi[v] : lo[v];
      }
      const auto base = uint32_t(m.st.size() / 2);
      for (auto& p : q) {
        f.positions.insert(f.positions.end(), {p[0], p[1], p[2]});
        m.st.insert(m.st.end(), {(float(tx) + 0.5f) / 8, (float(ty) + 0.5f) / 8});
        m.source_vertex.push_back(int(m.source_vertex.size()));
      }
      float e1[3], e2[3], n[3];
      for (int k = 0; k < 3; k++) e1[k] = q[1][k] - q[0][k], e2[k] = q[2][k] - q[0][k];
      n[0] = e1[1] * e2[2] - e1[2] * e2[1];
      n[1] = e1[2] * e2[0] - e1[0] * e2[2];
      n[2] = e1[0] * e2[1] - e1[1] * e2[0];
      const float out = n[0] * (q[0][0] - c[0]) + n[1] * (q[0][1] - c[1]) + n[2] * (q[0][2] - c[2]);
      if (out > 0) {
        m.indices.insert(m.indices.end(), {base, base + 1, base + 2, base, base + 2, base + 3});
      } else {
        m.indices.insert(m.indices.end(), {base, base + 2, base + 1, base, base + 3, base + 2});
      }
    }
  }
}

// The lab's held weapon: a receiver, a barrel and a glowing sight, authored at the eye as the
// classic files author theirs; a rest frame and a recoiled one. Written as a classic model file
// and read back, so the lab goes through the reader the pack's weapons go through.
mo::Model fixture_weapon(const ce::Palette& palette) {
  mo::Model m;
  mo::Skin skin{8, 8, std::vector<uint8_t>(64), 0, 0};
  const uint8_t metal = ce::labs::nearest_index(palette, 92, 96, 108);
  const uint8_t wood = ce::labs::nearest_index(palette, 120, 76, 40);
  for (size_t i = 0; i < 64; i++) skin.indices[i] = i < 32 ? metal : wood;
  skin.indices[63] = 254;  // fullbright: the sight's lamp
  m.skins.push_back(skin);
  const float shift[2] = {0, -3};
  for (int k = 0; k < 2; k++) {
    mo::Frame f;
    f.name = k == 0 ? "rest" : "fire";
    const float s = shift[k];
    const float barrel_lo[3] = {12 + s, -9, -9}, barrel_hi[3] = {34 + s, -6, -6};
    const float body_lo[3] = {2 + s, -11, -14}, body_hi[3] = {16 + s, -4, -7};
    const float grip_lo[3] = {4 + s, -10, -22}, grip_hi[3] = {9 + s, -5, -13};
    const float sight_lo[3] = {30 + s, -8, -6}, sight_hi[3] = {32 + s, -7, -4.5f};
    m.st.clear();
    m.indices.clear();
    m.source_vertex.clear();
    box(m, f, barrel_lo, barrel_hi, 2, 1);
    box(m, f, body_lo, body_hi, 3, 2);
    box(m, f, grip_lo, grip_hi, 3, 6);
    box(m, f, sight_lo, sight_hi, 7, 7);
    for (int a = 0; a < 3; a++) f.min[size_t(a)] = 1e9f, f.max[size_t(a)] = -1e9f;
    for (size_t i = 0; i + 2 < f.positions.size(); i += 3) {
      for (size_t a = 0; a < 3; a++) {
        f.min[a] = std::min(f.min[a], f.positions[i + a]);
        f.max[a] = std::max(f.max[a], f.positions[i + a]);
      }
    }
    m.frames.push_back(std::move(f));
  }
  m.radius = 40;
  mo::Model back;
  std::string error;
  if (!mo::read_model(mo::write_model(m), "fixture.mdl", &back, &error)) return m;
  return back;
}

double grey(const vm::Probe& p) {
  return (p.ambient[0] + p.ambient[1] + p.ambient[2] + p.direct[0] + p.direct[1] + p.direct[2]) /
         3.0;
}

bool parse_origin(const std::string& s, float out[3]) {
  return std::sscanf(s.c_str(), "%f %f %f", &out[0], &out[1], &out[2]) == 3;
}

bool open_at(const br::Map& map, const float p[3]) {
  const uint32_t leaf = br::leaf_at(map, p);
  return leaf < map.leaves.size() && !map.leaves[leaf].solid;
}

// The way with the most room in front of `eye`, of eight, degrees.
double open_yaw(const br::Map& map, const float eye[3]) {
  double best = 0;
  int best_run = -1;
  for (int k = 0; k < 8; k++) {
    const double yaw = k * 45.0;
    const double c = ge::core::libm::cos(yaw * ge::core::kPi / 180);
    const double s = ge::core::libm::sin(yaw * ge::core::kPi / 180);
    int step = 0;
    for (; step < 64; step++) {
      const double run = step * 16.0;
      const float p[3] = {float(eye[0] + c * run), float(eye[1] + s * run), eye[2]};
      if (!open_at(map, p)) break;
    }
    if (step > best_run) best_run = step, best = yaw;
  }
  return best;
}

// Every place a player could stand that the map names: its starts, its items and weapons, and
// the floor under each of its lights; dropped to the floor and raised to the eye.
std::vector<Spot> spots_of(const br::Map& map) {
  std::vector<Spot> out;
  for (const br::Entity& e : map.entities) {
    const std::string& cls = e.get("classname");
    const bool start = cls.rfind("info_player", 0) == 0;
    if (!start && cls.rfind("item_", 0) != 0 && cls.rfind("weapon_", 0) != 0 &&
        cls.rfind("light", 0) != 0) {
      continue;
    }
    float o[3];
    if (!parse_origin(e.get("origin"), o)) continue;
    o[2] += 8;  // off the floor an item stands on
    bool found = false;
    float floor = 0;
    Spot s;
    s.probe = ce::probe_under(map, o, &found, &floor);
    if (!found) continue;
    s.eye[0] = o[0];
    s.eye[1] = o[1];
    s.eye[2] = float(floor + kEyeAboveFloor);
    if (!open_at(map, s.eye)) continue;
    s.level = grey(s.probe);
    s.from = cls;
    if (start && !e.get("angle").empty()) {
      s.yaw_deg = std::atof(e.get("angle").c_str());
    } else {
      s.yaw_deg = open_yaw(map, s.eye);
    }
    out.push_back(s);
  }
  return out;
}

struct Scene {
  const br::Map* map = nullptr;
  const ce::Palette* palette = nullptr;
  int width = 480, height = 360;
};

// One first-person frame: the map from `spot`, the weapon over it in its own depth range.
Picture frame(const Scene& sc, const Spot& spot, const vm::HeldModel& gun, int fa, int fb,
              double ft, const vm::Placement& place, const std::vector<vm::PointLight>& lights,
              double* gun_level = nullptr) {
  const double aspect = double(sc.width) / sc.height;
  const double fov_y = ge::camera::fov_y_of(kFovH * ge::core::kPi / 180);
  Picture room = ge::lab::create_picture(sc.width, sc.height, {0, 0, 0});
  br::labs::TexturedEye eye;
  for (size_t k = 0; k < 3; k++) eye.at[k] = spot.eye[k];
  eye.yaw_deg = spot.yaw_deg;
  eye.fov_deg = ge::camera::fov_h_of(fov_y, aspect) * 180 / ge::core::kPi;
  br::labs::draw_textured(room, 0, 0, sc.width, sc.height, *sc.map, sc.palette->rgb, eye);

  ge::camera::Pose view;
  double at[3];
  br::to_engine(spot.eye, at);
  view.eye = {at[0], at[1], at[2]};
  view.yaw = (90 - spot.yaw_deg) * ge::core::kPi / 180;
  view.fov_y = fov_y;
  const float black[3] = {0, 0, 0};
  vm::ViewTarget t = vm::make_target(sc.width, sc.height, 2, black);
  std::vector<float> gp, gn;
  vm::place_held(gun, place, fa, fb, ft, gp, gn);
  const float grip[3] = {float(place.m[3]), float(place.m[7]), float(place.m[11] + 0.4)};
  vm::Lighting lit = vm::light_held(spot.probe, lights, view, grip, ce::kClassicFloor);
  lit.gloss = 0.25f;
  if (gun_level) *gun_level = (lit.ambient[0] + lit.ambient[1] + lit.ambient[2]) / 3.0;
  vm::DrawLit d;
  d.positions = gp;
  d.normals = gn;
  d.indices = gun.indices;
  d.colour = gun.colour;
  d.uv = gun.uv;
  d.skin = gun.skin.image.rgba.empty() ? nullptr : &gun.skin;
  d.fov_y = place.fov_y > 0 ? place.fov_y : fov_y;
  d.near_m = 0.01;
  vm::draw_lit(t, d, lit);
  room.rgba = vm::resolve_over(t, room.rgba);
  return room;
}

Picture sheet(const std::vector<Picture>& panels, const std::vector<std::string>& labels,
              int cols) {
  const int w = panels[0].width, h = panels[0].height, gap = 4;
  const int rows = (int(panels.size()) + cols - 1) / cols;
  Picture out =
      ge::lab::create_picture(cols * (w + gap) + gap, rows * (h + gap) + gap, {16, 17, 20});
  for (size_t i = 0; i < panels.size(); ++i) {
    const int x = gap + int(i % size_t(cols)) * (w + gap);
    const int y = gap + int(i / size_t(cols)) * (h + gap);
    ge::lab::blit(out, panels[i], x, y);
    ge::lab::fill_rect(out, x, y, ge::lab::text_width(labels[i], 2) + 8, 22, {0, 0, 0, 150});
    ge::lab::draw_text(out, x + 4, y + 4, labels[i], {240, 240, 240}, 2);
  }
  return out;
}

std::string f2(double v) {
  char b[32];
  std::snprintf(b, sizeof b, "%.2f", v);
  return b;
}

std::string short_name(const std::string& path) {
  std::string s = path.substr(path.rfind('/') + 1);
  if (s.size() > 4 && s.substr(s.size() - 4) == ".mdl") s.resize(s.size() - 4);
  return s;
}

void run(ge::lab::Lab& lab) {
  const int width = int(lab.arg_num("width"));
  if (width < 96 || width > 1920) {
    lab.fail("--width must be 96..1920", "next: rerun with --width=480.");
    return;
  }
  Scene sc;
  sc.width = width;
  sc.height = width * 3 / 4;
  const std::string path = lab.arg("source");
  ce::Palette palette;
  br::Map map;
  std::vector<Weapon> weapons;
  std::string error;
  std::string where = "the fixture map";
  if (path.empty()) {
    palette = ce::labs::fixture_palette();
    std::vector<uint8_t> bytes;
    if (!br::write_bsp(br::sample_fixture(), br::Family::v29, &bytes, &error) ||
        !br::read_bsp(bytes, &map, &error)) {
      lab.fail(error, "next: run tests/brush_read_test; skill: brush.");
      return;
    }
    weapons.push_back({"fixture", ce::held_from_morph(fixture_weapon(palette), palette)});
  } else {
    ce::Source src;
    if (!ce::open_source(path, &src, &error)) {
      lab.fail(error,
               "next: give --source the shareware release zip, an installed folder or its pack.");
      return;
    }
    ge::vfs::Pak pak;
    if (!ge::vfs::read_pak(src.pack, &pak, &error)) {
      lab.fail(error, "next: check the pack with `ge tool classicenhance -- verify`.");
      return;
    }
    const ge::vfs::PakEntry* pal = ge::vfs::pak_find(pak, "gfx/palette.lmp");
    if (!pal || pal->size != 768) {
      lab.fail("the pack has no gfx/palette.lmp", "next: give --source the classic pack.");
      return;
    }
    const auto pb = ge::vfs::pak_file(src.pack, *pal);
    std::copy(pb.begin(), pb.end(), palette.rgb.begin());
    where = "maps/" + lab.arg("map") + ".bsp";
    const ge::vfs::PakEntry* me = ge::vfs::pak_find(pak, where);
    if (!me || !br::read_bsp(ge::vfs::pak_file(src.pack, *me), &map, &error)) {
      lab.fail("no readable " + where + (error.empty() ? "" : ": " + error),
               "next: pick a map the pack carries with --map (e1m1 .. e1m8, start).");
      return;
    }
    std::vector<std::string> skipped;
    for (ce::PackWeapon& w : ce::pack_weapons(src.pack, &skipped)) {
      weapons.push_back({w.name, ce::held_from_morph(w.model, palette)});
    }
    for (const std::string& s : skipped) lab.note("skipped: " + s);
    lab.note(
        "local run on the classic pack: these pictures are derived from it; keep them on this "
        "machine");
  }
  if (weapons.empty()) {
    lab.fail("the pack has no held weapons (progs/v_*.mdl)", "next: give --source the pack.");
    return;
  }
  sc.map = &map;
  sc.palette = &palette;

  std::vector<Spot> spots = spots_of(map);
  if (spots.empty()) {
    lab.fail("no place to stand in " + where, "next: pick another map with --map.");
    return;
  }
  const Spot start = spots[0];
  std::sort(spots.begin(), spots.end(),
            [](const Spot& a, const Spot& b) { return a.level < b.level; });

  const vm::PlaceRow row = ce::classic_weapon_row();
  vm::PlaceState rest_state;
  const vm::Placement rest = vm::frame_viewmodel(row, vm::PlaceInput{}, rest_state, 1.0 / 60);

  // The weapon the light and the fire are shown with.
  size_t chosen = 0;
  const std::string want = lab.arg("weapon");
  for (size_t i = 0; i < weapons.size(); i++) {
    if (!want.empty() && short_name(weapons[i].name) == want) chosen = i;
  }
  if (want.empty()) {
    for (size_t i = 0; i < weapons.size(); i++) {
      if (short_name(weapons[i].name) == "v_shot") chosen = i;
    }
  }
  const Weapon& gun = weapons[chosen];

  ge::lab::LabTable table{"every held weapon: its size and its frames",
                          {"weapon", "frames", "vertices", "triangles", "length m", "glows"},
                          {}};
  {
    std::vector<Picture> panels;
    std::vector<std::string> labels;
    for (const Weapon& w : weapons) {
      float lo[3], hi[3];
      vm::held_bounds(w.held, lo, hi);
      table.rows.push_back({short_name(w.name), std::to_string(w.held.frames.size()),
                            std::to_string(w.held.vertex_count()),
                            std::to_string(w.held.indices.size() / 3), f2(hi[2] - lo[2]),
                            w.held.skin.glow.empty() ? "no" : "yes"});
      lab.metric("length-" + short_name(w.name), hi[2] - lo[2], "m");
      panels.push_back(frame(sc, start, w.held, 0, 0, 0, rest, {}));
      labels.push_back(short_name(w.name));
    }
    lab.shot({"lineup",
              "EVERY HELD WEAPON AT THE START",
              "Each held weapon in its first frame, placed by the classic row, from where the "
              "player starts in " +
                  where + ", lit by the floor under the player.",
              "the weapons sit low and right where the classic game holds them; none points "
              "back, none clips the near plane; skins read the right way round",
              {{"map", where}, {"width", std::to_string(width)}}},
             sheet(panels, labels, std::min<int>(3, int(panels.size()))));
  }
  lab.table(table);
  {
    // The skins as the files carry them, their glowing texels ringed in white beside.
    std::vector<Picture> panels;
    std::vector<std::string> labels;
    for (const Weapon& w : weapons) {
      const ge::assets::Image& img = w.held.skin.image;
      if (img.rgba.empty()) continue;
      Picture p;
      p.width = img.width;
      p.height = img.height;
      p.rgba = img.rgba;
      for (size_t i = 0; i < w.held.skin.glow.size(); i++) {
        if (w.held.skin.glow[i] == 0) continue;
        const int x = int(i % size_t(img.width)), y = int(i / size_t(img.width));
        for (int k = 0; k < 2; k++) {
          const int nx = x + (k == 0 ? -1 : 1);
          if (nx >= 0 && nx < img.width &&
              w.held.skin.glow[size_t(y) * size_t(img.width) + size_t(nx)] == 0) {
            ge::lab::set_pixel(p, nx, y, {255, 255, 255});
          }
        }
      }
      panels.push_back(ge::lab::resized(p, sc.width, sc.height));
      labels.push_back(short_name(w.name));
    }
    if (!panels.empty()) {
      lab.shot({"skins",
                "EVERY SKIN, ITS GLOW MARKED",
                "Each held weapon's skin through the palette, stretched to a panel; a texel from "
                "the fullbright range is edged in white.",
                "the glow marks only lamps, sights and hot parts, never the body",
                {{"map", where}}},
               sheet(panels, labels, std::min<int>(3, int(panels.size()))));
    }
  }

  {
    // The darkest place to stand, the brightest and four between.
    std::vector<Picture> panels;
    std::vector<std::string> labels;
    const size_t n = std::min<size_t>(6, spots.size());
    for (size_t k = 0; k < n; k++) {
      const size_t i = n == 1 ? 0 : k * (spots.size() - 1) / (n - 1);
      double level = 0;
      panels.push_back(frame(sc, spots[i], gun.held, 0, 0, 0, rest, {}, &level));
      labels.push_back("floor " + f2(spots[i].level));
      if (k == 0) lab.metric("gun-light-darkest", level, "share");
      if (k + 1 == n) lab.metric("gun-light-brightest", level, "share");
    }
    lab.shot({"light",
              "ONE WEAPON, THE MAP'S LIGHT",
              short_name(gun.name) + " at the darkest place to stand in " + where +
                  ", the brightest and four between, each lit by the lightmap under the "
                  "player (the label: that light, 1 = the surface as painted).",
              "the weapon follows the room: dark where the floor is dark (never black: the "
              "classic floor), bright and warm where it is lit",
              {{"map", where}, {"weapon", short_name(gun.name)}}},
             sheet(panels, labels, 3));
  }

  {
    // A shot in the dark: the frames at ten a second, the flash on the first two.
    const Spot& dark = spots[0];
    std::vector<Picture> panels;
    std::vector<std::string> labels;
    vm::PlaceState st;
    vm::frame_viewmodel(row, vm::PlaceInput{}, st, 1.0 / 60);
    const int frames = std::min<int>(6, int(gun.held.frames.size()));
    ge::camera::Pose view;
    double at[3];
    br::to_engine(dark.eye, at);
    view.eye = {at[0], at[1], at[2]};
    view.yaw = (90 - dark.yaw_deg) * ge::core::kPi / 180;
    const float muzzle_view[3] = {0, float(-6 * br::kUnitM), float(18 * br::kUnitM)};
    double muzzle[3];
    vm::view_to_world(view, muzzle_view, muzzle);
    for (int f = 0; f < frames; f++) {
      vm::PlaceInput in;
      in.shot = f == 1 ? 1.0 : 0.0;
      const vm::Placement p = vm::frame_viewmodel(row, in, st, ce::kFrameSeconds);
      std::vector<vm::PointLight> lights;
      if (f >= 1 && f <= 2) lights.push_back(ce::muzzle_flash_light(muzzle));
      panels.push_back(frame(sc, dark, gun.held, f, f, 0, p, lights));
      labels.push_back(gun.held.frames[size_t(f)].name + (lights.empty() ? "" : " +flash"));
    }
    lab.shot({"fire",
              "A SHOT IN THE DARK",
              short_name(gun.name) + "'s frames at ten a second from the darkest place in " +
                  where + "; the shot's flash lights the weapon for the two frames it lasts.",
              "the flash lights the weapon warm from the muzzle, then it falls back to the dark",
              {{"map", where}, {"weapon", short_name(gun.name)}}},
             sheet(panels, labels, 3));
  }

  {
    // The enhanced mode's live torches beside the baked light alone, where a torch is nearest.
    const std::vector<vm::PointLight> flames = ce::flame_lights(map);
    lab.metric("flames", double(flames.size()), "count");
    if (!flames.empty()) {
      size_t best = 0;
      const vm::PointLight* torch = &flames[0];
      double best_d = 1e18;
      for (size_t i = 0; i < spots.size(); i++) {
        double e[3];
        br::to_engine(spots[i].eye, e);
        for (const vm::PointLight& l : flames) {
          const double dx = e[0] - l.position[0], dy = e[1] - l.position[1],
                       dz = e[2] - l.position[2];
          const double d = dx * dx + dy * dy + dz * dz;
          if (d < best_d) best_d = d, best = i, torch = &l;
        }
      }
      // Turned so the torch is off to the left, its wall running ahead (the engine's z is the map's
      // y).
      Spot by = spots[best];
      double e[3];
      br::to_engine(by.eye, e);
      by.yaw_deg = ge::core::libm::atan2(torch->position[2] - e[2], torch->position[0] - e[0]) *
                       180 / ge::core::kPi -
                   70;
      std::vector<Picture> panels = {frame(sc, by, gun.held, 0, 0, 0, rest, {}),
                                     frame(sc, by, gun.held, 0, 0, 0, rest, flames)};
      lab.shot({"torches",
                "BY A TORCH: BAKED, THEN LIVE",
                "The place nearest a torch in " + where +
                    ": the baked light alone, then the map's torches and flames as live lights "
                    "as the enhanced mode has them.",
                "the live torch warms the weapon's side toward it",
                {{"map", where}, {"weapon", short_name(gun.name)}}},
               sheet(panels, {"baked", "live torches"}, 2));
    }
  }
}

const ge::lab::LabSpec kSpec{
    "classic-weapons",
    ge::lab::LabKind::picture,
    "plugin:classicenhance",
    "plugins/classicenhance/weapons",
    "classicenhance",
    "Do the classic held weapons sit where the classic game holds them and take the light of the "
    "map round the player?",
    {
        {"source", "", "the classic pack (zip, folder or pack); empty: the lab's fixtures"},
        {"map", "e1m1", "with --source, the map to stand in"},
        {"weapon", "", "the weapon the light and the fire are shown with (default: v_shot)"},
        {"width", "480", "a panel's width, px (its height is 3/4 of it)"},
    },
    "--width=128",
    run,
};
GE_LAB(kSpec);

}  // namespace
