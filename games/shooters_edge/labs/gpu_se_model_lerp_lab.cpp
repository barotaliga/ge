// LAB `se-model-lerp`: DO THE MODELS MOVE SMOOTHLY? The listen server (client/listen.hpp) runs its
// frames at the server's rate while the window draws at another (`--fps`, 50 by default: a GIF's
// finest), and a classic program steps a model's frame ten times a second and a walking monster's
// place a stride at a time. Each picture is made twice off the same server, the models drawn
// between their frames (client/model_lerp.hpp: `r_lerpmodels 1`, `r_lerpmove 1`) and not (both 0),
// and the SUBJECT's drawn place and pose read off each.
//
// THE SUBJECT: with `--game=<base>/<folder>` (a game folder of packs; `--map`, e1m1 by default) and
// a single-player program among its packs, the monster of the map with the most open floor round it
// (`--monster` names its model instead, "soldier" for progs/soldier.mdl), the player stood
// `--away` units from it in the open, unhurt (no damage taken), firing once to wake it; it walks to
// him and fights, seen from his eye through a long lens (`--zoom`). Without a monster (a deathmatch
// program) or without
// `--game` (the fixture halls, baseline mod, no models: numbers only, for the smoke run), the
// player himself runs forward and is drawn as another player is, from beside.
//
// The numbers (deterministic: the server's state is exact and the pictures' clock is the lab's):
// for each way, the JUDDER (the mean change from one picture's step to the next, over the mean
// step, from its first move to its last: 0 is an even glide) of the subject's drawn place while it
// moves, and the POSE JUMPS a second (the pictures whose pose is a whole frame from the last one's:
// every frame change when not drawn between). A chart draws the drawn place along its way, both
// ways, against time.
//
// With `--game` and a GPU (PRIVATE pictures only: the pack's maps and models): a GIF of every
// picture, the two ways side by side (`se-model-lerp.gif`), and eight pictures in a row each way.
//
// ge:summary Lab: models drawn between frames on the listen server: judder and pose jumps, a GIF
// ge:keywords lab se-model-lerp model-lerp r_lerpmodels r_lerpmove judder pose-jumps monster
// ge:keywords stride step-lerp frame-lerp jittery smooth-animation gif side-by-side
// ge:skill shooter render labs
// ge:test tests/shooters_edge_model_lerp_test.cpp
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <memory>
#include <string>
#include <vector>

#include "brush/map.hpp"
#include "brush/write.hpp"
#include "core/libm/libm.hpp"
#include "core/num_text.hpp"
#include "gpu/device.hpp"
#include "gpu/target.hpp"
#include "lab/chart.hpp"
#include "lab/gif.hpp"
#include "lab/lab.hpp"
#include "progs/file.hpp"
#include "shooters_edge/app/look.hpp"
#include "shooters_edge/client/files.hpp"
#include "shooters_edge/client/listen.hpp"
#include "shooters_edge/client/model_lerp.hpp"
#include "shooters_edge/client/present.hpp"
#include "shooters_edge/client/view.hpp"
#include "shooters_edge/client/world_draw.hpp"
#include "shooters_edge/client/world_fx.hpp"
#include "shooters_edge/net/classic_game.hpp"
#include "vfs/pak.hpp"

namespace {

namespace cl = shooters_edge::client;
namespace gl = ge::lab;
namespace br = ge::brush;
namespace sh = ge::shooter;

using ge::core::num::to_fixed;

constexpr const char* kMonsters[] = {"soldier",  "dog",     "ogre",     "knight",  "demon",
                                     "zombie",   "wizard",  "enforcer", "hknight", "shalrath",
                                     "shambler", "tarbaby", "fish"};

// The fixture halls with a deathmatch spot in the hall facing east, in a pack.
bool fixture_files(cl::SearchPath& files, std::string& error) {
  br::Fixture f = br::sample_fixture();
  br::Entity spot;
  spot.pairs = {{"classname", "info_player_deathmatch"}, {"origin", "128 192 40"}, {"angle", "0"}};
  f.entities.push_back(spot);
  std::vector<uint8_t> map, pak;
  if (!br::write_bsp(f, br::Family::v29, &map, &error)) return false;
  if (!ge::vfs::write_pak({{"maps/halls.bsp", map}}, &pak, &error)) return false;
  return cl::add_archive(files, "pak0.pak", std::move(pak), error);
}

bool open_at(const br::Map& m, float x, float y, float z) {
  const float q[3] = {x, y, z};
  return br::hull_contents(m, q, 0, 0) != -2;  // not solid
}

// Solid within 96 units under the point: a floor to stand on (stairs and slopes too).
bool floor_under(const br::Map& m, float x, float y, float z) {
  for (int d = 24; d <= 96; d += 8)
    if (!open_at(m, x, y, z - static_cast<float>(d))) return true;
  return false;
}

// From `at`, the way (of eight) with the most open floor, and how far it is open (units).
float open_way(const br::Map& m, const sh::Vec3f& at, float want, float& yaw) {
  float best = -1;
  for (int k = 0; k < 8; k++) {
    const double a = k * 3.14159265358979 / 4;
    const auto dx = static_cast<float>(ge::core::libm::cos(a));
    const auto dy = static_cast<float>(ge::core::libm::sin(a));
    float open = 0;
    for (int step = 1; 16.0f * static_cast<float>(step) <= want; step++) {
      const float s = 16.0f * static_cast<float>(step);
      if (!open_at(m, at.x + dx * s, at.y + dy * s, at.z) ||
          !open_at(m, at.x + dx * s, at.y + dy * s, at.z + 24) ||
          !floor_under(m, at.x + dx * s, at.y + dy * s, at.z))
        break;
      open = s;
    }
    if (open > best) {
      best = open;
      yaw = static_cast<float>(45 * k);
    }
  }
  return best;
}

// One way's pictures read: the subject's drawn place along its way and its pose.
struct Way {
  std::vector<double> t, along;
  std::vector<bool> moving;
  int pose_jumps = 0;
  int last_frame = -1;
  float last_f = 1;
  int last_from = -1;
};

void read_subject(Way& w, double t, const cl::Placed& p, const sh::Vec3f& way_from,
                  const sh::Vec3f& way) {
  const float dx = p.origin.x - way_from.x, dy = p.origin.y - way_from.y;
  const double along = dx * way.x + dy * way.y;
  w.moving.push_back(!w.along.empty() && std::fabs(along - w.along.back()) > 1e-3);
  w.t.push_back(t);
  w.along.push_back(along);
  // A whole frame's jump: the frame shown changed with nothing drawn between, or a blend begun
  // somewhere other than where the last picture stood.
  if (w.last_frame >= 0 && p.frame != w.last_frame) {
    const bool flowed = p.frame_from == w.last_frame && p.frame_f < 0.75f;
    if (!flowed) w.pose_jumps++;
  }
  w.last_frame = p.frame;
}

// Over the pictures from the subject's first move to its last (a standing picture between two
// moves is a step of 0: a stride drawn as a stair is judder too).
double judder(const Way& w) {
  size_t first = w.along.size(), last = 0;
  for (size_t k = 1; k < w.along.size(); k++)
    if (w.moving[k]) {
      first = std::min(first, k);
      last = k;
    }
  double sum_step = 0, sum_change = 0;
  int steps = 0;
  for (size_t k = first + 1; k <= last && k < w.along.size(); k++) {
    const double a = std::fabs(w.along[k] - w.along[k - 1]);
    const double b = std::fabs(w.along[k - 1] - w.along[k - 2]);
    sum_step += a;
    sum_change += std::fabs(a - b);
    steps++;
  }
  if (steps == 0 || sum_step <= 0) return 0;
  return sum_change / sum_step;
}

gl::Picture side_by_side(const gl::Picture& a, const gl::Picture& b) {
  gl::Picture out{a.width + b.width + 4, std::max(a.height, b.height), {}};
  out.rgba.assign(size_t(out.width) * size_t(out.height) * 4, 0);
  for (size_t i = 3; i < out.rgba.size(); i += 4) out.rgba[i] = 255;
  const auto put = [&](const gl::Picture& p, int x0) {
    for (int y = 0; y < p.height; y++)
      std::copy_n(&p.rgba[size_t(y) * size_t(p.width) * 4], size_t(p.width) * 4,
                  &out.rgba[(size_t(y) * size_t(out.width) + size_t(x0)) * 4]);
  };
  put(a, 0);
  put(b, a.width + 4);
  return out;
}

void run(gl::Lab& lab) {
  const std::string folder = lab.arg("game");
  std::string error;
  cl::SearchPath files;
  if (folder.empty()) {
    if (!fixture_files(files, error)) {
      lab.fail("se-model-lerp: the fixture halls: " + error,
               "Read the reason above. Skill: brush.");
      return;
    }
  } else {
    const size_t cut = folder.find_last_of('/');
    const std::string base = cut == std::string::npos ? "." : folder.substr(0, cut);
    const std::string game = cut == std::string::npos ? folder : folder.substr(cut + 1);
    if (!cl::open_game_dir(base, game, files, error)) {
      lab.fail("se-model-lerp: " + error, "Pass --game=<base>/<game folder>. Skill: shooter.");
      return;
    }
  }
  // The pack's own program when it has one (a campaign: monsters), else the baseline mod.
  ge::progs::Progs progs;
  {
    std::vector<uint8_t> store;
    const std::span<const uint8_t> bytes =
        folder.empty() ? std::span<const uint8_t>{} : cl::find_file(files, "progs.dat", store);
    if (bytes.empty() ||
        !ge::progs::load_progs(bytes.data(), bytes.size(), progs, error, ge::progs::every_form())) {
      error.clear();
      if (!cl::baseline_progs(progs, error)) {
        lab.fail("se-model-lerp: the baseline mod: " + error,
                 "Read the reason above. Skill: progs.");
        return;
      }
    }
  }
  const std::string map_name = folder.empty() ? "halls" : lab.arg("map");
  std::unique_ptr<cl::LoadedMap> map = cl::load_named_map(files, map_name, error);
  if (!map) {
    lab.fail("se-model-lerp: " + error, "Pass --map=<a map of the folder>. Skill: shooter.");
    return;
  }
  cl::ListenOptions lo = shooters_edge::net::listen_options();
  lo.files = &files;
  if (!folder.empty()) lo.console = {"deathmatch 0", "coop 0", "skill 1"};
  cl::Listen l;
  if (!cl::listen_start(l, progs, std::move(map), lo, error)) {
    lab.fail("se-model-lerp: " + error, "Read the reason above. Skill: shooter.");
    return;
  }
  const double fps = std::max(10.0, lab.arg_num("fps"));
  const double pic = 1.0 / fps;
  for (int i = 0; i < 40; i++) cl::listen_advance(l, pic, {});  // the spawn settles

  // THE SUBJECT: the monster nearest the start, the player stood off it; else the player running.
  int subject = 0;
  const float away = static_cast<float>(lab.arg_num("away"));
  const std::string wanted = lab.arg("monster");
  {
    const sh::Vec3f start = l.server->sv.edicts[1].origin;
    float best = -1e30f;
    for (int e = 2; e < l.server->sv.num_edicts; e++) {
      const sh::Edict& ed = l.server->sv.edicts[size_t(e)];
      if (ed.free || ed.modelindex <= 0 || size_t(ed.modelindex) >= l.server->models.size())
        continue;
      const std::string& model = l.server->models[size_t(ed.modelindex)];
      bool is = false;
      for (const char* m : kMonsters)
        if ((wanted.empty() || wanted == m) && model == std::string("progs/") + m + ".mdl")
          is = true;
      if (!is) continue;
      // The one with the most open floor round it (then the nearest the start): room to walk.
      float y = 0;
      const float open = open_way(l.map->bsp.map, ed.origin, away, y);
      const float dx = ed.origin.x - start.x, dy = ed.origin.y - start.y,
                  dz = ed.origin.z - start.z;
      const float score = open * 1e6f - (dx * dx + dy * dy + dz * dz) * 1e-3f;
      if (score > best) {
        best = score;
        subject = e;
      }
    }
  }
  const bool monster = subject > 0;
  sh::Vec3f way_from = l.server->sv.edicts[1].origin, way{1, 0, 0};
  float yaw = 0;
  if (monster) {
    const sh::Vec3f at = l.server->sv.edicts[size_t(subject)].origin;
    const float open = open_way(l.map->bsp.map, at, away, yaw);
    const double a = yaw * 3.14159265358979 / 180;
    const auto dx = static_cast<float>(ge::core::libm::cos(a)),
               dy = static_cast<float>(ge::core::libm::sin(a));
    sh::Edict& me = l.server->sv.edicts[1];
    me.origin = {at.x + dx * open, at.y + dy * open, at.z};
    me.oldorigin = me.origin;
    me.velocity = {};
    l.view = {0, yaw + 180, 0};  // facing the monster
    cl::set_field_named(l, 1, "takedamage", 0);
    way_from = at;
    way = {dx, dy, 0};  // it walks toward the player: along the way out to him
    lab.note("se-model-lerp: the subject is " +
             l.server->models[size_t(l.server->sv.edicts[size_t(subject)].modelindex)] +
             " (entity " + std::to_string(subject) + "), the player " + to_fixed(open, 0) +
             " units off it");
  } else {
    l.view = {0, 0, 0};
    lab.note("se-model-lerp: no monster on the map: the player runs and is drawn as another is");
  }

  // THE RUN: the pictures at `fps`, each made both ways off the same server.
  const int pictures = std::max(10, static_cast<int>(lab.arg_num("seconds") * fps));
  std::unique_ptr<ge::gpu::Device> device;
  if (!folder.empty()) device = ge::gpu::create_device({.label = "se-model-lerp", .validation = 1});
  if (!folder.empty() && !device)
    lab.note("no GPU device: no pictures (install Mesa's Vulkan drivers; skill: gpu)");
  const auto size = lab.arg_nums("size");
  const uint32_t kW = size.size() > 0 ? static_cast<uint32_t>(size[0]) : 400;
  const uint32_t kH = size.size() > 1 ? static_cast<uint32_t>(size[1]) : 250;
  shooters_edge::look::Picture look;
  ge::console::Console con;
  shooters_edge::look::add_cvars(con, look);
  con.execute(lab.arg("cvars"));
  con.execute(lab.cvars());
  cl::WorldDrawPtr worlds[2];
  std::unique_ptr<ge::gpu::Target> outs[2];
  if (device) {
    for (int w = 0; w < 2; w++) {
      worlds[w] =
          cl::create_world_draw(*device, files, *l.map, kW, kH, wgpu::TextureFormat::RGBA8Unorm,
                                look, {}, error, {l.server->models, {}});
      if (!worlds[w]) {
        lab.fail("se-model-lerp: " + error, "Read the gpu: lines above. Skill: render.");
        return;
      }
      outs[w] = std::make_unique<ge::gpu::Target>(
          ge::gpu::create_target(*device, "se-model-lerp.final", kW, kH));
    }
  }
  cl::ViewState shown;
  cl::FxState fx_state[2];
  cl::FxFrame fx[2];
  cl::ModelLerp own[2];  // the runner's own lerps (he is not among the server's placed models)
  for (int w = 0; w < 2; w++) {
    fx_state[w].models = worlds[w] ? &cl::world_models(*worlds[w]) : nullptr;
    fx_state[w].on.lerp = {.frames = w == 1, .moves = w == 1};
  }
  Way ways[2];
  std::vector<gl::Picture> gif;
  std::vector<gl::Picture> row[2];
  sh::MoveCmd cmd;
  if (monster)
    cmd.buttons = sh::kButtonAttack;  // a shot to wake it
  else
    cmd.forward = 320;
  for (int k = 0; k < pictures; k++) {
    const double t = (k + 1) * pic;
    const int frames = cl::listen_advance(l, pic, cmd);
    if (monster) cmd.buttons = 0;
    // The player turned to keep the monster ahead.
    const sh::Vec3f me = l.server->sv.edicts[1].origin;
    if (monster && !l.server->sv.edicts[size_t(subject)].free) {
      const sh::Vec3f it = l.server->sv.edicts[size_t(subject)].origin;
      l.view.y = static_cast<float>(ge::core::libm::atan2(it.y - me.y, it.x - me.x) * 180 /
                                    3.14159265358979);
      const double h =
          std::sqrt(double(it.x - me.x) * (it.x - me.x) + double(it.y - me.y) * (it.y - me.y));
      // Pitch down positive, from the eye (22 up) to its chest (10 up).
      l.view.x = static_cast<float>(-ge::core::libm::atan2(double(it.z + 10 - (me.z + 22)), h) *
                                    180 / 3.14159265358979);
    }
    ge::render::FrameView view = cl::frame_view(l, shown, frames, t, pic);
    view.time_s = t;
    const double tic = cl::listen_tic(l);
    const cl::ModelLerpClock clock{.moves = true,
                                   .state_s = l.server->sv.time,
                                   .drawn_s = l.server->sv.time + std::min(l.owed, tic) - tic};
    for (int w = 0; w < 2; w++) {
      cl::step_fx(l, shown, fx_state[w], t, pic, fx[w]);
      const cl::Placed* p = nullptr;
      if (monster) {
        for (const cl::Placed& q : fx[w].models)
          if (q.entity == subject) p = &q;
      } else {
        // The runner as another player (entity 2), drawn between by the way's switches.
        const sh::Edict& ed = l.server->sv.edicts[1];
        cl::LerpModel m{.entity = 2,
                        .modelindex = ed.modelindex,
                        .frame = static_cast<int>(cl::field_named(l, 1, "frame")),
                        .origin = ed.origin,
                        .angles = {0, ed.angles.y, 0}};
        cl::lerp_models(own[w], std::span<cl::LerpModel>(&m, 1), t, clock, fx_state[w].on.lerp);
        cl::Placed q;
        q.entity = 2;
        q.modelindex = m.modelindex;
        q.frame = m.frame;
        q.frame_from = m.frame_from;
        q.frame_f = m.frame_f;
        q.lerp = m.lerp;
        q.origin = m.origin;
        q.angles = m.angles;
        q.top = 4;
        q.bottom = 12;
        fx[w].models.push_back(q);
        p = &fx[w].models.back();
      }
      if (p != nullptr) read_subject(ways[w], t, *p, way_from, way);
    }
    if (!device) continue;
    // The eye: the player's own on a monster; beside the runner, looking at him.
    if (!monster) {
      const sh::Vec3f eye{me.x + 64, me.y - 200, me.z + 22};
      const ge::camera::Vec3 e = cl::engine_point(eye);
      view.eye[0] = e.x, view.eye[1] = e.y, view.eye[2] = e.z;
      view.heading = cl::engine_heading(static_cast<float>(
          ge::core::libm::atan2(me.y - eye.y, me.x - eye.x) * 180.0 / 3.14159265358979));
      view.pitch = 0;
    }
    // A long lens on the monster, so its limbs fill the picture.
    if (monster) view.fov_y *= lab.arg_num("zoom") > 0 ? 1 / lab.arg_num("zoom") : 1;
    gl::Picture two[2];
    for (int w = 0; w < 2; w++) {
      cl::draw_world(*device, *worlds[w], view, fx[w], nullptr, pic, {}, outs[w]->view);
      const ge::gpu::Image img = ge::gpu::read_now(*device, *outs[w]);
      two[w] = gl::Picture{static_cast<int>(img.width), static_cast<int>(img.height), img.rgba};
    }
    gif.push_back(side_by_side(two[0], two[1]));
    const int first = pictures / 2;
    if (k >= first && k < first + 8)
      for (int w = 0; w < 2; w++) row[w].push_back(two[w]);
  }

  // THE NUMBERS.
  const double secs = pictures * pic;
  const char* names[2] = {"off", "on"};
  for (int w = 0; w < 2; w++) {
    lab.metric(std::string("judder.") + names[w], judder(ways[w]), "ratio");
    lab.metric(std::string("pose-jumps.") + names[w], ways[w].pose_jumps / secs, "per-s");
  }
  int moving = 0;
  for (const bool m : ways[1].moving) moving += int(m);
  lab.metric("pictures.moving", moving, "count");
  gl::LineChart ch;
  ch.spikes = false;
  ch.frame = {"THE SUBJECT'S DRAWN PLACE ALONG ITS WAY",
              (monster ? std::string("a monster walking, ") : std::string("the player running, ")) +
                  map_name + ", " + to_fixed(fps, 0) + " pictures a second",
              "t (s)", "along (units)"};
  ch.traces = {{"r_lerpmove 0: where each server frame puts it", ways[0].t, ways[0].along},
               {"r_lerpmove 1: drawn between", ways[1].t, ways[1].along}};
  lab.shot({"place",
            ch.frame.title,
            "Where the subject is drawn in each picture, along its way, with the models drawn "
            "between frames and without. Judder " +
                to_fixed(judder(ways[0]), 2) + " without, " + to_fixed(judder(ways[1]), 2) +
                " with (0: an even glide).",
            "the drawn line a smooth ramp; without, stairs (a monster's strides) or uneven steps "
            "(the server's frames falling unevenly on the pictures)",
            {{"map", map_name}}},
           gl::to_picture(gl::line_chart(ch).plot));
  if (!device || gif.empty()) return;
  for (int w = 0; w < 2; w++) {
    gl::Picture strip{0, 0, {}};
    for (const gl::Picture& p : row[w]) strip = strip.width == 0 ? p : side_by_side(strip, p);
    lab.shot({std::string("row-") + names[w],
              std::string("EIGHT PICTURES IN A ROW, ") + (w ? "DRAWN BETWEEN" : "AS STEPPED"),
              std::string("Eight pictures in a row (") + to_fixed(fps, 0) + " a second) " +
                  (w ? "with r_lerpmodels 1 and r_lerpmove 1." : "with both 0."),
              w ? "the limbs and the place a little further on in every picture"
                : "the same pose in two or three pictures, then a jump",
              {{"map", map_name}}},
             strip, gl::Det::measured);
  }
  lab.attach("se-model-lerp.gif",
             gl::encode_gif_frames(gif, static_cast<int>(std::lround(100 / fps))),
             gl::Det::measured);
  lab.note("se-model-lerp.gif: left as stepped, right drawn between, " +
           std::to_string(gif.size()) + " pictures");
}

const gl::LabSpec kSpec{
    "se-model-lerp",
    gl::LabKind::scenario,
    "game:shooters_edge",
    "the models drawn between their frames (client/model_lerp.hpp) on the listen server "
    "(client/listen.hpp), drawn by client/world_models.hpp",
    "shooter",
    "Do the models move smoothly? A monster walking (or the player running) on the listen server, "
    "drawn at another rate than the server's, with the models drawn between frames and without.",
    {
        {"game", "", "a game folder (<base>/<folder>); empty: the fixture halls, numbers only"},
        {"map", "e1m1", "with --game: the map"},
        {"monster", "", "with --game: the monster's model (soldier, dog, ...); empty: the nearest"},
        {"away", "320", "with a monster: units the player stands off it (at most)"},
        {"zoom", "2.5", "with a monster: the eye's lens, times the window's"},
        {"fps", "50", "pictures a second"},
        {"seconds", "3", "how long"},
        {"size", "400,250", "with --game: each picture's size, px"},
        {"cvars", "", "with --game: picture cvars (r_look ...)"},
    },
    "--seconds=1",
    run,
};
GE_LAB(kSpec);

}  // namespace
