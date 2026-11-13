// LAB `se-sequel-models`: THE SEQUEL'S THINGS AS THE WINDOW DRAWS THEM. Its models are the second
// triangle-model family (plugins/morph read.hpp, magic IDP2) with their skins named as picture
// files of the game's, and its sprites name a picture a frame (IDS2); the sequel's adapter opens
// both (client/adapter_parts.cpp `open_model`) into the forms the window's world draws.
//
// Without `--game` it is our own: a model and a sprite written by the fixture writers in that
// family, with their pictures, in a pack, opened through the sequel's adapter, their counts the
// lab's deterministic metrics (CI needs no game data). With `--game=<folder>` (the folder holding
// the sequel's base folder) a map of its (`--map`, default base1) is played on the window's own
// listen server as a single-player game; every model the server lists is opened (the counts by
// kind measured), and each kind of thing it placed (a weapon, armour, health, ammo, another item,
// a monster, the player as another sees him) is shot from a little way off in both looks: PRIVATE
// pictures only, never committed.
//
// ge:summary Lab: the sequel's models and sprites opened and drawn on its map, in both looks
// ge:keywords lab se-sequel-models sequel md2 sp2 idp2 ids2 second-model-family skin-pictures
// ge:keywords items weapons armour health ammo monsters player world-models classic modern
// ge:skill morph shooter labs
#include <cmath>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "assets/pcx.hpp"
#include "brush/read.hpp"
#include "classicenhance/profile.hpp"
#include "core/libm/libm.hpp"
#include "gpu/device.hpp"
#include "gpu/target.hpp"
#include "lab/lab.hpp"
#include "lab/labs/liquid_scene.hpp"
#include "morph/write.hpp"
#include "shooter/trace.hpp"
#include "shooters_edge/client/files.hpp"
#include "shooters_edge/client/game_adapter.hpp"
#include "shooters_edge/client/listen.hpp"
#include "shooters_edge/client/present.hpp"
#include "shooters_edge/client/world_draw.hpp"
#include "shooters_edge/client/world_fx.hpp"
#include "shooters_edge/client/world_models.hpp"
#include "shooters_edge/net/classic_game.hpp"
#include "shooters_edge/profiles/game_profile.hpp"
#include "shooters_edge/server/sequel_server.hpp"
#include "vfs/pak.hpp"

namespace {

namespace gl = ge::lab;
namespace cl = shooters_edge::client;
namespace sh = ge::shooter;
namespace br = ge::brush;
namespace ls = ge::lab::liquid_scene;
namespace lm = ge::core::libm;
namespace pf = shooters_edge::profiles;
namespace mo = ge::morph;

constexpr const char* kModel = "models/items/armor/body/tris.md2";
constexpr const char* kSprite = "sprites/s_sample.sp2";

// The fixture: the morph plugin's sample model and a two-frame sprite in the second family, each
// picture a PCX of its own colours.
bool fixture_pack(std::vector<uint8_t>& out, std::string& error) {
  std::vector<std::pair<std::string, std::vector<uint8_t>>> files;
  mo::Model m = mo::sample_model();
  m.skins.clear();
  m.skin_paths = {"models/items/armor/body/skin.pcx"};
  m.skin_width = 16;
  m.skin_height = 8;
  files.emplace_back(kModel, mo::write_model2(m));
  const auto picture = [](int w, int h, uint8_t base) {
    ge::assets::IndexedImage img;
    img.width = w;
    img.height = h;
    img.indices.resize(size_t(w) * size_t(h));
    for (size_t i = 0; i < img.indices.size(); i++) img.indices[i] = uint8_t(base + i % 16);
    return ge::assets::encode_pcx(img);
  };
  files.emplace_back(m.skin_paths[0], picture(16, 8, 32));
  mo::Sprite s;
  for (int i = 0; i < 2; i++) {
    mo::SpriteFrame f;
    f.width = 8;
    f.height = 8;
    f.origin_x = -4;
    f.origin_y = 4;
    f.picture = "sprites/s_sample_" + std::to_string(i) + ".pcx";
    files.emplace_back(f.picture, picture(8, 8, uint8_t(64 + 32 * i)));
    s.frames.push_back(f);
  }
  files.emplace_back(kSprite, mo::write_sprite2(s));
  return ge::vfs::write_pak(files, &out, &error);
}

// What a model of the sequel's is, by where its file lives.
const char* kind_of(const std::string& name) {
  if (name.rfind("models/weapons/g_", 0) == 0) return "weapon";
  if (name.rfind("models/items/armor/", 0) == 0) return "armour";
  if (name.rfind("models/items/healing/", 0) == 0) return "health";
  if (name.rfind("models/items/ammo/", 0) == 0) return "ammo";
  if (name.rfind("models/items/", 0) == 0) return "item";
  if (name.rfind("models/monsters/", 0) == 0) return "monster";
  if (name.rfind("players/", 0) == 0) return "player";
  if (name.rfind("sprites/", 0) == 0) return "sprite";
  if (name.rfind("models/", 0) == 0) return "other";
  return "map";
}

struct Subject {
  const char* id;
  const char* title;
  const char* look_for;
};
constexpr Subject kSubjects[] = {
    {"weapon", "A WEAPON LYING ABOUT", "the gun on the floor in its own skin's colours"},
    {"armour", "THE ARMOUR", "the armour standing on the floor, its colours the skin's"},
    {"health", "HEALTH", "the health on the floor, whole, in its skin's colours"},
    {"ammo", "AMMO", "the ammo box on the floor, its label the skin's"},
    {"item", "ANOTHER ITEM", "the item whole, standing on the floor"},
    {"monster", "A MONSTER", "the monster standing on its feet, its skin's colours, no torn faces"},
    {"player", "THE PLAYER", "a player standing on the floor in his skin, as another sees him"},
};
constexpr size_t kSubjectCount = sizeof(kSubjects) / sizeof(kSubjects[0]);

void opened_counts(gl::Lab& lab, const cl::GameAdapter& adapter, const cl::SearchPath& files,
                   const std::vector<std::string>& names, gl::Det det) {
  int opened = 0, refused = 0, missing = 0, verts = 0, frames = 0, skins = 0;
  for (const std::string& name : names) {
    const std::string kind = kind_of(name);
    if (kind == "map" || name.empty() || name[0] == '*') continue;
    cl::OpenedModel m;
    std::string error;
    if (!adapter.open_model(adapter, files, name, m, error)) {
      (error.empty() ? missing : refused)++;
      if (!error.empty()) lab.note(std::string(name).append(" refused: ").append(error));
      continue;
    }
    opened++;
    if (m.form == cl::ModelForm::morph) {
      verts += int(m.morph.vertex_count());
      frames += int(m.morph.frames.size());
      skins += int(m.morph.skins.size());
    } else if (m.form == cl::ModelForm::sprite) {
      frames += int(m.sprite.frames.size());
    }
  }
  lab.metric("models.opened", opened, "count", det);
  lab.metric("models.refused", refused, "count", det);
  lab.metric("models.missing", missing, "count", det);
  lab.metric("models.vertices", verts, "count", det);
  lab.metric("models.frames", frames, "count", det);
  lab.metric("models.skins", skins, "count", det);
  if (refused > 0)
    lab.fail(std::to_string(refused) + " models refused (the notes name them)",
             "Read each refusal; the reader is plugins/morph/read.cpp. Skill: morph.");
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
  if (lab.arg("game").empty()) {
    std::vector<uint8_t> pak;
    if (!fixture_pack(pak, error) || !cl::add_archive(files, "pak0.pak", std::move(pak), error)) {
      lab.fail(error, "The fixture's files should write and mount. Skill: morph.");
      return;
    }
    opened_counts(lab, adapter, files, {kModel, kSprite}, gl::Det::deterministic);
    lab.note("No --game: the fixture's model and sprite opened, no map to stand them on.");
    ls::checked(lab, 0, 0);
    return;
  }
  if (!cl::open_game_dir(lab.arg("game"), std::string(content.base_dir), files, error, content)) {
    lab.fail(error, "Pass --game=<the folder holding " + std::string(content.base_dir) +
                        "/pak0.pak>. Skill: shooter.");
    return;
  }
  const std::string map_name = lab.arg("map");
  static const ge::progs::Progs progs = [] {
    ge::progs::Progs p;
    std::string why;
    cl::baseline_progs(p, why);
    return p;
  }();
  cl::ListenOptions opt = shooters_edge::net::listen_options();
  opt.console = {"deathmatch 0"};  // the single-player game: its monsters stand
  cl::Listen l;
  std::unique_ptr<cl::LoadedMap> map = cl::load_named_map(files, map_name, error);
  if (!map || !cl::listen_start(l, progs, std::move(map), opt, error)) {
    lab.fail(error, "Check --map names a map in the game's packs. Skill: shooter.");
    return;
  }
  constexpr double kTic = 0.013;
  double t = 0;
  for (int i = 0; i < 120; i++) {  // the items drop to the floor; the player spawns
    cl::listen_frame(l, {});
    t += kTic;
  }
  const std::vector<std::string>& names = l.server->models;
  lab.metric("models.listed", double(names.size()), "count", gl::Det::measured);
  opened_counts(lab, adapter, files, names, gl::Det::measured);

  std::vector<cl::Placed> placed;
  cl::place_models(l, placed);
  std::vector<std::vector<const cl::Placed*>> of_kind(kSubjectCount);
  for (const cl::Placed& p : placed) {
    const std::string kind = kind_of(names[size_t(p.modelindex)]);
    for (size_t k = 0; k < kSubjectCount; k++)
      if (kind == kSubjects[k].id) of_kind[k].push_back(&p);
  }
  // The player as another sees him (the window never draws its own).
  cl::Placed me;
  {
    const cl::PlayerState ps = cl::player_state(l);
    const sh::Edict& ed = l.server->sv.edicts[size_t(ps.entity)];
    me.entity = ps.entity;
    // The sequel's player model by its name (the server draws the window's own player with none).
    for (size_t i = 0; i < names.size(); i++)
      if (names[i] == shooters_edge::server::kSequelPlayerModel) me.modelindex = int(i);
    me.origin = ed.origin;
    me.angles = {0, ed.angles.y, 0};
    of_kind[kSubjectCount - 1] = {&me};
  }
  for (size_t k = 0; k + 1 < kSubjectCount; k++)
    lab.metric(std::string("placed.") + kSubjects[k].id, double(of_kind[k].size()), "count",
               gl::Det::measured);

  auto device = ge::gpu::create_device({.label = "se-sequel-models", .validation = 1});
  if (!device && ls::gpu_required())
    lab.fail("no GPU device, and GE_GPU_REQUIRE=1",
             "Install Mesa's Vulkan drivers (apt install mesa-vulkan-drivers). Skill: gpu.");
  if (!device) {
    lab.note("No GPU adapter on this machine: no pictures.");
    ls::checked(lab, 0, 0);
    return;
  }
  shooters_edge::look::Picture look;
  const cl::ModelLoad load{names, {}};
  cl::WorldDrawPtr frame =
      cl::create_world_draw(*device, files, *l.map, w, h, wgpu::TextureFormat::RGBA8Unorm, look, {},
                            error, load, adapter);
  if (!frame) {
    lab.fail(error, "Read the gpu: lines above. Skill: render.");
    return;
  }
  ge::gpu::Target out = ge::gpu::create_target(*device, "se-sequel-models.final", w, h);
  cl::ViewState shown;
  cl::FxState fx_state;
  cl::FxFrame fx;
  cl::frame_view(l, shown, 1, t, kTic);
  cl::step_fx(l, shown, fx_state, t + 1.0, 0, fx);  // past the spawn's own effects
  const ge::render::FrameView base = cl::frame_view(l, shown, 0, t, 0);
  const auto open_line = [&](sh::Vec3f a, sh::Vec3f b, float r) {
    const sh::HullTrace tr =
        sh::trace_move(l.server->sv, a, {-r, -r, -r}, {r, r, r}, b, sh::kTraceNoMonsters, -1);
    return !tr.startsolid && !tr.allsolid && tr.fraction >= 1;
  };
  // A free spot `dist` units from `at` and `up` above it, the eight ways round tried in turn.
  const auto eye_for = [&](const sh::Vec3f& at, float dist, float up, float out_eye[3]) {
    for (int k = 0; k < 8; k++) {
      const double a = k * 0.785398163397448;
      const sh::Vec3f e{at.x + float(dist * lm::cos(a)), at.y + float(dist * lm::sin(a)),
                        at.z + up};
      if (open_line(e, e, 16) && open_line(e, {at.x, at.y, at.z + 16}, 0)) {
        out_eye[0] = e.x;
        out_eye[1] = e.y;
        out_eye[2] = e.z;
        return true;
      }
    }
    return false;
  };
  int shots = 0;
  for (size_t k = 0; k < kSubjectCount; k++) {
    const Subject& s = kSubjects[k];
    const bool figure = std::string(s.id) == "monster" || std::string(s.id) == "player";
    float e[3];
    const cl::Placed* chosen = nullptr;
    for (const cl::Placed* p : of_kind[k])
      if (eye_for(p->origin, figure ? 96.0f : 56.0f, figure ? 24.0f : 32.0f, e)) {
        chosen = p;
        break;
      }
    if (chosen == nullptr) {
      lab.note(std::string("no ") + s.id + " on " + map_name + " with a clear view; not shot");
      continue;
    }
    lab.note(std::string(s.id) + ": " + names[size_t(chosen->modelindex)] + " frame " +
             std::to_string(chosen->frame) + " skin " + std::to_string(chosen->skin));
    if (chosen == &me) fx.models.push_back(me);
    ge::render::FrameView v = base;
    const float at_f[3] = {chosen->origin.x, chosen->origin.y,
                           chosen->origin.z + (figure ? 0.0f : 8.0f)};
    double eye[3], at[3];
    br::to_engine(e, eye);
    br::to_engine(at_f, at);
    for (int c = 0; c < 3; c++) v.eye[c] = eye[c];
    const double dx = at[0] - eye[0], dy = at[1] - eye[1], dz = at[2] - eye[2];
    v.heading = lm::atan2(dx, dz);
    v.pitch = lm::atan2(dy, std::sqrt(dx * dx + dz * dz));
    for (const char* lk : {"classic", "modern"}) {
      look.look = lk;
      cl::draw_world(*device, *frame, v, fx, nullptr, 0, {}, out.view);
      const ge::gpu::Image img = ge::gpu::read_now(*device, out);
      lab.shot({std::string(s.id) + "-" + lk,
                std::string(s.title) + " (" + lk + ")",
                std::string("The first ") + s.id + " " + map_name +
                    " places with a clear view, from a little way off, drawn as the window "
                    "draws it: " +
                    names[size_t(chosen->modelindex)] + ".",
                s.look_for,
                {{"map", map_name}, {"look", lk}, {"size", lab.arg("size")}}},
               gl::Picture{static_cast<int>(img.width), static_cast<int>(img.height), img.rgba},
               gl::Det::measured);
      shots++;
    }
    if (chosen == &me) fx.models.pop_back();
  }
  lab.metric("shots", shots, "count", gl::Det::measured);
  ls::checked(lab, static_cast<int>(device->errors.load()),
              static_cast<int>(device->pipelines_after_load));
  cl::listen_stop(l);
}

const gl::LabSpec kSpec{
    "se-sequel-models",
    gl::LabKind::picture,
    "game:shooters_edge",
    "plugins/morph's second model family and client/adapter_parts.cpp's open_model (the sequel's "
    "models and sprites in the world)",
    "morph",
    "Do the sequel's models and sprites open with their skin pictures, and do its weapons, "
    "armour, health, ammo, monsters and players stand on its map, in both looks?",
    {{"size", "640,360", "the frame's width and height, px"},
     {"game", "", "the folder holding the sequel's base folder (empty: the fixture, no pictures)"},
     {"map", "base1", "the map to stand in (with --game)"}},
    "",
    run,
};
GE_LAB(kSpec);

}  // namespace
