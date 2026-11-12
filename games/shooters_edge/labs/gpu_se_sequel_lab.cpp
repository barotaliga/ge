// LAB `se-sequel`: THE SEQUEL'S MAPS ENTERED. A game folder of the sequel (its base folder of
// numbered packs, as an install lays it out) opened through the client's search path with the
// sequel's content row and adapter (client/sequel_adapter.cpp: the calls every classic game shares,
// client/adapter_parts.hpp), each map read (version 38: the textures named in the map, their
// pixels in wall texture files, plugins/brush/wal.hpp, coloured by the palette at the end of a PCX
// picture), its world made as the window makes it (client/world_draw.hpp) and drawn from the
// player's start looking four ways, each view the classic look on the left and the modern look
// (every classic-enhance switch at its default) on the right. The counts (maps, textures, textures
// found in their files, entities, starts) are deterministic metrics; the pictures are measured.
// A map with brushes is then played on the window's own listen server (the sequel's rules on its
// brushes): the player stands at his spawn and walks forward for two seconds, and the view from
// his eye where he stopped is drawn (`<map>-walked`).
//
// Without `--game` the map is our own: the brush plugin's fixture halls written as a version 38 map
// with a wall texture file for each of its textures and a palette picture of our own, in a pack, so
// CI needs no game data. With `--game=<folder>` (the folder holding the sequel's base folder) the
// maps come from its packs (`--maps=<a>,<b>`, else every map, the start map first), for PRIVATE
// pictures only: the demo's packs are fetched at run time and never committed.
//
// ge:summary Lab: the sequel's maps entered: its packs, wall textures and palette, drawn four ways
// ge:keywords lab se-sequel sequel second-game v38 wal wall-texture pcx palette colormap demo-pack
// ge:keywords enter-map player-start classic modern look sequel-adapter content-row walk listen
// ge:skill shooter brush labs
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <span>
#include <string>
#include <utility>
#include <vector>

#include "assets/pcx.hpp"
#include "brush/read.hpp"
#include "brush/wal.hpp"
#include "brush/write.hpp"
#include "classicenhance/profile.hpp"
#include "gpu/device.hpp"
#include "gpu/target.hpp"
#include "lab/lab.hpp"
#include "lab/labs/liquid_scene.hpp"
#include "lab/raster.hpp"
#include "shooters_edge/app/look.hpp"
#include "shooters_edge/client/files.hpp"
#include "shooters_edge/client/game_adapter.hpp"
#include "shooters_edge/client/listen.hpp"
#include "shooters_edge/client/view.hpp"
#include "shooters_edge/client/world_draw.hpp"
#include "shooters_edge/net/classic_game.hpp"
#include "shooters_edge/profiles/game_profile.hpp"
#include "shooters_edge/server/sequel_server.hpp"
#include "vfs/pak.hpp"

namespace {

namespace gl = ge::lab;
namespace cl = shooters_edge::client;
namespace ls = ge::lab::liquid_scene;
namespace pf = shooters_edge::profiles;
namespace sh = ge::shooter;

constexpr float kEyeHeight = 22;  // the sequel's standing eye over a start's origin, units
constexpr const char* kWays[] = {"ahead", "left", "behind", "right"};

// Our own palette: eight ramps of 32, so the fixture's textures have colours to show.
std::vector<uint8_t> fixture_palette() {
  std::vector<uint8_t> rgb(768);
  const int tint[8][3] = {{3, 3, 3}, {4, 2, 1}, {2, 3, 4}, {3, 4, 2},
                          {4, 3, 2}, {2, 2, 3}, {4, 4, 3}, {3, 2, 2}};
  for (int i = 0; i < 256; i++) {
    const int ramp = i / 32, v = (i % 32) * 2 + 8;
    for (int k = 0; k < 3; k++)
      rgb[size_t(i) * 3 + size_t(k)] = uint8_t(std::min(255, v * tint[ramp][k]));
  }
  return rgb;
}

// The fixture halls as the sequel's files: the map, a wall texture a texture (bricks of two
// shades of one ramp each), and the palette picture.
bool fixture_pack(std::vector<uint8_t>& out, std::string& error) {
  std::vector<std::pair<std::string, std::vector<uint8_t>>> files;
  std::vector<uint8_t> bsp;
  if (!ge::brush::write_bsp(ge::brush::sample_fixture(), ge::brush::Family::v38, &bsp, &error))
    return false;
  ge::brush::Map map;
  if (!ge::brush::read_bsp(bsp, &map, &error)) return false;
  for (size_t t = 0; t < map.textures.size(); t++) {
    std::vector<uint8_t> px(size_t{64} * 64);
    const auto base = uint8_t(32 * (1 + t % 7) + 10);
    for (uint32_t y = 0; y < 64; y++)
      for (uint32_t x = 0; x < 64; x++) {
        const uint32_t shift = (y / 16) % 2 != 0 ? 16 : 0;
        const bool mortar = y % 16 == 0 || (x + shift) % 32 == 0;
        px[size_t(y) * 64 + x] = mortar ? uint8_t(base - 8) : uint8_t(base + (x * 7 + y * 3) % 9);
      }
    ge::brush::WalHead head;
    head.name = map.textures[t].name;
    files.emplace_back(ge::brush::wal_path(map.textures[t].name),
                       ge::brush::write_wal(head, 64, 64, px));
  }
  ge::assets::IndexedImage pal;
  pal.width = 16;
  pal.height = 16;
  pal.indices.resize(256);
  for (int i = 0; i < 256; i++) pal.indices[size_t(i)] = uint8_t(i);
  pal.palette = fixture_palette();
  files.emplace_back("pics/colormap.pcx", ge::assets::encode_pcx(pal));
  files.emplace_back("maps/fixture.bsp", std::move(bsp));
  return ge::vfs::write_pak(files, &out, &error);
}

// The two looks side by side, a gap between.
// The walk: the window's listen server on the map (a map of brushes plays on the sequel's rules,
// net/classic_game.cpp), the player stood for a second at his spawn, then turned `turn` degrees
// left of the spawn's facing and walked forward for two seconds. Where he ended, his facing,
// whether he stood.
struct Walk {
  bool started = false, stood = false;
  sh::Vec3f from, to;
  float facing = 0, eye = 0;   // degrees; the eye over the origin, units
  int items = 0, removed = 0;  // the map's items standing, and those removed (in a solid, left out)
  int taken = 0;               // the items the walk took
  struct Dot {
    sh::Vec3f at;
    int kind;  // sequel_items.hpp's SqKind
  };
  std::vector<Dot> dots;        // the items standing
  std::vector<sh::Vec3f> path;  // where the walk went, every 10 ticks
};

// The map from above: each item standing as a disc coloured by its kind, the walk's path, its
// start ringed. Scaled to the world's bounds.
gl::Picture overhead(const ge::brush::Map& map, const Walk& walk, const std::string& title) {
  constexpr int kSize = 640, kPad = 24;
  gl::Picture p = gl::create_picture(kSize, kSize + 64, {18, 18, 22});
  if (map.models.empty()) return p;
  const float* lo = map.models[0].mins;
  const float* hi = map.models[0].maxs;
  const float span = std::max({hi[0] - lo[0], hi[1] - lo[1], 1.0f});
  const auto px = [&](sh::Vec3f v) {
    return std::pair<double, double>{kPad + (v.x - lo[0]) / span * (kSize - 2 * kPad),
                                     kPad + (hi[1] - v.y) / span * (kSize - 2 * kPad)};
  };
  for (int g = 0; g <= 8; g++) {  // a grid of the world's span in eighths
    const double at = kPad + g * (kSize - 2 * kPad) / 8.0;
    gl::draw_line(p, at, kPad, at, kSize - kPad, {40, 40, 48});
    gl::draw_line(p, kPad, at, kSize - kPad, at, {40, 40, 48});
  }
  const gl::Rgba kinds[] = {{90, 160, 255}, {120, 200, 255}, {255, 150, 60},  {230, 200, 80},
                            {90, 220, 110}, {220, 90, 230},  {255, 255, 255}, {200, 200, 200}};
  for (const Walk::Dot& d : walk.dots) {
    const auto [x, y] = px(d.at);
    gl::fill_disc(p, x, y, 4, kinds[d.kind]);
  }
  for (size_t i = 1; i < walk.path.size(); i++) {
    const auto [x0, y0] = px(walk.path[i - 1]);
    const auto [x1, y1] = px(walk.path[i]);
    gl::draw_line(p, x0, y0, x1, y1, {255, 80, 80});
  }
  if (!walk.path.empty()) {
    const auto [x, y] = px(walk.path.front());
    gl::draw_ring(p, x, y, 7, {255, 80, 80});
  }
  const char* names[] = {"ARMOUR", "POWER", "WEAPON", "AMMO", "HEALTH", "POWERUP", "KEY", "PACK"};
  for (int k = 0; k < 8; k++) {
    gl::fill_disc(p, 16 + k * 78, kSize + 14, 4, kinds[k]);
    gl::draw_text(p, 24 + k * 78, kSize + 10, names[k], {220, 220, 220});
  }
  gl::draw_text(p, 12, kSize + 36, title + ": ITEMS FROM ABOVE, THE WALK IN RED", {230, 230, 230});
  return p;
}

Walk walk(const std::vector<uint8_t>& file, const std::string& name, float turn,
          std::string& error) {
  Walk out;
  static const ge::progs::Progs progs = [] {
    ge::progs::Progs p;
    std::string why;
    cl::baseline_progs(p, why);
    return p;
  }();
  std::unique_ptr<cl::LoadedMap> map = cl::map_from_bytes(file, name, error);
  if (!map) return out;
  cl::Listen l;
  if (!cl::listen_start(l, progs, std::move(map), shooters_edge::net::listen_options(), error))
    return out;
  out.started = true;
  if (const shooters_edge::server::SequelServer* sq = shooters_edge::server::sequel_of(*l.rules)) {
    for (const shooters_edge::server::SequelEntity& e : sq->entities)
      if (e.item >= 0) {
        (e.gone ? out.removed : out.items)++;
        if (!e.gone)
          out.dots.push_back(
              {e.origin, static_cast<int>(shooters_edge::server::kSqItems[e.item].kind)});
      }
  }
  const auto me = [&] { return l.server->world.entities[1]; };
  for (int i = 0; i < 80; i++) cl::listen_frame(l, {});  // about a second: spawned, landed
  out.stood = (me().flags & sh::kFlagOnGround) != 0;
  out.from = me().origin;
  l.view.y = turn;
  sh::MoveCmd ahead;
  ahead.forward = 200;
  for (int i = 0; i < 154; i++) {  // about two seconds
    cl::listen_frame(l, ahead);
    if (i % 10 == 0) out.path.push_back(me().origin);
  }
  out.to = me().origin;
  out.facing = me().view_angles.y;
  out.eye = l.server->world.clients[0].numbers.view_z;
  if (const shooters_edge::server::SequelServer* sq = shooters_edge::server::sequel_of(*l.rules))
    out.taken = static_cast<int>(sq->players[0].taken.size());
  cl::listen_stop(l);
  return out;
}

gl::Picture side_by_side(const ge::gpu::Image& a, const ge::gpu::Image& b) {
  const int w = int(a.width), h = int(a.height), gap = 4;
  gl::Picture out = gl::create_picture(2 * w + gap, h, {0, 0, 0});
  for (int y = 0; y < h; y++) {
    std::copy_n(a.rgba.begin() + ptrdiff_t(y) * w * 4, size_t(w) * 4,
                out.rgba.begin() + ptrdiff_t(y) * out.width * 4);
    std::copy_n(b.rgba.begin() + ptrdiff_t(y) * w * 4, size_t(w) * 4,
                out.rgba.begin() + (ptrdiff_t(y) * out.width + w + gap) * 4);
  }
  return out;
}

void run(gl::Lab& lab) {
  const auto size = lab.arg_nums("size");
  const uint32_t w = size.size() > 0 ? static_cast<uint32_t>(size[0]) : 640;
  const uint32_t h = size.size() > 1 ? static_cast<uint32_t>(size[1]) : 360;
  const pf::GameProfile& game = *pf::find_game("sequel");
  const cl::GameAdapter& adapter = cl::adapter_of(game);
  const ge::classicenhance::Profile& content = pf::content_of(game);
  std::string error;
  cl::SearchPath files;
  const bool own = lab.arg("game").empty();
  if (own) {
    std::vector<uint8_t> pak;
    if (!fixture_pack(pak, error) || !cl::add_archive(files, "pak0.pak", std::move(pak), error)) {
      lab.fail(error, "The fixture's files should write and mount. Skill: brush.");
      return;
    }
  } else if (!cl::open_game_dir(lab.arg("game"), std::string(content.base_dir), files, error,
                                content)) {
    lab.fail(error, "Pass --game=<the folder holding " + std::string(content.base_dir) +
                        "/pak0.pak>. Skill: shooter.");
    return;
  }
  std::vector<std::string> maps = adapter.list_maps(adapter, files);
  if (const std::string& asked = lab.arg("maps"); !asked.empty()) {
    maps.clear();
    for (size_t at = 0; at <= asked.size();) {
      const size_t comma = std::min(asked.find(',', at), asked.size());
      if (comma > at) maps.push_back(asked.substr(at, comma - at));
      at = comma + 1;
    }
  }
  lab.metric("maps", double(maps.size()), "count");
  std::vector<uint8_t> pal_store;
  lab.metric("palette.bytes", double(cl::game_palette(files, pal_store, content).size()), "bytes");
  if (maps.empty()) {
    lab.fail("the game's files hold no maps", "Check --game's packs. Skill: shooter.");
    return;
  }

  auto device = ge::gpu::create_device({.label = "se-sequel", .validation = 1});
  if (!device && ls::gpu_required())
    lab.fail("no GPU device, and GE_GPU_REQUIRE=1",
             "Install Mesa's Vulkan drivers (apt install mesa-vulkan-drivers). Skill: gpu.");
  ge::gpu::Target out;
  if (device) out = ge::gpu::create_target(*device, "se-sequel.final", w, h);
  shooters_edge::look::Picture look;
  int errors = 0, late = 0;
  for (const std::string& name : maps) {
    cl::MapInfo info;
    if (!adapter.map_info(adapter, files, name, info, error)) {
      lab.fail(error, "Read the reason above. Skill: brush.");
      continue;
    }
    auto map = std::make_unique<cl::LoadedMap>();
    map->name = name;
    map->from_file = true;
    std::vector<uint8_t> store;
    const std::span<const uint8_t> bytes =
        cl::find_file(files, ge::classicenhance::asset_path(content.maps, name), store);
    map->file.assign(bytes.begin(), bytes.end());
    ge::brush::Map read;
    if (!ge::brush::read_bsp(map->file, &read, &error)) {
      lab.fail(std::string(name).append(": ").append(error),
               "Read the reason above. Skill: brush.");
      continue;
    }
    std::vector<uint8_t> pic_store;
    const std::vector<ge::brush::MapPicture> pics =
        adapter.map_textures(adapter, files, name, map->file, pic_store);
    const auto found = std::count_if(pics.begin(), pics.end(),
                                     [](const ge::brush::MapPicture& p) { return p.width > 0; });
    lab.metric(name + ".textures", double(info.textures), "count");
    lab.metric(name + ".textures-found", double(found), "count");
    lab.metric(name + ".entities", double(read.entities.size()), "count");
    if (std::cmp_less(found, info.textures))
      lab.fail(name + ": " + std::to_string(info.textures - uint32_t(found)) + " of " +
                   std::to_string(info.textures) + " textures have no wall texture file",
               "Check the game's packs hold textures/<name>.wal for each. Skill: brush.");
    // The player's start: the first single-player start, else the first deathmatch spot.
    const ge::brush::Entity* start = nullptr;
    int starts = 0;
    for (const char* kind : {"info_player_start", "info_player_deathmatch"})
      for (const ge::brush::Entity& e : read.entities)
        if (e.get("classname") == kind) {
          starts++;
          if (start == nullptr) start = &e;
        }
    lab.metric(name + ".starts", starts, "count");
    sh::Vec3f at{0, 0, 0};
    float yaw = 0;
    if (start != nullptr) {
      std::sscanf(start->get("origin").c_str(), "%f %f %f", &at.x, &at.y, &at.z);
      yaw = std::strtof(start->get("angle").c_str(), nullptr);
    } else {
      lab.fail(name + " has no player start", "Check the map's entities. Skill: brush.");
    }
    at.z += kEyeHeight;
    // Played: a map with brushes is walked on the window's own server (our fixture's has faces
    // and no brushes, so it is only drawn).
    Walk walked;
    if (!read.brushes.empty()) {
      // Four tries from the spawn, a quarter turn apart; the one that went furthest is kept (a
      // spawn may face a wall).
      float best = -1;
      for (int k = 0; k < 4; k++) {
        const Walk tried = walk(map->file, name, 90.0f * float(k), error);
        if (!tried.started) break;
        const float tx = tried.to.x - tried.from.x, ty = tried.to.y - tried.from.y;
        if (tx * tx + ty * ty > best) {
          best = tx * tx + ty * ty;
          walked = tried;
        }
      }
      if (!walked.started) {
        lab.fail(std::string(name).append(" did not start: ").append(error),
                 "Read the reason above. Skill: shooter.");
      } else {
        const float dx = walked.to.x - walked.from.x, dy = walked.to.y - walked.from.y;
        const float units = std::sqrt(dx * dx + dy * dy);
        lab.metric(name + ".walk.stood", walked.stood ? 1 : 0, "bool");
        lab.metric(name + ".items", walked.items, "count");
        lab.metric(name + ".items-removed", walked.removed, "count");
        lab.metric(name + ".walk.taken", walked.taken, "count", gl::Det::measured);
        lab.shot({name + "-items",
                  (info.title.empty() ? name : info.title) + ", items from above",
                  "Every item standing on " + name +
                      " once the sequel's rules have dropped them to the floor (those in a solid "
                      "or left out of this mode removed), coloured by kind, and the walk's path "
                      "from the spawn in red.",
                  "items along the map's rooms, none floating outside them",
                  {{"map", name}, {"game", own ? "fixture" : "local packs"}}},
                 overhead(read, walked, name), gl::Det::measured);
        lab.metric(name + ".walk.units", units, "units", gl::Det::measured);
        if (!walked.stood)
          lab.fail(name + ": the player did not land on the floor at his spawn",
                   "Trace the spawn spot down through the brushes. Skill: shooter.");
      }
    }
    if (!device) {
      // No GPU adapter: a placeholder by each shot's id, in order, so the digest (which holds the
      // ids) is the same on every row.
      std::vector<std::string> ids;
      for (const char* way : kWays) ids.push_back(name + "-" + way);
      if (walked.started) ids.push_back(name + "-walked");
      for (const std::string& id : ids) {
        gl::Picture none = gl::create_picture(160, 90, {40, 40, 48});
        gl::draw_text(none, 4, 4, "NO GPU ADAPTER", {230, 230, 230});
        lab.shot({id, "NO GPU ADAPTER", "This machine has no GPU adapter.", "nothing", {}}, none,
                 gl::Det::measured);
      }
      continue;
    }
    cl::WorldDrawPtr world = cl::create_world_draw(
        *device, files, *map, w, h, wgpu::TextureFormat::RGBA8Unorm, look, {}, error, {}, adapter);
    if (!world) {
      lab.fail(std::string(name).append(": ").append(error),
               "Read the gpu: lines above. Skill: render.");
      continue;
    }
    for (int k = 0; k < 4; k++) {
      ge::render::FrameView view;
      const ge::camera::Vec3 eye = cl::engine_point(at);
      view.eye[0] = eye.x;
      view.eye[1] = eye.y;
      view.eye[2] = eye.z;
      view.heading = cl::engine_heading(yaw + 90.0f * float(k));
      view.pitch = 0;
      view.fov_y = 1.2;
      view.time_s = 1;
      ge::gpu::Image looks[2];
      for (int side = 0; side < 2; side++) {
        look.look = side == 0 ? "classic" : "modern";
        const cl::FxFrame fx;
        cl::draw_world(*device, *world, view, fx, nullptr, 0, {}, out.view);
        looks[side] = ge::gpu::read_now(*device, out);
      }
      const std::string title = (info.title.empty() ? name : info.title);
      lab.shot({name + "-" + kWays[k],
                title + ", " + kWays[k],
                "The player's start on " + name +
                    (info.title.empty() ? "" : " (" + info.title + ")") + ", looking " + kWays[k] +
                    ": the classic look on the left (the wall textures in the palette, the "
                    "lightmaps), the modern look on the right.",
                "every wall textured (no checks), the light as the map was lit, the sky where it "
                "shows",
                {{"map", name}, {"game", own ? "fixture" : "local packs"}}},
               side_by_side(looks[0], looks[1]), gl::Det::measured);
    }
    if (walked.started) {
      ge::render::FrameView view;
      const ge::camera::Vec3 eye =
          cl::engine_point({walked.to.x, walked.to.y, walked.to.z + walked.eye});
      view.eye[0] = eye.x;
      view.eye[1] = eye.y;
      view.eye[2] = eye.z;
      view.heading = cl::engine_heading(walked.facing);
      view.fov_y = 1.2;
      view.time_s = 1;
      ge::gpu::Image looks[2];
      for (int side = 0; side < 2; side++) {
        look.look = side == 0 ? "classic" : "modern";
        const cl::FxFrame fx;
        cl::draw_world(*device, *world, view, fx, nullptr, 0, {}, out.view);
        looks[side] = ge::gpu::read_now(*device, out);
      }
      lab.shot({name + "-walked",
                (info.title.empty() ? name : info.title) + ", walked",
                "The player on " + name +
                    " after two seconds walking forward from his spawn on the window's own "
                    "server (the sequel's rules on the map's brushes), seen from his eye.",
                "the view moved on from the start, at standing height, not in a wall or under "
                "the floor",
                {{"map", name}, {"game", own ? "fixture" : "local packs"}}},
               side_by_side(looks[0], looks[1]), gl::Det::measured);
    }
    world.reset();
  }
  if (device) {
    errors = static_cast<int>(device->errors.load());
    late = static_cast<int>(device->pipelines_after_load);
  } else {
    ls::placeholders(lab, {});  // each map's shots stood in above
  }
  ls::checked(lab, errors, late);
}

const gl::LabSpec kSpec{
    "se-sequel",
    gl::LabKind::picture,
    "game:shooters_edge",
    "the sequel's files on the client (its content row, its adapter, wall textures, the world)",
    "shooter",
    "Does the sequel's game folder open, and is each of its maps entered and drawn whole from the "
    "player's start?",
    {{"size", "640,360", "each look's width and height, px"},
     {"game", "",
      "the folder holding the sequel's base folder of packs (private pictures only); none: our own "
      "fixture as the sequel's files"},
     {"maps", "", "the maps to enter, comma-separated (none: every map, the start map first)"}},
    "--size=320,180",
    run,
};
GE_LAB(kSpec);

}  // namespace
