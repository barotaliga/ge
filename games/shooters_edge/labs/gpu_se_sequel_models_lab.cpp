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
// a monster, the player as another sees him) is shot from a little way off in both looks. Then the
// poses (server/sequel_poses.hpp): a strip of a monster's runs and one of the player's sequences,
// each at the frame the rules set for it, and a strip of the armour turning where it lies; with
// `--looks` the player again in each look named, as another who chose it is seen: PRIVATE
// pictures only, never committed.
//
// ge:summary Lab: the sequel's models and sprites opened and drawn on its map, in both looks
// ge:keywords lab se-sequel-models sequel md2 sp2 idp2 ids2 second-model-family skin-pictures
// ge:keywords items weapons armour health ammo monsters player world-models classic modern
// ge:keywords poses pose-strip turning-items animation-frames looks player-looks
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
#include "shooters_edge/server/sequel_map.hpp"
#include "shooters_edge/server/sequel_monster_kinds.hpp"
#include "shooters_edge/server/sequel_poses.hpp"
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
    {"player", "THE PLAYER",
     "a player standing on the floor in his skin, the weapon he carries in his hands, as another "
     "sees him"},
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

namespace sv = shooters_edge::server;

// A frame's middle, or its last for a held one.
int mid(sv::SqSpan s) { return (s.first + s.last) / 2; }

// One pose of a strip: its caption word and the frame shown.
struct Pose {
  std::string word;
  int frame;
};

// The poses a strip shows of a monster kind: each of its runs, the death's last (held).
std::vector<Pose> monster_poses(const sv::SqMonsterPoses& t) {
  std::vector<Pose> out;
  if (!t.stand.none()) out.push_back({"stand", mid(t.stand)});
  if (!t.walk.none()) out.push_back({"walk", mid(t.walk)});
  if (!t.run.none()) out.push_back({"run", mid(t.run)});
  for (int a = 0; a < 5; a++)
    if (!t.attack[a][0].none()) {
      out.push_back({a < 2 ? "blow" : "attack", mid(t.attack[a][0])});
      break;
    }
  if (!t.pain[0].none()) out.push_back({"pain", mid(t.pain[0])});
  if (!t.death[0].none()) out.push_back({"dead", t.death[0].last});
  return out;
}

std::vector<Pose> player_poses() {
  namespace f = sv::sq_frames;
  return {{"stand", mid(f::kStand)},
          {"run", mid(f::kRun)},
          {"attack", mid(f::kAttack)},
          {"pain", mid(f::kPain[0])},
          {"jump", f::kJump.first + 1},
          {"crouch", mid(f::kCrouchStand)},
          {"crouch walk", mid(f::kCrouchWalk)},
          {"dead", f::kDeath[0].last}};
}

// Tiles of `w` x `h` laid side by side, each halved.
gl::Picture strip(const std::vector<ge::gpu::Image>& tiles) {
  if (tiles.empty()) return {};
  const int tw = int(tiles[0].width) / 2, th = int(tiles[0].height) / 2;
  gl::Picture p{tw * int(tiles.size()), th,
                std::vector<uint8_t>(size_t(tw) * tiles.size() * th * 4)};
  for (size_t k = 0; k < tiles.size(); k++)
    for (int y = 0; y < th; y++)
      for (int x = 0; x < tw; x++)
        for (int c = 0; c < 4; c++) {
          const ge::gpu::Image& im = tiles[k];
          int sum = 0;
          for (int dy = 0; dy < 2; dy++)
            for (int dx = 0; dx < 2; dx++)
              sum += im.rgba[(size_t(2 * y + dy) * im.width + size_t(2 * x + dx)) * 4 + size_t(c)];
          p.rgba[(size_t(y) * size_t(p.width) + size_t(int(k) * tw + x)) * 4 + size_t(c)] =
              uint8_t(sum / 4);
        }
  return p;
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
    me.carried = l.server->world.entities[size_t(ps.entity)].carried_model;
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
  // The player as another sees him: his body and the weapon he carries, riding it.
  const auto push_me = [&] {
    fx.models.push_back(me);
    if (me.carried <= 0) return;
    cl::Placed gun = me;
    gun.entity = me.entity + cl::kCarriedEntity;
    gun.modelindex = me.carried;
    gun.carried = 0;
    fx.models.push_back(gun);
  };
  const auto pop_me = [&] {
    fx.models.pop_back();
    if (me.carried > 0) fx.models.pop_back();
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
    if (chosen == &me) push_me();
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
    if (chosen == &me) pop_me();
  }
  // `--looks`: the player again in each look ("<model>/<skin>", ';' between), as another player
  // who chose it sees them: their own figure, its skin and the weapon from its folder.
  const std::string looks = lab.arg("looks");
  float look_eye[3];
  if (!looks.empty() && eye_for(me.origin, 72.0f, 16.0f, look_eye)) {
    look.look = "classic";
    int n = 0;
    for (size_t a = 0; a < looks.size(); n++) {
      const size_t b = std::min(looks.find(';', a), looks.size());
      me.skin_name = looks.substr(a, b - a);
      a = b + 1;
      push_me();
      ge::render::FrameView v = base;
      const float at_f[3] = {me.origin.x, me.origin.y, me.origin.z};
      double eye[3], at[3];
      br::to_engine(look_eye, eye);
      br::to_engine(at_f, at);
      for (int c = 0; c < 3; c++) v.eye[c] = eye[c];
      const double dx = at[0] - eye[0], dy = at[1] - eye[1], dz = at[2] - eye[2];
      v.heading = lm::atan2(dx, dz);
      v.pitch = lm::atan2(dy, std::sqrt(dx * dx + dz * dz));
      cl::draw_world(*device, *frame, v, fx, nullptr, 0, {}, out.view);
      const ge::gpu::Image img = ge::gpu::read_now(*device, out);
      lab.shot({"look-" + std::to_string(n),
                "The player as " + me.skin_name,
                "The player drawn as another who chose the look " + me.skin_name + " is seen.",
                "the figure and skin the look names, else the default figure's",
                {{"map", map_name}, {"look", me.skin_name}, {"size", lab.arg("size")}}},
               gl::Picture{static_cast<int>(img.width), static_cast<int>(img.height), img.rgba},
               gl::Det::measured);
      pop_me();
    }
    me.skin_name.clear();
  }
  // THE POSES: a monster's runs and the player's sequences, frame by frame as the rules set them,
  // and an item turning where it lies, a strip each (classic look).
  look.look = "classic";
  const auto view_of = [&](const cl::Placed& p, const float e[3], bool figure) {
    ge::render::FrameView v = base;
    const float at_f[3] = {p.origin.x, p.origin.y, p.origin.z + (figure ? 0.0f : 8.0f)};
    double eye[3], at[3];
    br::to_engine(e, eye);
    br::to_engine(at_f, at);
    for (int c = 0; c < 3; c++) v.eye[c] = eye[c];
    const double dx = at[0] - eye[0], dy = at[1] - eye[1], dz = at[2] - eye[2];
    v.heading = lm::atan2(dx, dz);
    v.pitch = lm::atan2(dy, std::sqrt(dx * dx + dz * dz));
    return v;
  };
  const auto film = [&](size_t k, const cl::Placed* chosen, const std::vector<Pose>& poses,
                        const std::string& name, const std::string& id) {
    float e[3];
    if (chosen == nullptr || !eye_for(chosen->origin, 96.0f, 24.0f, e)) return;
    const ge::render::FrameView v = view_of(*chosen, e, true);
    const bool mine = chosen == &me;
    if (mine) push_me();
    cl::Placed* drawn = nullptr;
    for (cl::Placed& p : fx.models)
      if (p.entity == chosen->entity) drawn = &p;
    if (drawn == nullptr) {
      if (mine) pop_me();
      return;
    }
    cl::Placed* held = nullptr;  // the weapon it carries, on the same frame
    for (cl::Placed& p : fx.models)
      if (p.entity == chosen->entity + cl::kCarriedEntity) held = &p;
    const cl::Placed kept = *drawn;
    std::vector<ge::gpu::Image> tiles;
    std::string words;
    const int held_model = held != nullptr ? held->modelindex : 0;
    for (const Pose& pose : poses) {
      if (held != nullptr) {  // the dead carry nothing (the weapon's frames end before death's)
        held->frame = pose.frame;
        held->modelindex = pose.word == "dead" ? 0 : held_model;
      }
      drawn->frame = pose.frame;
      drawn->frame_from = -1;
      drawn->frame_f = 1;
      drawn->lerp = false;
      cl::draw_world(*device, *frame, v, fx, nullptr, 0, {}, out.view);
      tiles.push_back(ge::gpu::read_now(*device, out));
      words.append(words.empty() ? "" : ", ")
          .append(pose.word)
          .append(" ")
          .append(std::to_string(pose.frame));
    }
    *drawn = kept;
    if (held != nullptr) held->modelindex = held_model;
    if (mine) pop_me();
    lab.shot({id,
              std::string(kSubjects[k].title) + ": ITS POSES",
              "The " + std::string(kSubjects[k].id) + " " + name +
                  " drawn in each of its poses as "
                  "the rules set them, left to right: " +
                  words + ".",
              "each pose a different stance, the frame the word says; the dead one lying down",
              {{"map", map_name}, {"look", "classic"}, {"size", lab.arg("size")}}},
             strip(tiles), gl::Det::measured);
    lab.metric("poses." + std::string(kSubjects[k].id), double(poses.size()), "count",
               gl::Det::measured);
    shots++;
  };
  const sv::SequelServer* sq = l.rules != nullptr ? sv::sequel_of(*l.rules) : nullptr;
  if (sq != nullptr) {
    const size_t mk = 5, pk = kSubjectCount - 1;  // the monster, the player
    for (const cl::Placed* p : of_kind[mk]) {
      const sv::SqMonsterPoses* kind_poses = nullptr;
      for (const sv::SequelMonster& m : sq->monsters)
        if (sv::map_row(m.entity) == p->entity)
          kind_poses = sv::sq_monster_poses(sv::roster::kKinds[m.kind].classname);
      float e[3];
      if (kind_poses == nullptr || !eye_for(p->origin, 96.0f, 24.0f, e)) continue;
      film(mk, p, monster_poses(*kind_poses), kind_poses->classname, "monster-poses");
      break;
    }
    film(pk, &me, player_poses(), shooters_edge::server::kSequelPlayerModel, "player-poses");
    // An item that turns, a quarter turn (0.9 s) a tile.
    for (const cl::Placed* p : of_kind[1]) {  // the armour
      float e[3];
      if (!p->turns || !eye_for(p->origin, 56.0f, 32.0f, e)) continue;
      ge::render::FrameView v = view_of(*p, e, false);
      std::vector<ge::gpu::Image> tiles;
      for (int q = 0; q < 4; q++) {
        v.time_s = base.time_s + 0.9 * q;
        cl::draw_world(*device, *frame, v, fx, nullptr, 0, {}, out.view);
        tiles.push_back(ge::gpu::read_now(*device, out));
      }
      lab.shot({"armour-turns",
                "THE ARMOUR TURNS",
                "The armour " + names[size_t(p->modelindex)] +
                    " where it lies, 0.9 s apart left to right: it turns 100 degrees a second "
                    "about its upright.",
                "a quarter turn more in each tile, standing upright, never tipping",
                {{"map", map_name}, {"look", "classic"}, {"size", lab.arg("size")}}},
               strip(tiles), gl::Det::measured);
      shots++;
      break;
    }
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
     {"map", "base1", "the map to stand in (with --game)"},
     {"looks", "", "the player again in each look (\"<model>/<skin>\", ';' between; with --game)"}},
    "",
    run,
};
GE_LAB(kSpec);

}  // namespace
