// LAB `se-models`: THE THINGS ON A MAP AS THE WINDOW DRAWS THEM. A map under the baseline mod as
// a listen server, its items dropped to the floor, then each kind of thing it placed seen from a
// little way off in both looks, drawn as the window draws them (client/world_draw.hpp with
// client/world_models.hpp): the ARMOUR, a HEALTH box, an AMMO box, a WEAPON lying about, a
// TORCH or a FLAME (static: the server forgot them), and the view from the spawn.
//
// With `--pak` (a pack holding the map, its models and `gfx/palette.lmp`) it is the pack's own
// map (`--map`, default e1m1); without one, the fixture halls (plugins/brush) with the fixture
// model (plugins/morph) under the names the mod asks for, so the loader and the passes run on any
// machine. The models loaded by kind and the things placed by kind are deterministic metrics; the
// pictures are measured (each adapter rasterises its own way).
//
// ge:summary Lab: the window's models on a map (armour, health, ammo, weapons, flames), both looks
// ge:keywords lab se-models world-models items armour health ammo weapons flames torches statics
// ge:keywords classic modern r_look pak
// ge:skill shooter render labs
#include <algorithm>
#include <bit>
#include <cmath>
#include <memory>
#include <string>
#include <vector>

#include "brush/app/draw_data.hpp"
#include "brush/read.hpp"
#include "brush/write.hpp"
#include "console/console.hpp"
#include "core/libm/libm.hpp"
#include "gpu/device.hpp"
#include "gpu/target.hpp"
#include "lab/lab.hpp"
#include "lab/labs/liquid_scene.hpp"
#include "morph/write.hpp"
#include "shooter/trace.hpp"
#include "shooters_edge/client/listen.hpp"
#include "shooters_edge/client/present.hpp"
#include "shooters_edge/client/world_draw.hpp"
#include "shooters_edge/client/world_fx.hpp"
#include "shooters_edge/client/world_models.hpp"
#include "vfs/pak.hpp"

namespace {

namespace gl = ge::lab;
namespace cl = shooters_edge::client;
namespace sh = ge::shooter;
namespace br = ge::brush;
namespace ls = ge::lab::liquid_scene;
namespace lm = ge::core::libm;

ge::brush::Entity entity(std::initializer_list<std::pair<std::string, std::string>> pairs) {
  ge::brush::Entity e;
  for (const auto& p : pairs) e.pairs.push_back(p);
  return e;
}

// The halls with a thing of each kind on the floor, the fixture model under every name they
// ask for, and a palette of our own ramps, in a pack.
bool fixture_pack(std::vector<uint8_t>& out, std::string& error) {
  br::Fixture f = br::sample_fixture();
  f.entities.push_back(
      entity({{"classname", "info_player_deathmatch"}, {"origin", "64 192 40"}, {"angle", "0"}}));
  f.entities.push_back(entity({{"classname", "item_armor2"}, {"origin", "160 160 24"}}));
  f.entities.push_back(entity({{"classname", "weapon_rocketlauncher"}, {"origin", "200 224 24"}}));
  f.entities.push_back(
      entity({{"classname", "light_flame_large_yellow"}, {"origin", "232 160 24"}}));
  std::vector<uint8_t> bsp;
  if (!br::write_bsp(f, br::Family::v29, &bsp, &error)) return false;
  const std::vector<uint8_t> model = ge::morph::write_model(ge::morph::sample_model());
  std::vector<uint8_t> palette(768);
  for (int i = 0; i < 256; ++i) {
    const int ramp = i / 16, v = (i % 16) * 16 + 8;
    const int tint[16][3] = {{1, 1, 1}, {3, 2, 1}, {2, 2, 3}, {1, 3, 1}, {3, 1, 1}, {3, 3, 1},
                             {1, 2, 3}, {3, 2, 2}, {2, 3, 2}, {2, 1, 3}, {3, 3, 2}, {2, 2, 2},
                             {1, 1, 3}, {3, 1, 2}, {2, 3, 3}, {3, 2, 3}};
    for (int k = 0; k < 3; ++k)
      palette[size_t(i) * 3 + size_t(k)] = uint8_t(std::min(255, v * tint[ramp][k] / 3));
  }
  std::vector<std::pair<std::string, std::vector<uint8_t>>> files = {{"maps/halls.bsp", bsp},
                                                                     {"gfx/palette.lmp", palette}};
  for (const char* name : {"progs/armor.mdl", "progs/g_rock2.mdl", "progs/flame2.mdl"})
    files.push_back({name, model});
  return ge::vfs::write_pak(files, &out, &error);
}

// What a thing is, by its model's name.
const char* kind_of(const std::string& name) {
  if (name.find("armor") != std::string::npos) return "armour";
  if (name.rfind("maps/b_bh", 0) == 0) return "health";
  if (name.rfind("maps/b_", 0) == 0) return "ammo";
  if (name.rfind("progs/g_", 0) == 0) return "weapon";
  if (name.find("flame") != std::string::npos) return "flame";
  if (name.find("quaddama") != std::string::npos) return "quad";
  if (name.find("invulner") != std::string::npos) return "shield";
  if (name.find("player") != std::string::npos) return "player";
  return "other";
}

struct Subject {
  const char* id;
  const char* title;
  const char* caption;
  const char* look_for;
};
constexpr Subject kSubjects[] = {
    {"armour", "THE ARMOUR",
     "The nearest armour the map places, from a little way off and above, lit by the floor "
     "under it.",
     "the armour's vest standing on the floor, its colour the skin's"},
    {"health", "A HEALTH BOX", "A health box (an item of brush faces) on the floor.",
     "a box with its painted cross, lit as the floor round it"},
    {"ammo", "AN AMMO BOX", "An ammunition crate on the floor.", "a crate, lit as the floor"},
    {"weapon", "A WEAPON LYING ABOUT", "A weapon the map places, turning where it lies.",
     "the gun on the floor, not floating, not sunk"},
    {"flame", "A TORCH OR A FLAME",
     "A torch on its wall or a flame in its bowl: a static thing the server forgot at spawn.",
     "the flame's bright fullbright texels, the stick or the bowl lit by the wall"},
    {"quad", "THE QUAD DAMAGE",
     "The quad's pickup where the map puts it, turning, its blue glow on the floor round it.",
     "the rune turning over the floor, the floor lit blue round it"},
    {"shield", "THE SHIELD",
     "The shield's pickup (no damage taken for 30 s), turning, its red glow round it.",
     "the pentagram turning over the floor, lit red round it"},
    {"player", "ANOTHER PLAYER",
     "The player as another player sees him, his shirt in colour 4 and his pants in colour 12 "
     "(the palette's rows), lit by the floor under him.",
     "the figure standing on the floor, the shirt and the pants in two different colours"},
    {"powered", "A PLAYER WITH BOTH POWERS",
     "The same player holding the quad and the shield: the glow of each round him, blue and red, "
     "and the screen tinted as his own would be.",
     "the walls and the floor round him lit purple-pink, the figure lit by it"},
};

}  // namespace

namespace {

void run(gl::Lab& lab) {
  const auto size = lab.arg_nums("size");
  const uint32_t w = size.size() > 0 ? static_cast<uint32_t>(size[0]) : 960;
  const uint32_t h = size.size() > 1 ? static_cast<uint32_t>(size[1]) : 540;
  std::string error;
  cl::SearchPath files;
  ge::progs::Progs progs;
  const bool own = lab.arg("pak").empty();
  std::string map_name = own ? "halls" : lab.arg("map");
  if (own) {
    std::vector<uint8_t> pak;
    if (!fixture_pack(pak, error) || !cl::add_archive(files, "pak0.pak", std::move(pak), error)) {
      lab.fail(error, "Read the fixture writers' refusal above. Skill: shooter.");
      return;
    }
  } else {
    ge::brush::app::Pack pack;
    if (!ge::brush::app::read_pack(lab.arg("pak"), &pack, &error) ||
        !cl::add_archive(files, lab.arg("pak"), std::move(pack.bytes), error)) {
      lab.fail(error, "Pass --pak=<a pack with maps, progs and gfx/palette.lmp>. Skill: shooter.");
      return;
    }
  }
  if (!cl::baseline_progs(progs, error)) {
    lab.fail(error, "The baseline mod should compile. Skill: progs.");
    return;
  }
  cl::Listen l;
  std::unique_ptr<cl::LoadedMap> map = cl::load_named_map(files, map_name, error);
  if (!map || !cl::listen_start(l, progs, std::move(map), {}, error)) {
    lab.fail(error, "Check --map names a map in the pack. Skill: shooter.");
    return;
  }
  constexpr double kTic = 0.013;
  double t = 0;
  for (int i = 0; i < 120; i++) {  // the items drop to the floor; the player spawns
    cl::listen_frame(l, {});
    t += kTic;
  }
  br::Map world;
  if (!br::read_bsp(l.map->file, &world, &error)) {
    lab.fail(error, "The map read for the server should read again. Skill: brush.");
    return;
  }

  const shooters_edge::models::LightProbe probe = shooters_edge::models::make_probe(world);
  // What the server placed, by kind; the first of each kind is a subject.
  std::vector<cl::Placed> placed;
  cl::place_models(l, placed);
  constexpr size_t kPlayer = 7, kPowered = 8, kKindCount = 9;
  double placed_by[kKindCount] = {};
  const char* kKinds[kKindCount] = {"armour", "health", "ammo",   "weapon", "flame",
                                    "quad",   "shield", "player", "powered"};
  std::vector<std::vector<const cl::Placed*>> of_kind(kKindCount);
  for (const cl::Placed& p : placed) {
    const std::string kind = kind_of(l.server->models[size_t(p.modelindex)]);
    for (size_t k = 0; k < kPlayer; k++) {
      if (kind != kKinds[k]) continue;
      placed_by[k]++;
      of_kind[k].push_back(&p);
    }
  }
  // The player as another player sees him, in colours of his own (row 4 over row 12).
  cl::Placed me;
  {
    const cl::PlayerState ps = cl::player_state(l);
    const sh::Edict& ed = l.server->sv.edicts[size_t(ps.entity)];
    me.entity = ps.entity;
    me.modelindex = ed.modelindex;
    me.origin = ed.origin;
    me.angles = {0, ed.angles.y, 0};
    me.top = 4;
    me.bottom = 12;
    of_kind[kPlayer] = {&me};
    of_kind[kPowered] = {&me};
  }
  for (size_t k = 0; k < kPlayer; k++)
    lab.metric(std::string("placed.") + kKinds[k], placed_by[k], "count");
  lab.metric("placed.statics", double(l.server->statics.size()), "count");
  lab.metric("models.listed", double(l.server->models.size()), "count");

  auto device = ge::gpu::create_device({.label = "se-models", .validation = 1});
  if (!device && ls::gpu_required()) {
    lab.fail("no GPU device, and GE_GPU_REQUIRE=1",
             "Install Mesa's Vulkan drivers (apt install mesa-vulkan-drivers). Skill: gpu.");
  }
  shooters_edge::look::Picture look;
  {
    ge::console::Console con;
    shooters_edge::look::add_cvars(con, look);
    con.execute(lab.arg("cvars"));
    con.execute(lab.cvars());  // the runner's --cvars, after the lab's own (`ge lab pair`)
    for (const auto& p : con.problems()) lab.note("--cvars: " + p.what);
  }
  cl::WorldDrawPtr frame;
  ge::gpu::Target out;
  const cl::ModelLoad load{l.server->models, {}};
  if (device) {
    frame = cl::create_world_draw(*device, files, *l.map, w, h, wgpu::TextureFormat::RGBA8Unorm,
                                  look, {}, error, load);
    if (!frame) {
      lab.fail(error, "Read the gpu: lines above. Skill: render.");
      return;
    }
    out = ge::gpu::create_target(*device, "se-models.final", w, h);
  } else {
    lab.note("No GPU adapter on this machine: no pictures.");
  }

  cl::ViewState shown;
  cl::FxState fx_state;
  cl::FxFrame fx;
  cl::frame_view(l, shown, 1, t, kTic);
  cl::step_fx(l, shown, fx_state, t, kTic, fx);
  // A free spot `dist` units from `at` toward the eye, `up` units above it: the eight ways round
  // tried in turn, the first whose line to the thing crosses no wall.
  // The server's own sweep (brush models alone block), a box of `r` units each way.
  const auto open_line = [&](sh::Vec3f a, sh::Vec3f b, float r) {
    const sh::HullTrace tr =
        sh::trace_move(l.server->sv, a, {-r, -r, -r}, {r, r, r}, b, sh::kTraceNoMonsters, -1);
    return !tr.startsolid && !tr.allsolid && tr.fraction >= 1;
  };
  const auto eye_for = [&](const sh::Vec3f& at, float dist, float up, float out_eye[3]) {
    for (int k = 0; k < 8; k++) {
      const double a = k * 0.785398163397448;
      const sh::Vec3f e{at.x + float(dist * lm::cos(a)), at.y + float(dist * lm::sin(a)),
                        at.z + up};
      // The eye clear of the walls by a margin, its line to the thing's middle clear.
      if (open_line(e, e, 16) && open_line(e, {at.x, at.y, at.z + 16}, 0)) {
        out_eye[0] = e.x;
        out_eye[1] = e.y;
        out_eye[2] = e.z;
        return true;
      }
    }
    return false;
  };
  const auto shoot = [&](const std::string& id, const std::string& title,
                         const std::string& caption, const std::string& look_for,
                         ge::render::FrameView view) {
    for (const char* lk : {"classic", "modern"}) {
      const std::string shot_id = id + "-" + lk;
      if (!device) {
        ls::placeholders(lab, {shot_id.c_str()});
        continue;
      }
      look.look = lk;
      cl::draw_world(*device, *frame, view, fx, nullptr, 0, {}, out.view);
      const ge::gpu::Image img = ge::gpu::read_now(*device, out);
      lab.shot({shot_id,
                title + " (" + lk + ")",
                caption,
                look_for,
                {{"map", map_name}, {"look", lk}, {"size", lab.arg("size")}}},
               gl::Picture{static_cast<int>(img.width), static_cast<int>(img.height), img.rgba},
               gl::Det::measured);
    }
  };
  ge::render::FrameView base = cl::frame_view(l, shown, 0, t, 0);
  shoot("spawn", "FROM THE SPAWN", "The player's eyes where he spawned, looking the way he faces.",
        "whatever stands in view drawn on the floor, the HUD-less frame", base);
  for (size_t k = 0; k < kKindCount; k++) {
    const Subject& s = kSubjects[k];
    // The first of the kind with a clear view of it; a brush item's origin is its corner.
    float e[3];
    sh::Vec3f aim;
    // The best lit of the kind with a clear view of it (a lit room, not a secret's corner); a
    // brush item's origin is its corner.
    bool found = false;
    float best = -1;
    const cl::Placed* chosen = nullptr;
    for (const cl::Placed* p : of_kind[k]) {
      const bool box = l.server->models[size_t(p->modelindex)].rfind("maps/", 0) == 0;
      sh::Vec3f c = p->origin;
      if (box) c = {c.x + 16, c.y + 16, c.z};
      float ce[3], rgb[3] = {0, 0, 0};
      const bool figure = k >= kPlayer;
      if (!eye_for(c, figure ? 96.0f : 56.0f, figure ? 24.0f : 32.0f, ce)) continue;
      const float above[3] = {c.x, c.y, c.z + 8};
      shooters_edge::models::light_under(probe, world, above, rgb);
      const float lum = rgb[0] + rgb[1] + rgb[2];
      if (found && lum <= best) continue;
      found = true;
      best = lum;
      chosen = p;
      aim = c;
      for (int q = 0; q < 3; q++) e[q] = ce[q];
    }
    if (!found) {
      lab.note(std::string("no ") + s.id + " on " + map_name + " with a clear view; not shot");
      continue;
    }
    {
      const float above[3] = {aim.x, aim.y, aim.z + 8};
      float rgb[3];
      const bool hit = shooters_edge::models::light_under(probe, world, above, rgb);
      lab.metric(std::string("light.") + s.id, hit ? (rgb[0] + rgb[1] + rgb[2]) / 3 : -1,
                 "lightmap");
    }
    lab.note(std::string(s.id) + ": eye " + std::to_string(int(e[0])) + " " +
             std::to_string(int(e[1])) + " " + std::to_string(int(e[2])) + ", the thing at " +
             std::to_string(int(aim.x)) + " " + std::to_string(int(aim.y)) + " " +
             std::to_string(int(aim.z)) + " (" + l.server->models[size_t(chosen->modelindex)] +
             " frame " + std::to_string(chosen->frame) + " skin " + std::to_string(chosen->skin) +
             " entity " + std::to_string(chosen->entity) + ")");
    if (k == kPowered) {
      // Both powers handed to him and a frame run, so the mod lights him with both.
      for (const char* f : {"super_damage_finished", "invincible_finished"}) {
        const ge::progs::Def* def = ge::progs::find_field(progs, f);
        if (def != nullptr)
          *ge::progs::entity_field(l.server->q.vm, me.entity, def->offset) =
              std::bit_cast<uint32_t>(float(l.server->sv.time + 30));
      }
      cl::listen_frame(l, {});
      t += kTic;
      me.effects = int(cl::field_named(l, me.entity, "effects"));
      lab.metric("powered.effects", me.effects, "bits");
      cl::step_fx(l, shown, fx_state, t, 0, fx);
    }
    if (k >= kPlayer)
      fx.models.push_back(me);  // the window never draws its own player; the lab does
    ge::render::FrameView v = base;
    const float at_f[3] = {aim.x, aim.y, aim.z + (k >= kPlayer ? 0.0f : 12.0f)};
    double eye[3], at[3];
    br::to_engine(e, eye);
    br::to_engine(at_f, at);
    for (int c = 0; c < 3; c++) v.eye[c] = eye[c];
    const double dx = at[0] - eye[0], dy = at[1] - eye[1], dz = at[2] - eye[2];
    v.heading = lm::atan2(dx, dz);
    v.pitch = lm::atan2(dy, std::sqrt(dx * dx + dz * dz));
    shoot(s.id, s.title, s.caption, s.look_for, v);
  }
  // The map's own moving brush models: a lift where the mod lowered it at spawn and raised, a door
  // shut and open. The server's entity holding "*N" moves its faces (place_brushes).
  const auto vector_field = [&](int e, const char* name, sh::Vec3f& out_v) {
    const ge::progs::Def* def = ge::progs::find_field(progs, name);
    if (def == nullptr) return false;
    float c[3];
    for (int q = 0; q < 3; q++)
      c[q] = std::bit_cast<float>(
          *ge::progs::entity_field(l.server->q.vm, e, def->offset + uint32_t(q)));
    out_v = {c[0], c[1], c[2]};
    return true;
  };
  struct Mover {
    const char* classname;
    const char* field;  // the place it moves to for the second picture
    const char* id[2];
    const char* title[2];
    const char* caption;
    const char* look_for;
  };
  constexpr Mover kMovers[] = {
      {"func_plat",
       "pos1",
       {"lift-down", "lift-up"},
       {"A LIFT, LOWERED", "THE SAME LIFT, UP"},
       "A lift (a plat of the map's own brushes) where the mod lowers it at spawn, then at the top "
       "of its ride: its faces drawn where the server holds it.",
       "the lift's top level with the floor below it, then risen to the floor above"},
      {"func_door",
       "pos2",
       {"door-shut", "door-open"},
       {"A DOOR, SHUT", "THE SAME DOOR, OPEN"},
       "A door of the map's own brushes shut, then where the mod opens it to.",
       "the door filling its frame, then slid away showing the room behind"},
  };
  double movers_found = 0;
  for (const Mover& mv : kMovers) {
    // The first of the class with a brush model the server placed and a clear view of it.
    int ent = -1;
    sh::Vec3f c{};
    float e[3];
    for (const ge::brush::Entity& me_ent : world.entities) {
      if (me_ent.get("classname") != mv.classname) continue;
      const std::string& model = me_ent.get("model");
      if (model.empty() || model[0] != '*') continue;
      for (int q = 1; q < l.server->sv.num_edicts && ent < 0; q++) {
        const sh::Edict& ed = l.server->sv.edicts[size_t(q)];
        if (ed.free || ed.modelindex <= 0 || size_t(ed.modelindex) >= l.server->models.size() ||
            l.server->models[size_t(ed.modelindex)] != model)
          continue;
        // A door from its middle's height; a lift from above, at its top.
        const bool lift = std::string(mv.classname) == "func_plat";
        const sh::Vec3f mid{0.5f * (ed.absmin.x + ed.absmax.x), 0.5f * (ed.absmin.y + ed.absmax.y),
                            lift ? ed.absmax.z : 0.5f * (ed.absmin.z + ed.absmax.z)};
        for (int k = 0; k < 8 && ent < 0; k++) {
          const double a = k * 0.785398163397448;
          const float dx = float(lm::cos(a)), dy = float(lm::sin(a));
          const float dist = lift ? 128.0f : 160.0f, up = lift ? 96.0f : 40.0f;
          const sh::Vec3f eye{mid.x + dist * dx, mid.y + dist * dy, mid.z + up};
          const sh::Vec3f near{mid.x + 48 * dx, mid.y + 48 * dy, mid.z + (lift ? 8.0f : 0.0f)};
          if (open_line(eye, eye, 16) && open_line(eye, near, 0)) {
            ent = q;
            c = mid;
            e[0] = eye.x;
            e[1] = eye.y;
            e[2] = eye.z;
          }
        }
      }
      if (ent >= 0) break;
    }
    if (ent < 0) {
      lab.note(std::string("no ") + mv.classname + " on " + map_name + " with a clear view");
      continue;
    }
    movers_found++;
    ge::render::FrameView v = base;
    const float at_f[3] = {c.x, c.y, c.z};
    double eye[3], at[3];
    br::to_engine(e, eye);
    br::to_engine(at_f, at);
    for (int q = 0; q < 3; q++) v.eye[q] = eye[q];
    const double dx = at[0] - eye[0], dy = at[1] - eye[1], dz = at[2] - eye[2];
    v.heading = lm::atan2(dx, dz);
    v.pitch = lm::atan2(dy, std::sqrt(dx * dx + dz * dz));
    lab.note(std::string(mv.classname) + ": entity " + std::to_string(ent) + ", eye " +
             std::to_string(int(e[0])) + " " + std::to_string(int(e[1])) + " " +
             std::to_string(int(e[2])));
    for (int stage = 0; stage < 2; stage++) {
      if (stage == 1) {
        sh::Vec3f to{};
        if (!vector_field(ent, mv.field, to)) break;
        l.server->sv.edicts[size_t(ent)].origin = to;
      }
      cl::step_fx(l, shown, fx_state, t, 0, fx);
      shoot(mv.id[stage], mv.title[stage], mv.caption, mv.look_for, v);
    }
  }
  lab.metric("movers.shot", movers_found, "count");

  // The map's liquids as the window draws them: each kind's largest pool from over its top, and
  // the water from under it (the window's own view under the surface).
  {
    ge::brush::app::Pack mp;
    mp.bytes = l.map->file;
    mp.files.push_back({"maps/x.bsp", 0, static_cast<uint32_t>(l.map->file.size())});
    ge::brush::app::Scene scene;
    const std::vector<uint8_t> grey(768, 128);  // the pools' shapes alone are read
    std::string err;
    if (!ge::brush::app::load_scene(mp, mp.files[0].name, grey, &scene, &err))
      lab.note("the map's liquids not read: " + err);
    const struct {
      ge::brush::app::Kind kind;
      const char* id;
      const char* title;
    } kLiquids[] = {{ge::brush::app::kWater, "water", "THE WATER"},
                    {ge::brush::app::kSlime, "slime", "THE SLIME"},
                    {ge::brush::app::kLava, "lava", "THE LAVA"}};
    double pools = 0;
    for (const auto& lq : kLiquids) {
      const ge::brush::app::LiquidPatch* best = nullptr;
      for (const ge::brush::app::LiquidPatch& p : scene.liquids)
        if (p.kind == lq.kind && (best == nullptr || p.largest > best->largest)) best = &p;
      if (best == nullptr) {
        lab.note(std::string("no ") + lq.id + " on " + map_name);
        continue;
      }
      // Its top's middle in the file's units, then a clear eye over it looking across.
      const float top = float(best->level / br::kUnitM);
      const sh::Vec3f mid{float(best->centre[0] / br::kUnitM), float(best->centre[1] / br::kUnitM),
                          top};
      float e[3];
      bool found = false;
      for (const float dist : {96.0f, 64.0f, 32.0f}) {
        for (int k = 0; k < 8 && !found; k++) {
          const double a = k * 0.785398163397448;
          const sh::Vec3f eye{mid.x + dist * float(lm::cos(a)), mid.y + dist * float(lm::sin(a)),
                              top + 56};
          if (open_line(eye, eye, 8) && open_line(eye, {mid.x, mid.y, top + 4}, 0)) {
            e[0] = eye.x;
            e[1] = eye.y;
            e[2] = eye.z;
            found = true;
          }
        }
        if (found) break;
      }
      if (!found) {
        lab.note(std::string("no clear view over the ") + lq.id + " on " + map_name);
        continue;
      }
      pools++;
      const auto look_at = [&](const float from[3], const float to[3]) {
        ge::render::FrameView v = base;
        double eye[3], at[3];
        br::to_engine(from, eye);
        br::to_engine(to, at);
        for (int q = 0; q < 3; q++) v.eye[q] = eye[q];
        const double dx = at[0] - eye[0], dy = at[1] - eye[1], dz = at[2] - eye[2];
        v.heading = lm::atan2(dx, dz);
        v.pitch = lm::atan2(dy, std::sqrt(dx * dx + dz * dz));
        return v;
      };
      const float to[3] = {mid.x, mid.y, top};
      shoot(lq.id, lq.title,
            std::string("The map's largest pool of ") + lq.id +
                " from a little way off and over its top, as the window draws it.",
            "the surface moving and lit by the room, the walls round it in it", look_at(e, to));
      if (lq.kind != ge::brush::app::kWater) continue;
      // Under it: halfway down to the pool's floor (at most a metre), looking across it.
      const sh::HullTrace down =
          sh::trace_move(l.server->sv, {mid.x, mid.y, top - 1}, {0, 0, 0}, {0, 0, 0},
                         {mid.x, mid.y, top - 512}, sh::kTraceNoMonsters, -1);
      const float depth = (top - 1) - down.endpos.z;
      lab.metric("water.depth", depth * br::kUnitM, "m");
      const float under_eye[3] = {mid.x - 24, mid.y - 24, top - std::min(depth * 0.5f, 40.0f)};
      const float across[3] = {mid.x + 64, mid.y + 64, under_eye[2] + 8};
      shoot("underwater", "UNDER THE WATER",
            "The same pool from under its top, looking across it: the view the player has when "
            "he swims.",
            "the room seen through the surface above, the colour and the haze of the water",
            look_at(under_eye, across));
    }
    lab.metric("liquids.shot", pools, "count");
  }
  if (device)
    ls::checked(lab, static_cast<int>(device->errors.load()),
                static_cast<int>(device->pipelines_after_load));
  else
    ls::checked(lab, 0, 0);
}

const gl::LabSpec kSpec{
    "se-models",
    gl::LabKind::picture,
    "game:shooters_edge",
    "games/shooters_edge/client/world_models (the packs' models, sprites and items in the world)",
    "shooter",
    "Do the items, weapons, armour, health and ammo boxes and the torches' flames a map places "
    "stand where the server put them, lit as the floor under them, in both looks?",
    {{"size", "960,540", "the frame's width and height, px"},
     {"pak", "", "a pack holding the map, its models and gfx/palette.lmp (empty: the fixture)"},
     {"map", "e1m1", "the pack's map to stand in (with --pak)"},
     {"cvars", "", "picture cvars run before the frames (r_look is set per shot)"}},
    "--size=480,270",
    run,
};
GE_LAB(kSpec);

}  // namespace
