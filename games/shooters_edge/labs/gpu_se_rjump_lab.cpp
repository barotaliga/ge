// LAB `se-rjump`: DO +RJUMP AND +RRJUMP ROCKET-JUMP THE PLAYER IN BOTH GAMES, AS THE SERVER ALLOWS?
// The rocket jump helpers (plugins/shooter/rjump_assist.hpp, games/shooters_edge/server/
// rjump_rules.hpp) on the window's own server, once on the first game's rules (the baseline mod)
// and once on the sequel's, each on a real map when its files are given.
//
// FOUR RUNS A GAME, each from the same spot (the most open place the map puts a thing, or
// `--spot` in the first game), the launcher and
// rockets given and another weapon in hand: a plain jump; +rjump (the request with a jump, the view
// level); +rrjump with the view 60 degrees down (the rocket behind, the push forward); and +rjump
// with `sv_rjump 0` (refused, the player told). The player's height over time and his path from the
// side for each, the rockets spent, the health the blasts took and the push.
//
// THE EYES: from the player's eyes at the top of the +rjump climb, the view as he left it (level:
// the helper turned only the shot), in each game.
//
// ge:summary Lab: the rocket jump helpers (+rjump, +rrjump) in both games, gated by sv_rjump
// ge:keywords lab se-rjump rjump rrjump rocket-jump sv_rjump sv_rrjump listen-server sequel
// ge:keywords height-over-time accessibility
// ge:skill shooter labs
// ge:test tests/shooters_edge_rjump_test.cpp
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <memory>
#include <string>
#include <vector>

#include "brush/read.hpp"
#include "brush/trace.hpp"
#include "brush/write.hpp"
#include "classicenhance/profile.hpp"
#include "core/libm/libm.hpp"
#include "core/num_text.hpp"
#include "gpu/device.hpp"
#include "gpu/target.hpp"
#include "lab/chart.hpp"
#include "lab/lab.hpp"
#include "lab/labs/liquid_scene.hpp"
#include "lab/raster.hpp"
#include "shooter/client.hpp"
#include "shooter/rjump_assist.hpp"
#include "shooter/sequel_move.hpp"
#include "shooters_edge/app/look.hpp"
#include "shooters_edge/client/files.hpp"
#include "shooters_edge/client/game_adapter.hpp"
#include "shooters_edge/client/listen.hpp"
#include "shooters_edge/client/present.hpp"
#include "shooters_edge/client/view.hpp"
#include "shooters_edge/client/world_draw.hpp"
#include "shooters_edge/client/world_fx.hpp"
#include "shooters_edge/net/classic_game.hpp"
#include "shooters_edge/profiles/game_profile.hpp"
#include "shooters_edge/server/sequel_items.hpp"
#include "shooters_edge/server/sequel_server.hpp"
#include "vfs/pak.hpp"

namespace {

namespace gl = ge::lab;
namespace cl = shooters_edge::client;
namespace sv = shooters_edge::server;
namespace sh = ge::shooter;
namespace br = ge::brush;
namespace pf = shooters_edge::profiles;
namespace ls = ge::lab::liquid_scene;
using ge::core::num::to_fixed;

constexpr int kRate = 77;  // the window's commands a second

enum class Way : uint8_t { jump, rjump, rrjump, refused };
constexpr const char* kWayNames[] = {"plain jump", "+rjump", "+rrjump, 60 down",
                                     "+rjump, sv_rjump 0"};

br::Entity entity(std::initializer_list<std::pair<std::string, std::string>> pairs) {
  br::Entity e;
  for (const auto& p : pairs) e.pairs.push_back(p);
  return e;
}

std::vector<uint8_t> pack_of(const std::string& name, std::vector<uint8_t> bsp, std::string& err) {
  std::vector<uint8_t> out;
  ge::vfs::write_pak({{"maps/" + name + ".bsp", std::move(bsp)}}, &out, &err);
  return out;
}

// Our own fixtures when no files are given: the first game's halls and the sequel's tall hall.
bool classic_fixture(cl::SearchPath& files, std::string& err) {
  br::Fixture f = br::sample_fixture();
  f.entities.push_back(
      entity({{"classname", "info_player_deathmatch"}, {"origin", "128 192 40"}, {"angle", "0"}}));
  std::vector<uint8_t> bsp;
  if (!br::write_bsp(f, br::Family::v29, &bsp, &err)) return false;
  return cl::add_archive(files, "pak0.pak", pack_of("halls", std::move(bsp), err), err);
}

bool sequel_fixture(cl::SearchPath& files, std::string& err) {
  br::BrushWorld world;
  world.models.resize(1);
  const auto box = [&](float x0, float y0, float z0, float x1, float y1, float z1) {
    const float lo[3] = {x0, y0, z0}, hi[3] = {x1, y1, z1};
    world.models[0].push_back(br::box_brush(lo, hi, sh::kSqSolid, "world/wall"));
  };
  box(-1024, -1024, -16, 1024, 1024, 0);
  box(-1040, -1024, -16, -1024, 1024, 1024);
  box(1024, -1024, -16, 1040, 1024, 1024);
  box(-1040, 1024, -16, 1040, 1040, 1024);
  box(-1040, -1040, -16, 1040, -1024, 1024);
  box(-1040, -1040, 1024, 1040, 1040, 1040);
  world.entities.push_back(
      entity({{"classname", "info_player_start"}, {"origin", "0 -256 24"}, {"angle", "90"}}));
  std::vector<uint8_t> bsp;
  if (!br::write_brush_world(world, &bsp, &err)) return false;
  return cl::add_archive(files, "pak0.pak", pack_of("hall", std::move(bsp), err), err);
}

struct Run {
  gl::Trace height, side;  // z over time; z over the distance along the facing
  float rise = 0;          // the highest point above the start, units
  float forward = 0;       // the fastest speed along the facing after the press, units/s
  float rockets = 0;       // rockets spent
  float health = 0;        // health the blasts took
  bool told = false;       // "The rocket jump helper is off on this server." printed
};

struct Spot {
  bool set = false;
  sh::Vec3f at;
  float yaw = 0;
};

// The player's numbers either game keeps.
struct Player {
  cl::Listen& l;
  sv::SequelServer* sq = nullptr;
  sh::Vec3f origin() const { return l.server->world.entities[1].origin; }
  float rockets() const {
    return sq != nullptr ? float(sq->players[0].body.ammo[int(sv::SqAmmo::rockets)])
                         : cl::field_named(l, 1, "ammo_rockets");
  }
  float health() const {
    return sq != nullptr ? sq->players[0].health : l.server->sv.edicts[1].health;
  }
  // The launcher and 50 rockets given, another weapon kept in hand.
  void arm() {
    if (sq != nullptr) {
      sv::SequelPlayer& p = sq->players[0];
      p.body.counts[sv::sq_item("weapon_rocketlauncher")] = 1;
      p.body.ammo[int(sv::SqAmmo::rockets)] = 50;
      return;
    }
    const float items = cl::field_named(l, 1, "items");
    cl::set_field_named(l, 1, "items", float(int(items) | 32));
    cl::set_field_named(l, 1, "ammo_rockets", 50);
  }
  // A box trace through the map's own brushes (the player's box), either game.
  float sweep(sh::Vec3f a, sh::Vec3f b, sh::Vec3f* end = nullptr) const {
    if (sq != nullptr) {
      const float lo[3] = {-16, -16, -24}, hi[3] = {16, 16, 32};
      const float from[3] = {a.x, a.y, a.z}, to[3] = {b.x, b.y, b.z};
      const ge::brush::Trace t = sq->tracer->trace(from, to, lo, hi, sh::kSqMaskPlayerSolid);
      if (end != nullptr) *end = {t.end[0], t.end[1], t.end[2]};
      return t.start_solid ? 0 : t.fraction;
    }
    const sh::Server& srv = l.server->sv;
    sh::MoveWorld world;
    int ents[sh::kMaxSolids];
    sh::build_player_world(srv, 1, a, world, ents);
    world.count = 1;  // the map's walls and floors alone
    const sh::HullTrace t = sh::player_trace(world, a, b);
    if (end != nullptr) *end = t.endpos;
    return t.startsolid ? 0 : t.fraction;
  }
  // The open spot the runs start from: of every place the map puts a thing (its items and spawns),
  // stood on the floor under it, the one with the most room overhead (to 800 units); facing the
  // longest clear run (to 512) of eight headings there, so the flights meet no wall early.
  void to_open_spot() {
    // The places a player stands or a thing lies: the spawns, weapons, items and ammo.
    std::vector<ge::brush::Entity> parsed;
    const std::vector<ge::brush::Entity>* ents = &parsed;
    if (l.map->spec.brushes != nullptr) {
      ents = &l.map->spec.brushes->entities;
    } else {
      std::string why;
      ge::brush::parse_entities(l.map->spec.entities, &parsed, &why);
    }
    std::vector<sh::Vec3f> places;
    for (const ge::brush::Entity& e : *ents) {
      const std::string& c = e.get("classname");
      if (c.rfind("info_player", 0) != 0 && c.rfind("weapon_", 0) != 0 &&
          c.rfind("item_", 0) != 0 && c.rfind("ammo_", 0) != 0)
        continue;
      sh::Vec3f o;
      if (std::sscanf(e.get("origin").c_str(), "%f %f %f", &o.x, &o.y, &o.z) == 3)
        places.push_back(o);
    }
    float best = -1, best_yaw = 0;
    sh::Vec3f best_at = origin();
    for (const sh::Vec3f p : places) {
      sh::Vec3f floor;
      if (sweep({p.x, p.y, p.z + 32}, {p.x, p.y, p.z - 256}, &floor) >= 1) continue;  // no floor
      floor.z += 1;
      if (sweep(floor, floor) == 0) continue;  // stuck
      const float room = sweep(floor, {floor.x, floor.y, floor.z + 800}) * 800;
      float run = 0, yaw = 0;
      for (int k = 0; k < 8; k++) {
        const double r = k * 45 * 3.14159265358979323846 / 180;
        const auto cx = static_cast<float>(ge::core::libm::cos(r)),
                   cy = static_cast<float>(ge::core::libm::sin(r));
        const float len = sweep(floor, {floor.x + cx * 512, floor.y + cy * 512, floor.z}) * 512;
        if (len > run) run = len, yaw = float(k * 45);
      }
      const float score = room + run;
      if (score > best) best = score, best_at = floor, best_yaw = yaw;
    }
    if (sq != nullptr) {
      sh::SequelState& st = sq->players[0].move;
      st.origin[0] = int16_t(best_at.x * 8), st.origin[1] = int16_t(best_at.y * 8);
      st.origin[2] = int16_t(best_at.z * 8);
      for (int16_t& v : st.velocity) v = 0;
    } else {
      sh::Edict& me = l.server->sv.edicts[1];
      me.origin = best_at;
      me.velocity = {};
    }
    l.view = {0, best_yaw, 0};
  }
  void place(const Spot& s) {
    if (!s.set) {
      to_open_spot();
      return;
    }
    sh::Edict& me = l.server->sv.edicts[1];
    me.origin = s.at;
    me.velocity = {};
    l.view = {0, s.yaw, 0};
  }
};

using Eyes = void (*)(void* ctx, cl::Listen& l);

// One run of `way` on `map_name` in `files`: settled, armed, then the press and four seconds.
bool run_way(const cl::SearchPath& files, const std::string& map_name, Way way, const Spot& spot,
             Run& out, std::string& err, void* eyes_ctx = nullptr, Eyes eyes = nullptr) {
  static const ge::progs::Progs progs = [] {
    ge::progs::Progs p;
    std::string why;
    cl::baseline_progs(p, why);
    return p;
  }();
  std::unique_ptr<cl::LoadedMap> map = cl::load_named_map(files, map_name, err);
  if (!map) return false;
  cl::Listen l;
  if (!cl::listen_start(l, progs, std::move(map), shooters_edge::net::listen_options(), err))
    return false;
  Player me{l, sv::sequel_of(*l.rules)};
  for (int i = 0; i < kRate * 3 / 2; i++) cl::listen_frame(l, {});  // spawned and landed
  me.place(spot);
  me.arm();
  for (int i = 0; i < kRate; i++) cl::listen_frame(l, {});  // settled, the weapon in hand
  if (way == Way::refused) l.rules->console("sv_rjump 0");
  if (way == Way::rrjump) l.view.x = 60;
  const sh::Vec3f start = me.origin();
  const double yaw = double(l.view.y) * 3.14159265358979323846 / 180;
  const float fx = static_cast<float>(ge::core::libm::cos(yaw)),
              fy = static_cast<float>(ge::core::libm::sin(yaw));
  const float rockets = me.rockets(), health = me.health();
  out.height.label = out.side.label = kWayNames[int(way)];
  l.prints.clear();
  sh::MoveCmd cmd;
  cmd.buttons = sh::kButtonJump;
  if (way != Way::jump)
    cmd.impulse = sh::rjump_impulse(way == Way::rrjump ? sh::Rjump::back : sh::Rjump::down);
  float top = start.z;
  bool seen = false;
  for (int i = 0; i < 4 * kRate; i++) {
    cl::listen_frame(l, cmd);
    cmd = {};
    const sh::Vec3f at = me.origin();
    const sh::Vec3f v = l.server->sv.edicts[1].velocity;
    out.height.x.push_back(double(i + 1) / kRate);
    out.height.y.push_back(at.z - start.z);
    out.side.x.push_back((at.x - start.x) * fx + (at.y - start.y) * fy);
    out.side.y.push_back(at.z - start.z);
    out.forward = std::max(out.forward, v.x * fx + v.y * fy);
    if (at.z > top) top = at.z;
    // The top of the climb: the first frame he falls after rising 64 or more.
    if (eyes != nullptr && !seen && top - start.z > 64 && at.z < top) {
      seen = true;
      eyes(eyes_ctx, l);
    }
  }
  out.rise = top - start.z;
  out.rockets = rockets - me.rockets();
  out.health = health - me.health();
  for (const std::string& p : l.prints)
    out.told =
        out.told || p.find("The rocket jump helper is off on this server.") != std::string::npos;
  return true;
}

void shoot_chart(gl::Lab& lab, const std::string& id, const gl::LineChart& spec,
                 const std::string& caption, const std::string& look_for) {
  const gl::Chart chart = gl::line_chart(spec);
  const std::string svg = gl::to_svg(chart.plot);
  lab.attach(id + ".svg", std::vector<uint8_t>(svg.begin(), svg.end()));
  lab.shot({id, chart.title, caption, look_for, {}}, gl::to_picture(chart.plot));
}

struct EyesCtx {
  gl::Lab* lab = nullptr;
  ge::gpu::Device* device = nullptr;
  const cl::SearchPath* files = nullptr;
  const cl::GameAdapter* adapter = nullptr;
  std::string game, map;
  uint32_t w = 0, h = 0;
  bool own = false;  // our own fixture, not the game's files
};

void draw_eyes(void* ctx, cl::Listen& l) {
  EyesCtx& c = *static_cast<EyesCtx*>(ctx);
  gl::Lab& lab = *c.lab;
  const std::string id = c.game + "-eyes";
  if (c.device == nullptr) {
    ls::placeholders(lab, {id.c_str()});
    return;
  }
  shooters_edge::look::Picture look;
  if (c.own) look.far_land = "off";  // our fixtures stand in no landscape
  std::string err;
  cl::WorldDrawPtr world =
      cl::create_world_draw(*c.device, *c.files, *l.map, c.w, c.h, wgpu::TextureFormat::RGBA8Unorm,
                            look, {}, err, {l.server->models, {}}, *c.adapter);
  if (!world) {
    lab.fail(err, "Read the gpu: lines above. Skill: render.");
    return;
  }
  ge::gpu::Target target = ge::gpu::create_target(*c.device, "se-rjump.final", c.w, c.h);
  cl::ViewState shown;
  cl::FxState fx_state;
  cl::FxFrame fx;
  const double t = 6;
  cl::frame_view(l, shown, 1, t, 0.013);
  const ge::render::FrameView frame = cl::frame_view(l, shown, 0, t, 0);
  cl::step_fx(l, shown, fx_state, t, 0, fx);
  cl::draw_world(*c.device, *world, frame, fx, nullptr, 0, {}, target.view);
  const ge::gpu::Image img = ge::gpu::read_now(*c.device, target);
  lab.shot({id,
            "AT THE TOP OF +RJUMP: " + c.map,
            "From the player's eyes on " + c.map + " (the " + c.game +
                " game's rules) at the top of the climb +rjump gave him. His view is where he "
                "left it, level: the helper turned the rocket's shot down for that one command, "
                "never his screen.",
            "the floor far below, the view level (not pitched down)",
            {{"map", c.map}, {"game", c.game}}},
           gl::Picture{static_cast<int>(img.width), static_cast<int>(img.height), img.rgba},
           gl::Det::measured);
}

bool parse_spot(const std::string& text, Spot& s) {
  if (text.empty()) return true;
  float v[4] = {};
  if (std::sscanf(text.c_str(), "%f,%f,%f,%f", &v[0], &v[1], &v[2], &v[3]) != 4) return false;
  s = {true, {v[0], v[1], v[2]}, v[3]};
  return true;
}

void one_game(gl::Lab& lab, ge::gpu::Device* device, const std::string& game,
              const std::string& dir, std::string map, const std::string& spot_text, uint32_t w,
              uint32_t h) {
  const pf::GameProfile& profile = *pf::find_game(game);
  const cl::GameAdapter& adapter = cl::adapter_of(profile);
  const ge::classicenhance::Profile& content = pf::content_of(profile);
  std::string err;
  cl::SearchPath files;
  const bool own = dir.empty();
  const bool opened =
      own ? (game == "sequel" ? sequel_fixture(files, err) : classic_fixture(files, err))
          : cl::open_game_dir(dir, std::string(content.base_dir), files, err, content);
  if (!opened) {
    lab.fail(game + ": " + err, "Pass the folder holding " + std::string(content.base_dir) +
                                    "/ with its packs. Skill: shooter.");
    return;
  }
  if (own) map = game == "sequel" ? "hall" : "halls";
  Spot spot;
  if (!parse_spot(spot_text, spot)) {
    lab.fail("--spot \"" + spot_text + "\" is not x,y,z,yaw", "Give four numbers. Skill: labs.");
    return;
  }
  EyesCtx eyes{&lab, device, &files, &adapter, game, map, w, h, own};
  Run runs[4];
  for (int k = 0; k < 4; k++) {
    const bool shoot = k == int(Way::rjump);
    if (!run_way(files, map, Way(k), spot, runs[k], err, shoot ? &eyes : nullptr,
                 shoot ? &draw_eyes : nullptr)) {
      lab.fail(std::string(game).append(" on ").append(map).append(": ").append(err),
               "The map should load on the window's server. "
               "Skill: shooter.");
      return;
    }
  }
  const char* keys[] = {"jump", "rjump", "rrjump", "refused"};
  for (int k = 0; k < 4; k++) {
    const std::string at = game + "." + keys[k];
    lab.metric(at + ".rise", runs[k].rise, "units", gl::Det::measured);
    lab.metric(at + ".rockets", runs[k].rockets, "count", gl::Det::measured);
    lab.metric(at + ".health-lost", runs[k].health, "hp", gl::Det::measured);
    lab.metric(at + ".forward", runs[k].forward, "u/s", gl::Det::measured);
  }
  const Run& plain = runs[int(Way::jump)];
  const Run& down = runs[int(Way::rjump)];
  const Run& back = runs[int(Way::rrjump)];
  const Run& refused = runs[int(Way::refused)];
  if (down.rockets != 1 || down.rise < plain.rise * 2)
    lab.fail(game + ": +rjump rose " + to_fixed(down.rise, 0) + " (a plain jump " +
                 to_fixed(plain.rise, 0) + ") on " + to_fixed(down.rockets, 0) + " rockets",
             "Check the request reached the runner (rjump_packet) and the launcher's readiness. "
             "Skill: shooter.");
  if (back.rockets != 1 || back.forward < 150)
    lab.fail(game + ": +rrjump pushed forward at " + to_fixed(back.forward, 0) + " u/s on " +
                 to_fixed(back.rockets, 0) + " rockets",
             "Check the back shot's turned yaw (rjump_command). Skill: shooter.");
  if (refused.rockets != 0 || !refused.told)
    lab.fail(game + ": with sv_rjump 0 the helper fired or said nothing",
             "Check rjump_allowed. Skill: shooter.");
  gl::LineChart height;
  height.spikes = false;
  height.frame = {std::string(game == "sequel" ? "THE SEQUEL" : "THE FIRST GAME") +
                      ": HEIGHT OVER TIME, " + map,
                  "the window's own server, the same spot, four presses", "t (s)",
                  "height above the start (units)"};
  height.traces = {plain.height, down.height, back.height, refused.height};
  shoot_chart(lab, game + "-height", height,
              "The player's height after each press on " + map + ": a plain jump rises " +
                  to_fixed(plain.rise, 0) +
                  "; +rjump fires a rocket at his feet as he jumps and "
                  "rises " +
                  to_fixed(down.rise, 0) + " (" + to_fixed(down.health, 0) +
                  " health to the blast); +rrjump, looking 60 down, fires behind him and rises " +
                  to_fixed(back.rise, 0) + " while thrown forward at " + to_fixed(back.forward, 0) +
                  " u/s; with sv_rjump 0 the press is refused, he "
                  "is told so and only jumps.",
              "the +rjump line climbing far above the plain jump; the refused line as the plain");
  gl::LineChart side;
  side.spikes = false;
  side.frame = {std::string(game == "sequel" ? "THE SEQUEL" : "THE FIRST GAME") +
                    ": THE FLIGHTS FROM THE SIDE, " + map,
                "along the way he faced", "distance along his facing (units)", "height (units)"};
  side.traces = {plain.side, down.side, back.side};
  shoot_chart(lab, game + "-side", side,
              "The same flights from the side: +rjump goes nearly straight up (the rocket lands "
              "a little ahead of his feet), +rrjump throws him forward, the way he faces.",
              "the +rrjump arc reaching forward, the +rjump one tall and short");
}

void run(gl::Lab& lab) {
  const auto size = lab.arg_nums("size");
  const uint32_t w = size.size() > 0 ? static_cast<uint32_t>(size[0]) : 960;
  const uint32_t h = size.size() > 1 ? static_cast<uint32_t>(size[1]) : 540;
  auto device = ge::gpu::create_device({.label = "se-rjump", .validation = 1});
  if (!device && ls::gpu_required())
    lab.fail("no GPU device, and GE_GPU_REQUIRE=1",
             "Install Mesa's Vulkan drivers (apt install mesa-vulkan-drivers). Skill: gpu.");
  one_game(lab, device ? &*device : nullptr, "classic", lab.arg("classic"), lab.arg("map"),
           lab.arg("spot"), w, h);
  one_game(lab, device ? &*device : nullptr, "sequel", lab.arg("sequel"), lab.arg("sequel-map"), "",
           w, h);
}

const gl::LabSpec kSpec{
    "se-rjump",
    gl::LabKind::picture,
    "game:shooters_edge",
    "the rocket jump helpers (+rjump, +rrjump: shooter/rjump_assist.hpp, server/rjump_rules.hpp)",
    "shooter",
    "Do +rjump and +rrjump rocket-jump the player on the first game's rules and the sequel's, and "
    "does sv_rjump 0 refuse them?",
    {{"size", "960,540", "the eyes' pictures' width and height, px"},
     {"classic", "",
      "the folder holding the first game's id1/ packs (private pictures only); none: our halls"},
     {"map", "dm2", "the first game's map"},
     {"spot", "", "x,y,z,yaw the first game's runs start from (none: the map's most open place)"},
     {"sequel", "",
      "the folder holding the sequel's baseq2/ packs (private pictures only); none: our hall"},
     {"sequel-map", "q2dm1", "the sequel's map"}},
    "--size=320,180",
    run,
};
GE_LAB(kSpec);

}  // namespace
