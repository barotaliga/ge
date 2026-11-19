// LAB `se-vweps`: DO PLAYERS CARRY THE WEAPON THEY HOLD? The first classic game's visible weapons
// (plugins/classic/vweps.hpp): the server's list (the common server mod's, `//vwep vwplayer w_axe
// ... w_light -`), each player's index into it, and the window drawing the body without its own
// gun and the listed weapon on the body's frame (client/world_models.hpp's `filtered_models`).
//
// With `--game=<base>/<folder>` (a game folder of packs; `--map`, default dm6) it is that map and
// the pack's own figure, for PRIVATE pictures only: a row of eight players, one with each weapon,
// drawn as the window draws them with `r_drawvweps 0` (every figure its own plain gun, the classic
// picture) and 1; then two of them close up through the frames of a run, a shot, a pain and two
// deaths. A listed file the pack lacks is made from it (the stand-ins: the body cut from the
// figure, its own axe and gun alone, each pickup model posed in the hands).
//
// Without `--game` it is a made figure (a body, a gun out in `stand1`, an axe out in `axstnd1`) and
// a made pickup: the pieces found, the stand-ins' sizes and where the pickup lands are the digest,
// so the smoke run checks the cut and the carry on any machine.
//
// ge:summary Lab: players carrying the weapon they hold, the plain figure beside it, real maps
// ge:keywords lab se-vweps vwep visible-weapons r_drawvweps vwplayer stand-in cut-out carried
// ge:keywords player-model pickup-model weapons frames death
// ge:skill shooter render labs classic
// ge:test tests/classic_vweps_test.cpp
#include <cmath>
#include <cstdio>
#include <memory>
#include <string>
#include <vector>

#include "brush/map.hpp"
#include "brush/read.hpp"
#include "classic/vweps.hpp"
#include "core/libm/libm.hpp"
#include "gpu/device.hpp"
#include "gpu/target.hpp"
#include "lab/gif.hpp"
#include "lab/lab.hpp"
#include "lab/labs/explosion_scene.hpp"
#include "lab/labs/liquid_scene.hpp"
#include "morph/carry.hpp"
#include "shooters_edge/client/game_adapter.hpp"
#include "shooters_edge/client/listen.hpp"
#include "shooters_edge/client/world_draw.hpp"
#include "shooters_edge/client/world_fx.hpp"
#include "shooters_edge/client/world_models.hpp"

namespace {

namespace gl = ge::lab;
namespace cl = shooters_edge::client;
namespace br = ge::brush;
namespace mo = ge::morph;
namespace ls = ge::lab::liquid_scene;
namespace es = ge::lab::explosion_scene;
namespace lm = ge::core::libm;

// The common server mod's list: the body, the eight weapons, a weapon of none.
const std::vector<std::string> kList = {"vwplayer", "w_axe",  "w_shot",  "w_shot2", "w_nail",
                                        "w_nail2",  "w_rock", "w_rock2", "w_light", "-"};
const char* const kWeapons[] = {
    "axe",           "shotgun",          "super shotgun",   "nailgun",
    "super nailgun", "grenade launcher", "rocket launcher", "lightning gun"};
// The figure's frames: standing with a gun, with the axe.
constexpr int kStand = 12, kAxeStand = 17;

// THE MADE FIGURE (no pack): a tetrahedron's points at (x, y, z), `size` across.
void tetra(std::vector<float>& p, float x, float y, float z, float size) {
  const float pts[4][3] = {{0, 0, 0}, {size, 0, 0}, {0, size, 0}, {0, 0, size}};
  for (const auto& q : pts) p.insert(p.end(), {x + q[0], y + q[1], z + q[2]});
}

mo::Model made_figure() {
  mo::Model m;
  for (const uint32_t b : {0u, 5u, 9u})
    m.indices.insert(m.indices.end(), {b, b + 1, b + 2, b, b + 2, b + 3, b, b + 3, b + 1});
  m.indices.insert(m.indices.end(), {1, 2, 4});
  m.st.assign(size_t{13} * 2, 0.5f);
  mo::Frame stand, axe;
  stand.name = "stand1";
  axe.name = "axstnd1";
  for (mo::Frame* f : {&stand, &axe}) {
    tetra(f->positions, -5, -5, -20, 20);
    f->positions.insert(f->positions.end(), {0, 0, 10});
  }
  stand.positions.insert(stand.positions.end(), {4, 0, 10, 20, 0, 10, 4, 2, 10, 4, 0, 12});
  tetra(axe.positions, -8, 0, 10, 2);
  tetra(stand.positions, 0, 0, 0, 0.5f);
  tetra(axe.positions, 6, 0, 0, 14);
  m.frames = {stand, axe};
  return m;
}

void made(gl::Lab& lab) {
  const mo::Model figure = made_figure();
  ge::classic::FigureParts parts;
  const bool found = ge::classic::figure_parts(figure, parts);
  lab.metric("made.parts-found", found ? 1 : 0, "count");
  lab.metric("made.gun-piece", parts.gun, "index");
  lab.metric("made.axe-piece", parts.axe, "index");
  const ge::classic::OpenModel open = [](std::string_view path, mo::Model& out) {
    if (path != "progs/g_rock.mdl") return false;
    out = {};
    out.indices = {0, 1, 2};
    out.st.assign(6, 0.0f);
    mo::Frame f;
    f.positions = {-10, -1, 24, 30, -1, 24, -10, 1, 30};
    out.frames = {f};
    return true;
  };
  for (const char* name : {"vwplayer", "w_axe", "w_shot", "w_rock", "w_light"}) {
    mo::Model m;
    const bool ok =
        ge::classic::vwep_stand_in(ge::classic::vwep_path(name), figure, parts, open, m);
    lab.metric(std::string("made.") + name + ".vertices", ok ? double(m.vertex_count()) : -1,
               "count");
    if (std::string(name) == "w_rock" && ok)
      lab.metric("made.w_rock.rear-x", std::round(double(m.frames[0].min[0]) * 100) / 100, "units");
  }
}

struct Scene {
  ge::gpu::Device* device = nullptr;
  cl::WorldDraw* world = nullptr;
  ge::gpu::Target* out = nullptr;
  shooters_edge::look::Picture* look = nullptr;
  uint32_t w = 0, h = 0;
};

gl::Picture draw(Scene& s, const std::vector<cl::Placed>& placed, const float eye_u[3],
                 const float at_u[3], double fov_deg) {
  cl::FxFrame fx;
  fx.models = placed;
  ge::render::FrameView v;
  double eye[3], at[3];
  br::to_engine(eye_u, eye);
  br::to_engine(at_u, at);
  for (int c = 0; c < 3; c++) v.eye[c] = eye[c];
  const double dx = at[0] - eye[0], dy = at[1] - eye[1], dz = at[2] - eye[2];
  v.heading = lm::atan2(dx, dz);
  v.pitch = lm::atan2(dy, std::sqrt(dx * dx + dz * dz));
  v.fov_y = fov_deg * 3.14159265358979 / 180;
  v.time_s = 1;
  // Twice: the first frame's draws settle the lights and the last poses.
  cl::draw_world(*s.device, *s.world, v, fx, nullptr, 0, {}, s.out->view);
  v.frame = 1;
  cl::draw_world(*s.device, *s.world, v, fx, nullptr, 0, {}, s.out->view);
  const ge::gpu::Image img = ge::gpu::read_now(*s.device, *s.out);
  return {int(img.width), int(img.height), img.rgba};
}

void real(gl::Lab& lab, const std::string& folder) {
  const auto size = lab.arg_nums("size");
  const uint32_t w = size.size() > 0 ? static_cast<uint32_t>(size[0]) : 1280;
  const uint32_t h = size.size() > 1 ? static_cast<uint32_t>(size[1]) : 720;
  const std::string map_name = lab.arg("map").empty() ? "dm6" : lab.arg("map");
  std::string error;
  cl::SearchPath files;
  const size_t cut = folder.find_last_of('/');
  const std::string base = cut == std::string::npos ? "." : folder.substr(0, cut);
  const std::string game = cut == std::string::npos ? folder : folder.substr(cut + 1);
  if (!cl::open_game_dir(base, game, files, error)) {
    lab.fail("se-vweps: " + error, "Pass --game=<base>/<game folder>. Skill: shooter.");
    return;
  }
  std::unique_ptr<cl::LoadedMap> map = cl::load_named_map(files, map_name, error);
  if (!map) {
    lab.fail("se-vweps: " + error, "Pass a --map the packs hold. Skill: shooter.");
    return;
  }
  // Where the row stands: `--at=x,y,z,yaw` (the row's middle on the floor plus 24, the way they
  // face), else in front of the map's first deathmatch spawn, facing it.
  std::vector<double> at = lab.arg_nums("at");
  if (at.size() < 4) {
    std::vector<br::Entity> ents;
    std::string why;
    br::parse_entities(map->spec.entities, &ents, &why);
    for (const br::Entity& e : ents) {
      if (e.get("classname") != "info_player_deathmatch") continue;
      float o[3] = {0, 0, 0};
      if (std::sscanf(e.get("origin").c_str(), "%f %f %f", &o[0], &o[1], &o[2]) != 3) continue;
      const double yaw = std::atof(e.get("angle").c_str());
      const double r = yaw * 3.14159265358979 / 180;
      at = {o[0] + 150 * lm::cos(r), o[1] + 150 * lm::sin(r), o[2], yaw + 180};
      break;
    }
  }
  if (at.size() < 4) {
    lab.fail("se-vweps: " + map_name + " has no deathmatch spawn",
             "Pass --at=x,y,z,yaw. Skill: shooter.");
    return;
  }
  const double yaw_r = at[3] * 3.14159265358979 / 180;
  const double fwd[2] = {lm::cos(yaw_r), lm::sin(yaw_r)}, right[2] = {fwd[1], -fwd[0]};
  auto device = ge::gpu::create_device({.label = "se-vweps", .validation = 1});
  if (!device) {
    lab.note("No GPU adapter on this machine: no pictures.");
    return;
  }
  const std::vector<std::string> names = {"", "maps/" + map_name + ".bsp", "progs/player.mdl"};
  const cl::ModelLoad load{names, {}, kList};
  shooters_edge::look::Picture look;
  look.look = lab.arg("look").empty() ? "classic" : lab.arg("look");
  cl::WorldDrawPtr world =
      cl::create_world_draw(*device, files, *map, w, h, wgpu::TextureFormat::RGBA8Unorm, look, {},
                            error, load, cl::classic_adapter());
  if (!world) {
    lab.fail("se-vweps: " + error, "Read the gpu: lines above. Skill: render.");
    return;
  }
  const cl::WorldModels& wm = cl::world_models(*world);
  lab.metric("real.vweps-on", wm.vweps.on ? 1 : 0, "count");
  int loaded = 0;
  for (uint32_t i = 0; i < wm.vweps.count; i++) {
    const bool is = wm.kinds[wm.vweps.first + i].is == cl::ModelKind::Is::alias;
    lab.metric("real." + kList[i] + ".loaded", is ? 1 : 0, "count");
    loaded += i > 0 && is;
  }
  lab.metric("real.weapons-loaded", loaded, "count");
  ge::gpu::Target out = ge::gpu::create_target(*device, "se-vweps.final", w, h);
  Scene s{device.get(), world.get(), &out, &look, w, h};

  // THE ROW: eight players, one with each weapon, a step apart, in team colours.
  std::vector<cl::Placed> row;
  for (int i = 0; i < 8; i++) {
    cl::Placed p;
    p.entity = i + 1;
    p.modelindex = 2;
    p.frame = i == 0 ? kAxeStand : kStand;
    const double side = (i - 3.5) * 30;
    p.origin = {float(at[0] + right[0] * side), float(at[1] + right[1] * side), float(at[2])};
    p.angles = {0, float(at[3] - 50), 0};  // turned a little, the weapon seen from the side
    p.top = uint32_t(i % 2 == 0 ? 4 : 13) * 16;
    p.bottom = uint32_t(i % 2 == 0 ? 4 : 13) * 16;
    p.vwep = i + 1;
    row.push_back(p);
  }
  const float eye[3] = {float(at[0] + fwd[0] * 120), float(at[1] + fwd[1] * 120),
                        float(at[2] + 10)};
  const float mid[3] = {float(at[0]), float(at[1]), float(at[2] + 4)};
  std::vector<gl::Picture> pics;
  for (const bool on : {false, true}) {
    look.draw_vweps = on;
    pics.push_back(draw(s, row, eye, mid, 64));
  }
  const std::vector<std::pair<std::string, std::string>> params = {{"map", map_name},
                                                                   {"look", look.look}};
  lab.shot({"row-plain", "R_DRAWVWEPS 0: EVERY FIGURE ITS OWN GUN",
            "Eight players on " + map_name +
                ", left to right holding the axe, shotgun, super shotgun, nailgun, super "
                "nailgun, grenade launcher, rocket launcher and lightning gun, drawn with the "
                "figure's own plain gun (the classic picture).",
            "eight alike figures, the same gun in every pair of hands but the axe's", params},
           pics[0], gl::Det::measured);
  lab.shot({"row-vweps", "R_DRAWVWEPS 1: EACH CARRIES HIS WEAPON",
            "The same eight, each figure's own gun cut away and the weapon he holds in his "
            "hands, frame for frame.",
            "eight different weapons, short to long, in the same hands", params},
           pics[1], gl::Det::measured);

  // CLOSE UP, each weapon in his hands, plain and carried side by side.
  {
    std::vector<gl::Picture> sheet;
    std::vector<std::string> labels;
    for (int weapon = 1; weapon <= 8; weapon++)
      for (const bool on : {false, true}) {
        look.draw_vweps = on;
        cl::Placed p = row[size_t(weapon - 1)];
        p.origin = {float(at[0]), float(at[1]), float(at[2])};
        p.angles = {0, float(at[3]), 0};
        const double r2 = yaw_r - 1.1;
        const float e2[3] = {float(at[0] + lm::cos(r2) * 48), float(at[1] + lm::sin(r2) * 48),
                             float(at[2] + 14)};
        const float m2[3] = {float(at[0]), float(at[1]), float(at[2] + 6)};
        sheet.push_back(gl::shrink(draw(s, {p}, e2, m2, 55), 380));
        labels.push_back(std::string(kWeapons[weapon - 1]) + (on ? ": carried" : ": plain"));
      }
    lab.shot({"weapons-close", "EACH WEAPON IN THE HANDS, PLAIN AND CARRIED",
              "One player close up, side on, holding each weapon in turn: on the left of each pair "
              "the figure's own plain gun (r_drawvweps 0), on the right the weapon he holds.",
              "the plain gun the same in every left picture; eight different weapons on the "
              "right, the axe and the shotgun the figure's own",
              params},
             es::sheet_of(sheet, labels, 4), gl::Det::measured);
  }
  // CLOSE UP through the frames: the rocket launcher and the lightning gun.
  const struct {
    const char* name;
    int frame;
  } frames[] = {{"stand1", 12}, {"rockrun3", 8}, {"rockatt2", 108}, {"nailatt1", 103},
                {"pain3", 37},  {"deatha5", 54}, {"deathc8", 77},   {"deathe9", 102}};
  for (const int weapon : {7, 8}) {
    std::vector<gl::Picture> sheet;
    std::vector<std::string> labels;
    look.draw_vweps = true;
    for (const auto& f : frames) {
      cl::Placed p = row[size_t(weapon - 1)];
      p.origin = {float(at[0]), float(at[1]), float(at[2])};
      p.frame = f.frame;
      p.angles = {0, float(at[3]), 0};
      // From the front and a little to his right, so the weapon is seen side on.
      const double r2 = yaw_r - 0.9;
      const float e2[3] = {float(at[0] + lm::cos(r2) * 62), float(at[1] + lm::sin(r2) * 62),
                           float(at[2] + 10)};
      const float m2[3] = {float(at[0]), float(at[1]), float(at[2] - 4)};
      sheet.push_back(gl::shrink(draw(s, {p}, e2, m2, 60), 400));
      labels.push_back(f.name);
    }
    const std::string id = weapon == 7 ? "frames-rocket" : "frames-lightning";
    lab.shot({id, std::string("THE ") + kWeapons[weapon - 1] + " THROUGH THE FIGURE'S FRAMES",
              "One player holding the " + std::string(kWeapons[weapon - 1]) +
                  ", close up, in eight of the figure's frames: standing, running, firing, in "
                  "pain and in three deaths. The weapon is drawn on the body's frame.",
              "the weapon in his hands on every frame, falling with him as he dies", params},
             es::sheet_of(sheet, labels, 4), gl::Det::measured);
  }
  ls::checked(lab, static_cast<int>(device->errors.load()),
              static_cast<int>(device->pipelines_after_load));
}

void run(gl::Lab& lab) {
  made(lab);
  const std::string folder = lab.arg("game");
  if (!folder.empty()) real(lab, folder);
}

const gl::LabSpec kSpec{
    "se-vweps",
    gl::LabKind::picture,
    "game:shooters_edge",
    "plugins/classic/vweps (the visible weapons: the list, the stand-ins) drawn by "
    "client/world_models",
    "shooter",
    "Does each player carry the weapon he holds, his figure's own gun cut away, frame for frame?",
    {{"game", "", "a game folder of packs (<base>/<folder>): its map and figure, private pictures"},
     {"map", "dm6", "the map with --game"},
     {"at", "", "x,y,z,yaw: where the row stands (default: before the first deathmatch spawn)"},
     {"look", "classic", "the picture's look: classic or modern"},
     {"size", "1280,720", "the frame's width and height, px"}},
    "",
    run,
};
GE_LAB(kSpec);

}  // namespace
