// LAB `se-remix-q2-hand`: QUAKE II'S WEAPONS AS QUAKE II HAS THEM ON QUAKE'S RULES (classic remix's
// `cr_items q2`, plugins/classic/q2_arms.hpp). Three things a player of Quake II knows by eye:
//
// - **The idle run rests** on its pause frames (`kQ2ArmPauses`): the rocket launcher's frame over
//   20 s of idling by the server's draws, against the run played round without a rest (before).
//   The mean time round the idle run of each weapon, before and now.
// - **A shot kicks the view** (`kQ2Kicks`, the window's `client/q2_arms_kick.hpp`): the view's
//   pitch and the eye's move back over two rocket shots and a chaingun burst, as the window reads
//   the shots off the hand's frames; before, no kick at all.
// - **A pickup stands and turns** as Quake II's own: the classic rules on a hall laying Quake II's
//   super shotgun, jacket armour, medium health and shells, drawn from the side with the floor: the
//   model's origin and Quake II's own box about it (-15..15), before (the weapon and the armour on
//   the floor, half sunk) and now (15 units over it), and the turn's yaw over time.
//
// ge:summary Lab: Quake II's weapons on Quake's rules: idle rests, the view kick, pickups standing
// ge:keywords lab se-remix-q2-hand idle-pause view-kick kick-angles pickups item-lift item-turn
// ge:keywords cr_items q2-arms remix
// ge:skill shooter labs
// ge:test tests/shooters_edge_remix_q2_arms_test.cpp
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <memory>
#include <string>
#include <vector>

#include "brush/write.hpp"
#include "classic/q2_arms.hpp"
#include "lab/chart.hpp"
#include "lab/lab.hpp"
#include "lab/plot.hpp"
#include "lab/raster.hpp"
#include "shooter/hull.hpp"
#include "shooters_edge/client/listen.hpp"
#include "shooters_edge/client/q2_arms_kick.hpp"
#include "shooters_edge/client/remix.hpp"
#include "shooters_edge/net/qw_game.hpp"
#include "shooters_edge/profiles/arsenal.hpp"
#include "shooters_edge/profiles/remix_arsenal.hpp"
#include "shooters_edge/server/q2_items.hpp"
#include "shooters_edge/server/qc_server.hpp"
#include "vfs/pak.hpp"

namespace {

namespace gl = ge::lab;
namespace gc = ge::classic;
namespace br = ge::brush;
namespace cl = shooters_edge::client;
namespace pf = shooters_edge::profiles;
namespace sv = shooters_edge::server;
namespace sh = ge::shooter;

const gl::Rgba kPaper{246, 244, 238, 255};
const gl::Rgba kInk{30, 30, 34, 255};
const gl::Rgba kFaint{120, 120, 120, 255};
const gl::Rgba kFloor{150, 140, 120, 255};
const gl::Rgba kBefore{40, 96, 150, 255};
const gl::Rgba kNow{150, 82, 40, 255};

// THE IDLE RUN: weapon `id`'s frame each step of `steps` from its first idle frame, resting by the
// server's draws (`rests`) or never (before); and the mean steps round the run.
std::vector<int> idle_frames(gc::Q2ArmId id, int steps, bool rests, double* round_steps) {
  const gc::Q2Arm& a = gc::q2_arm(id);
  const gc::Q2ArmPauses& p = gc::kQ2ArmPauses[static_cast<int>(id)];
  int frame = a.frames.last_fire + 1, wraps = 0;
  std::vector<int> out;
  for (int t = 0; t < steps; t++) {
    out.push_back(frame);
    const int was = frame;
    gc::q2_idle_step(a.frames, p, frame, rests ? gc::q2_pause_draw(uint32_t(t), 1) : 0);
    if (frame < was) wraps++;
  }
  if (round_steps != nullptr) *round_steps = wraps > 0 ? double(steps) / wraps : double(steps);
  return out;
}

// THE KICK as the window draws it: the hand stepped each tenth (attack held over `shots`' tenths),
// its frame read 60 times a second.
struct KickTrace {
  std::vector<double> t, pitch, back;
};
void kick_trace(gc::Q2ArmId id, std::string_view model, double from, double to,
                const std::vector<std::pair<double, double>>& held, KickTrace& out,
                cl::ArmsKick& k) {
  const gc::Q2Arm& a = gc::q2_arm(id);
  int frame = a.frames.last_fire + 1, held_shots = 0;
  bool firing = false;
  const float ahead[3] = {1, 0, 0};
  double next = from;
  const int draws = static_cast<int>((to - from) * 60);
  for (int i = 0; i < draws; i++) {
    const double t = from + i / 60.0;
    while (t + 1e-9 >= next) {  // the hand's step
      bool attack = false;
      for (const auto& [a0, a1] : held) attack = attack || (next >= a0 && next < a1);
      if (!firing && attack) {
        firing = true;
        frame = a.frames.last_activate + 1;
        held_shots = 0;
      }
      if (firing) {
        const gc::Q2Firing f =
            gc::q2_firing_step(a.frames, a.held, frame, held_shots, attack, 1 << 20, attack);
        if (f.done) firing = false;
      } else {
        gc::q2_idle_step(a.frames, gc::kQ2NoPauses, frame, 0);
      }
      next += 0.1;
    }
    const gc::Q2Kick kick = cl::arms_kick_step(k, model, frame, t, ahead, true, 1);
    out.t.push_back(t);
    out.pitch.push_back(0.0 - kick.angles[0]);  // up positive, as it looks
    out.back.push_back(-kick.origin[0]);
  }
}

// THE PICKUPS: a hall of the classic rules wearing Quake II's things (its weapons' lift on or, for
// before, off), and each pickup's origin and box over the floor (z 0).
std::vector<uint8_t> hall() {
  br::BrushWorld world;
  world.models.resize(1);
  const auto box = [&](float x0, float y0, float z0, float x1, float y1, float z1) {
    const float lo[3] = {x0, y0, z0}, hi[3] = {x1, y1, z1};
    world.models[0].push_back(br::box_brush(lo, hi, sh::kSqSolid, "world/wall"));
  };
  box(-512, -512, -16, 512, 512, 0);
  box(-528, -512, -16, -512, 512, 256);
  box(512, -512, -16, 528, 512, 256);
  box(-528, 512, -16, 528, 528, 256);
  box(-528, -528, -16, 528, -512, 256);
  const auto entity = [](std::initializer_list<std::pair<std::string, std::string>> pairs) {
    br::Entity e;
    for (const auto& p : pairs) e.pairs.push_back(p);
    return e;
  };
  world.entities.push_back(
      entity({{"classname", "info_player_deathmatch"}, {"origin", "-400 -400 24"}}));
  world.entities.push_back(entity({{"classname", "weapon_supershotgun"}, {"origin", "-120 0 16"}}));
  world.entities.push_back(entity({{"classname", "item_armor_jacket"}, {"origin", "-40 0 16"}}));
  world.entities.push_back(entity({{"classname", "item_health"}, {"origin", "40 0 16"}}));
  world.entities.push_back(entity({{"classname", "ammo_shells"}, {"origin", "120 0 16"}}));
  std::vector<uint8_t> bsp;
  std::string error;
  br::write_brush_world(world, &bsp, &error);
  return bsp;
}

struct Pickup {
  std::string model;
  float origin_z = 0, lo_z = 0, hi_z = 0;
};

bool lay_pickups(bool lifted, std::vector<Pickup>& out, std::string& why) {
  const pf::RemixArsenal* worn = pf::remix_arsenal("q1", "q2");
  if (worn == nullptr) {
    why = "no remix row for Quake's rules wearing Quake II's things";
    return false;
  }
  std::vector<pf::WeaponRow> weapons(worn->arsenal.weapons.begin(), worn->arsenal.weapons.end());
  std::vector<pf::ArmourRow> armour(worn->arsenal.armour.begin(), worn->arsenal.armour.end());
  if (!lifted) {
    for (pf::WeaponRow& w : weapons) w.lift = 0;
    for (pf::ArmourRow& a : armour) a.lift = 0;
  }
  pf::Arsenal arsenal = worn->arsenal;
  arsenal.weapons = weapons;
  arsenal.armour = armour;
  static const ge::progs::Progs progs = [] {
    ge::progs::Progs p;
    std::string w;
    cl::baseline_progs(p, w);
    return p;
  }();
  cl::SearchPath mine;
  cl::RemixFiles remix;
  remix.played = "q1";
  remix.state.on = true;
  remix.state.other = "q2";
  std::vector<uint8_t> pak;
  if (!ge::vfs::write_pak({{"maps/hall.bsp", hall()}}, &pak, &why)) return false;
  if (!cl::add_archive(remix.files, "pak0.pak", pak, why)) return false;
  std::unique_ptr<cl::LoadedMap> map = cl::load_remixed_map(mine, "hall", &remix, why);
  if (!map) return false;
  cl::ListenOptions opt = shooters_edge::net::listen_options();
  opt.arsenal = pf::arsenal_lines(arsenal);
  cl::Listen l;
  if (!cl::listen_start(l, progs, std::move(map), opt, why)) return false;
  for (int e = 1; e < l.server->sv.num_edicts; e++) {
    const sh::Edict& ed = l.server->sv.edicts[size_t(e)];
    const std::string model(cl::string_field_named(l, e, "model"));
    if (ed.free || !model.ends_with(".md2")) continue;
    out.push_back({model, ed.origin.z, ed.origin.z + ed.mins.z, ed.origin.z + ed.maxs.z});
  }
  return !out.empty();
}

std::string short_name(const std::string& model) {
  const size_t a = model.find("models/") == 0 ? 7 : 0;
  const size_t b = model.rfind("/tris.md2");
  return model.substr(a, b == std::string::npos ? std::string::npos : b - a);
}

void run(gl::Lab& lab) {
  // 1. The idle run.
  constexpr int kSteps = 200;  // 20 s
  double before_round = 0, now_round = 0;
  const std::vector<int> before = idle_frames(gc::Q2ArmId::rocket_launcher, kSteps, false, nullptr);
  const std::vector<int> now = idle_frames(gc::Q2ArmId::rocket_launcher, kSteps, true, nullptr);
  gl::LineChart idle;
  idle.frame = {"The rocket launcher's idle frames over 20 s", "the server's draws, slot 1",
                "t (s)", "frame"};
  idle.spikes = false;
  gl::Trace tb{"before: round and round, no rest", {}, {}},
      tn{"now: resting on 25, 33, 42", {}, {}};
  for (int i = 0; i < kSteps; i++) {
    tb.x.push_back(i * 0.1);
    tb.y.push_back(before[size_t(i)]);
    tn.x.push_back(i * 0.1);
    tn.y.push_back(now[size_t(i)]);
  }
  idle.traces = {tb, tn};
  lab.shot({"idle-rests",
            "THE IDLE RUN RESTS ON ITS PAUSE FRAMES",
            "The rocket launcher's idle frames over 20 s of the hand at rest: before, the run "
            "played round without a stop (once in 3.8 s); now, it rests on its pause frames, each "
            "rest 1.6 s on average, as Quake II's hand does.",
            "the now line holds flat on frames 25, 33 and 42 for a second or more",
            {}},
           gl::to_picture(gl::line_chart(idle).plot));
  std::vector<std::vector<std::string>> idle_table;
  const char* names[] = {
      "blaster",          "shotgun",         "super shotgun", "machinegun", "chaingun",
      "grenade launcher", "rocket launcher", "hyperblaster",  "railgun",    "BFG",
      "hand grenade"};
  for (int a = 0; a < gc::kQ2ArmCount; a++) {
    idle_frames(static_cast<gc::Q2ArmId>(a), 4000, false, &before_round);
    idle_frames(static_cast<gc::Q2ArmId>(a), 4000, true, &now_round);
    char b[32], n[32];
    std::snprintf(b, sizeof b, "%.1f", before_round / 10);
    std::snprintf(n, sizeof n, "%.1f", now_round / 10);
    idle_table.push_back({names[a], b, n});
    if (a == static_cast<int>(gc::Q2ArmId::rocket_launcher)) {
      lab.metric("rocket-idle-round-before", before_round / 10, "s");
      lab.metric("rocket-idle-round-now", now_round / 10, "s");
    }
  }
  lab.table({"time round the idle run (s)", {"weapon", "before", "now"}, idle_table});

  // 2. The kick.
  KickTrace rocket, chain;
  cl::ArmsKick kr, kc;
  kick_trace(gc::Q2ArmId::rocket_launcher, "models/weapons/v_rocket/tris.md2", 0, 2.4,
             {{0.4, 0.5}, {1.4, 1.5}}, rocket, kr);
  kick_trace(gc::Q2ArmId::chaingun, "models/weapons/v_chain/tris.md2", 0, 2.4, {{0.4, 1.6}}, chain,
             kc);
  gl::LineChart kick;
  kick.frame = {"The view's kick from Quake II's weapons on Quake's rules",
                "the window's reading of the hand's frames, drawn 60 times a second", "t (s)",
                "pitch up (deg) / eye back (units)"};
  kick.spikes = false;
  kick.traces = {{"before: no kick", rocket.t, std::vector<double>(rocket.t.size(), 0)},
                 {"now: rocket launcher, pitch up", rocket.t, rocket.pitch},
                 {"now: rocket launcher, eye back", rocket.t, rocket.back},
                 {"now: chaingun burst, pitch", chain.t, chain.pitch}};
  lab.shot({"view-kick",
            "A SHOT KICKS THE VIEW AS IN QUAKE II",
            "The view over two rocket shots and a chaingun burst held 1.2 s: before, Quake II's "
            "weapons on Quake's rules moved the view not at all; now each rocket tips it 1 degree "
            "up and the eye 2 units back, in over a tenth and out over the next, and the chaingun "
            "shakes it by up to half a degree each tenth.",
            "a 0.2 s triangle of 1 degree and 2 units at each rocket shot; the chaingun's jitter",
            {}},
           gl::to_picture(gl::line_chart(kick).plot));
  double most_pitch = 0, most_back = 0;
  for (size_t i = 0; i < rocket.t.size(); i++) {
    most_pitch = std::max(most_pitch, rocket.pitch[i]);
    most_back = std::max(most_back, rocket.back[i]);
  }
  lab.metric("rocket-kick-pitch", most_pitch, "deg");
  lab.metric("rocket-kick-back", most_back, "units");
  std::vector<std::vector<std::string>> kicks;
  for (int a = 0; a < gc::kQ2ArmCount; a++) {
    const gc::Q2KickRow& r = gc::kQ2Kicks[a];
    const char* kind = r.kind == gc::Q2KickKind::straight ? "up and back"
                       : r.kind == gc::Q2KickKind::shake  ? "shake 0.7 deg, 0.35 units"
                       : r.kind == gc::Q2KickKind::climb  ? "shake, pitch the climb"
                       : r.kind == gc::Q2KickKind::heave  ? "eye back, hurt kick -40"
                                                          : "none";
    char up[16], back[16];
    std::snprintf(up, sizeof up, "%g", 0.0 - r.pitch);
    std::snprintf(back, sizeof back, "%g", r.back);
    kicks.push_back({names[a], kind, up, back});
  }
  lab.table({"each weapon's kick", {"weapon", "kind", "up (deg)", "back (units)"}, kicks});

  // 3. The pickups.
  std::vector<Pickup> was, is;
  std::string why;
  if (!lay_pickups(false, was, why) || !lay_pickups(true, is, why)) {
    lab.fail("the hall of Quake II's pickups on the classic rules did not start: " + why,
             "Run tests/shooters_edge_remix_bodies_test.cpp; read client/remix.hpp. Skill: "
             "shooter.");
    return;
  }
  constexpr int kW = 1000, kH = 300, kFloorY = 210, kScale = 2;
  gl::Picture pic = gl::create_picture(kW, kH, kPaper);
  gl::draw_text(pic, 10, 14,
                "SIDE VIEW: the floor, the model's origin (+) and Quake II's own box "
                "about it (-15..15)",
                kInk);
  gl::draw_text(pic, 10, 30, "BEFORE", kBefore);
  gl::draw_text(pic, 70, 30, "NOW", kNow);
  gl::fill_rect(pic, 0, kFloorY, kW, kH - kFloorY, {226, 220, 206, 255});
  gl::draw_line(pic, 0, kFloorY, kW, kFloorY, kFloor);
  std::vector<std::vector<std::string>> table;
  const size_t n = std::min(was.size(), is.size());
  for (size_t i = 0; i < n; i++) {
    const int cx = 80 + static_cast<int>(i) * (kW - 120) / static_cast<int>(n);
    for (int side = 0; side < 2; side++) {
      const Pickup& p = side == 0 ? was[i] : is[i];
      const gl::Rgba c = side == 0 ? kBefore : kNow;
      const int x = cx + side * 90;
      const int oy = kFloorY - static_cast<int>(p.origin_z * kScale);
      const int l = x - 15 * kScale, r = x + 15 * kScale, t = oy - 15 * kScale,
                b = oy + 15 * kScale;
      gl::draw_line(pic, l, t, r, t, c);
      gl::draw_line(pic, l, b, r, b, c);
      gl::draw_line(pic, l, t, l, b, c);
      gl::draw_line(pic, r, t, r, b, c);
      gl::draw_line(pic, x - 6, oy, x + 6, oy, c);
      gl::draw_line(pic, x, oy - 6, x, oy + 6, c);
    }
    gl::draw_text(pic, cx - 30, kFloorY + 42, short_name(is[i].model), kInk);
    char b[64], w[64];
    std::snprintf(b, sizeof b, "%.0f up, still", was[i].origin_z);
    const bool turns = [&] {
      for (int item = 0; item < sv::kSqItemCount; item++)
        if (is[i].model == sv::kSqItems[item].model) return sv::sq_turns(item);
      return false;
    }();
    std::snprintf(w, sizeof w, "%.0f up, %s", is[i].origin_z, turns ? "turning" : "still");
    gl::draw_text(pic, cx - 30, kFloorY + 58, b, kBefore);
    gl::draw_text(pic, cx - 30, kFloorY + 74, w, kNow);
    table.push_back({short_name(is[i].model), b, w});
  }
  lab.shot({"pickups",
            "QUAKE II'S PICKUPS STAND AND TURN AS ITS OWN",
            "Quake II's super shotgun, jacket armour, medium health and shells laid by Quake's "
            "rules on a hall, seen from the side: before, the weapon and the armour had their "
            "origin on the floor, so Quake II's model (measured from its middle) sank half into "
            "it and lay still; now every pickup stands 15 units up as in Quake II, and the weapon "
            "and the armour turn 100 degrees a second as Quake II's do (its ammunition and health "
            "do not turn there either).",
            "every NOW cross 15 units over the floor, its box resting on it",
            {}},
           pic);
  lab.table({"pickups", {"pickup", "before", "now"}, table});
  int lifted = 0;
  for (const Pickup& p : is) lifted += p.origin_z == 15 ? 1 : 0;
  lab.metric("pickups-standing", lifted, "of " + std::to_string(is.size()));
}

const gl::LabSpec kSpec{
    "se-remix-q2-hand",
    gl::LabKind::sim,
    "game:shooters_edge",
    "plugins/classic/q2_arms, client/q2_arms_kick: Quake II's hand, kick and pickups on Quake's "
    "rules",
    "shooter",
    "Do Quake II's weapons on Quake's rules rest on their idle pause frames, kick the view as "
    "Quake II's do, and lie 15 units up turning as its own pickups, where they idled round, "
    "kicked nothing and lay half sunk before?",
    {},
    "",
    run,
};
GE_LAB(kSpec);

}  // namespace
