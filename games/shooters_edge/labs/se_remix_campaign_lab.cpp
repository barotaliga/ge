// LAB `se-remix-campaign`: does `cr_monsters` reach the next map of a CAMPAIGN, and does that map
// play on? A player starts Quake's campaign on the start map with classic remix on, sets
// `cr_monsters` to Quake II's during the map (as at the console), and walks through the start map's
// skill gate: the level change the map itself makes, not one the console asks for. The next map's
// monsters are counted by game, then the player rides its first lift down (the button touched,
// the player stood on the lift) and the map runs on with its monsters awake. Any exception the
// rules or the window's listen throw is the lab's failure, with its text.
//
// `--take=0` is the window before the fix: the campaign's own change of map read the next map
// without the cr_ settings changed during the last one, so Quake's monsters stood in it.
//
// `--fire=<rank>` gives him that weapon of Quake's (7 the rocket launcher) and from `--fire-at` he
// fires it at the nearest of Quake II's monsters hunting him in his sight: their dodges, ducks and
// shots on the run are counted. The side steps they take round what blocks their way are counted
// either way.
//
// Needs the player's own installs (`--game=<folder>/id1`, `--remix=<folder holding baseq2>`); with
// none (the smoke run) it notes that and passes.
//
// ge:summary Lab: cr_monsters through a campaign's own level change, then the first lift ridden
// ge:keywords lab se-remix-campaign cr_monsters remix campaign changelevel skill-gate lift
// ge:keywords before_map level-change monsters-swapped exception crash side-step
// ge:skill shooter labs
// ge:test tests/shooters_edge_remix_test.cpp
#include <algorithm>
#include <cstdio>
#include <exception>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "campaign_movers.hpp"
#include "core/libm/libm.hpp"
#include "lab/lab.hpp"
#include "shooter/trace.hpp"
#include "shooters_edge/client/listen.hpp"
#include "shooters_edge/client/present.hpp"
#include "shooters_edge/client/remix.hpp"
#include "shooters_edge/client/world_fx.hpp"
#include "shooters_edge/net/qw_game.hpp"
#include "shooters_edge/profiles/remix_monsters.hpp"
#include "shooters_edge/server/baseline_mod.hpp"
#include "shooters_edge/server/campaign.hpp"
#include "shooters_edge/server/qc_server.hpp"

namespace {

namespace gl = ge::lab;
namespace sh = ge::shooter;
namespace sv = shooters_edge::server;
namespace cl = shooters_edge::client;
namespace pf = shooters_edge::profiles;
namespace mv = shooters_edge::labs::mv;

// The window's part: the cr_ settings changed during the map taken before the next is read
// (main.cpp's `remix_sync`), the listen's files the remix's while it reads the other game's.
struct Window {
  cl::Listen* l = nullptr;
  cl::RemixFiles* remix = nullptr;
  cl::SearchPath* own = nullptr;
  std::string monsters;  // what `cr_monsters` says now
  int taken = 0;
};

void take_settings(void* ctx) {
  Window& w = *static_cast<Window*>(ctx);
  w.remix->monsters = w.monsters;
  w.l->opt.files = w.remix->from_files() ? &w.remix->files : w.own;
  w.taken++;
}

// The campaign's rules (the window's own listen runs them as a QC server).
sv::QcServer* rules_of(cl::Listen& l) {
  return l.campaign_host && l.campaign_host->server ? sv::qc_of(l.campaign_host->server) : nullptr;
}

// The monsters standing, by class name.
std::map<std::string, int> monsters(cl::Listen& l, const mv::Fields& f) {
  std::map<std::string, int> out;
  sv::QcServer* s = rules_of(l);
  if (s == nullptr) return out;
  for (int e = 1; e < s->sv.num_edicts; e++) {
    if (s->sv.edicts[size_t(e)].free) continue;
    const std::string cn = mv::string_at(*s, e, f.classname);
    if (pf::is_matched_monster(cn)) out[cn]++;
  }
  return out;
}

// The first entity of class `cn` (targeting `target` when given), or -1.
int find(sv::QcServer& s, const mv::Fields& f, int target_ofs, const std::string& cn,
         const std::string& target) {
  for (int e = 1; e < s.sv.num_edicts; e++) {
    if (s.sv.edicts[size_t(e)].free || mv::string_at(s, e, f.classname) != cn) continue;
    if (target.empty() || mv::string_at(s, e, target_ofs) == target) return e;
  }
  return -1;
}

// The points of a walk ("x,y,z;x,y,z").
std::vector<sh::Vec3f> points_of(const std::string& text) {
  std::vector<sh::Vec3f> out;
  size_t at = 0;
  while (at < text.size()) {
    const size_t end = std::min(text.find(';', at), text.size());
    sh::Vec3f p{};
    if (std::sscanf(text.substr(at, end - at).c_str(), "%f,%f,%f", &p.x, &p.y, &p.z) == 3)
      out.push_back(p);
    at = end + 1;
  }
  return out;
}

// Where a walk through `pts` is after `dist` units (its last point once it is over).
sh::Vec3f along(const std::vector<sh::Vec3f>& pts, float dist) {
  for (size_t i = 1; i < pts.size(); i++) {
    const float leg = sh::length(pts[i] - pts[i - 1]);
    if (dist <= leg) return pts[i - 1] + (pts[i] - pts[i - 1]) * (leg > 0 ? dist / leg : 0.0f);
    dist -= leg;
  }
  return pts.back();
}

void stand(sv::QcServer& s, sh::Vec3f at) {
  sh::Edict& me = s.sv.edicts[1];
  me.origin = {at.x, at.y, at.z};
  me.velocity = {0, 0, 0};
  sh::link_edict(s.sv, 1, true);
}

// On the lift beside its middle (a monster may stand in the middle), toward its button.
sh::Vec3f top_of(const sh::Edict& ed) {
  return {ed.absmin.x + 24.0f, (ed.absmin.y + ed.absmax.y) * 0.5f, ed.absmax.z + 25.0f};
}

// The window's frame: the server's, then the eyes and the effects drawn from it.
struct Eyes {
  cl::ViewState shown;
  cl::FxState fx = cl::fx_state_for(nullptr);
  cl::FxFrame out;
  double t = 0;
};
void frame(cl::Listen& l, Eyes& e, sh::MoveCmd cmd = {}) {
  cl::listen_frame(l, cmd);  // the view and an impulse are the listen's own (`l.view`, `l.impulse`)
  e.t += 1.0 / 72;
  if (l.server == nullptr) return;
  cl::frame_view(l, e.shown, 1, e.t, 1.0 / 72);
  cl::step_fx(l, e.shown, e.fx, e.t, 1.0 / 72, e.out);
}

void run(gl::Lab& lab) {
  const std::string game = lab.arg("game"), other = lab.arg("remix");
  if (game.empty() || other.empty()) {
    lab.note("no installs given (--game, --remix): nothing played; pass the player's own");
    lab.metric("remix_campaign.played", 0, "");
    return;
  }
  std::string error;
  cl::SearchPath files;
  const size_t cut = game.find_last_of('/');
  const std::string base = cut == std::string::npos ? "." : game.substr(0, cut);
  const std::string dir = cut == std::string::npos ? game : game.substr(cut + 1);
  if (!cl::open_game_dir(base, dir, files, error)) {
    lab.fail("se-remix-campaign: " + error, "next: pass --game=<folder>/id1");
    return;
  }
  cl::RemixState state;
  state.on = true;
  state.other = lab.arg("remix-game");
  state.other_basedir = other;
  cl::RemixFiles remix;
  if (!cl::open_remix(remix, state, "q1", base, dir, error)) {
    lab.fail("se-remix-campaign: " + error, "next: pass --remix=<folder holding baseq2>");
    return;
  }
  ge::progs::Progs progs;
  if (!sv::own_mod("campaign", progs, error)) {
    lab.fail("se-remix-campaign: " + error, "next: run tests/shooters_edge_campaign_rules_test");
    return;
  }
  cl::Listen l;
  Window w{&l, &remix, &files, "", 0};
  cl::ListenOptions lo = shooters_edge::net::listen_options();
  lo.files = &files;
  lo.remix = &remix;
  if (lab.arg("take") == "1") {
    lo.before_map = [&w] { take_settings(&w); };
  }
  lo.console.emplace_back("deathmatch 0");
  lo.console.emplace_back("skill " + lab.arg("skill"));
  gl::LabTable t{"the campaign with cr_monsters set during the start map", {"step", "what"}, {}};
  std::string thrown;
  Eyes eyes;
  int q2 = 0, q1 = 0, lift_dz = 0, frames_run = 0, deaths = 0, hits = 0, hunting = 0, trailing = 0,
      lost = 0, side_steps = 0, heard = 0, dodges = 0, ducking = 0, run_shots = 0, fired = 0;
  float lowest_hp = 100, first_hit = -1, nearest = -1;
  try {
    std::unique_ptr<cl::LoadedMap> map = cl::load_remixed_map(files, lab.arg("map"), &remix, error);
    if (!map || !cl::listen_start(l, progs, std::move(map), lo, error)) {
      lab.fail("se-remix-campaign: " + error, "next: read the reason above");
      return;
    }
    for (int i = 0; i < 20; i++) frame(l, eyes);
    t.rows.push_back({"started", l.map->name});
    // cr_monsters set at the console during the start map: taken from the next map.
    w.monsters = lab.arg("monsters");
    t.rows.push_back({"cr_monsters", w.monsters + ", set during " + l.map->name});
    // The map's own level change (its skill gate's changelevel).
    if (!cl::listen_campaign_word(l, "changelevel " + lab.arg("then"))) {
      lab.fail("se-remix-campaign: changelevel was refused", "next: read listen.cpp's words");
      return;
    }
    for (int i = 0; i < 40; i++) frame(l, eyes);
    t.rows.push_back({"next map", l.map->name + " (the settings taken " + std::to_string(w.taken) +
                                      " time(s) before it was read)"});
    sv::QcServer* s = rules_of(l);
    if (s == nullptr) {
      lab.fail("se-remix-campaign: no campaign plays after the change", "next: read the prints");
      return;
    }
    mv::Fields f;
    f.classname = mv::field_of(progs, "classname");
    const int target = mv::field_of(progs, "target");
    const int targetname = mv::field_of(progs, "targetname");
    for (const auto& [cn, n] : monsters(l, f)) {
      const bool theirs = pf::monster_as(cn, "q2") == cn;
      (theirs ? q2 : q1) += n;
      t.rows.push_back(
          {theirs ? "Quake II's monster" : "Quake's monster", cn + " x" + std::to_string(n)});
    }
    // The first lift: the button that moves it, touched; the player stood on it as it goes down.
    const int button = find(*s, f, target, "func_button", lab.arg("button"));
    int lift = -1;
    for (int e = 1; button > 0 && e < s->sv.num_edicts; e++)
      if (!s->sv.edicts[size_t(e)].free &&
          mv::string_at(*s, e, targetname) == mv::string_at(*s, button, target))
        lift = e;
    if (lift < 0) {
      t.rows.push_back({"lift", "no button targeting " + lab.arg("button") + " on this map"});
    } else {
      const float z0 = s->sv.edicts[size_t(lift)].origin.z;
      stand(*s, top_of(s->sv.edicts[size_t(lift)]));
      for (int i = 0; i < 5; i++) frame(l, eyes);
      // The button pressed by the player, as his touch does.
      f.touch = mv::field_of(progs, "touch");
      f.self = mv::global_of(progs, "self");
      f.other = mv::global_of(progs, "other");
      f.activator = mv::global_of(progs, "activator");
      mv::call_as(*s, f, mv::word_at(*s, button, f.touch), button);
      const int frames = static_cast<int>(lab.arg_num("seconds") / sv::kCampaignStep);
      float lowest = z0;
      const float me0 = s->sv.edicts[1].origin.z;
      float me_low = me0;
      bool alive = true;
      float last_hp = s->sv.edicts[1].health;
      const std::vector<sh::Vec3f> walk = points_of(lab.arg("walk"));
      const auto walk_after = static_cast<float>(lab.arg_num("walk-after"));
      const auto shot_at = static_cast<float>(lab.arg_num("shot-at"));
      const auto fire = static_cast<int>(lab.arg_num("fire"));
      const auto fire_at = static_cast<float>(lab.arg_num("fire-at"));
      if (fire > 0) {  // the weapon given, loaded
        const int items = mv::field_of(progs, "items");
        auto* it = ge::progs::entity_field(s->q.vm, 1, items);
        const float bit = fire == 7 ? 32.0f : fire == 5 ? 8.0f : 4.0f;  // its item: rockets, nails
        *it = ge::progs::as_word(ge::progs::as_float(*it) + bit);
        for (const char* ammo : {"ammo_rockets", "ammo_nails"})
          *ge::progs::entity_field(s->q.vm, 1, mv::field_of(progs, ammo)) =
              ge::progs::as_word(100.0f);
      }
      std::vector<int> was_ducking, was_running, was_stepping;
      for (int i = 0; i < frames && rules_of(l) != nullptr; i++, frames_run++) {
        sh::MoveCmd cmd;
        const float t0 = float(i) * float(sv::kCampaignStep);
        if (fire > 0 && alive && t0 >= fire_at) {  // at the nearest of Quake II's hunting him
          if (t0 < fire_at + 0.05f) l.impulse = fire;
          const int enemy = mv::field_of(progs, "enemy");
          const sh::Vec3f eye = s->sv.edicts[1].origin + sh::Vec3f{0, 0, 22};
          float best = -1;
          sh::Vec3f aim{};
          for (int e = 2; e < s->sv.num_edicts; e++) {
            const sh::Edict& ed = s->sv.edicts[size_t(e)];
            if (ed.free || ed.health <= 0 || mv::word_at(*s, e, enemy) == 0) continue;
            const std::string cn = mv::string_at(*s, e, f.classname);
            if (!pf::is_matched_monster(cn) || pf::monster_as(cn, "q2") != cn) continue;
            const sh::Vec3f d = ed.origin - eye;
            const sh::HullTrace line = sh::trace_move(s->sv, eye, {0, 0, 0}, {0, 0, 0}, ed.origin,
                                                      sh::kTraceNoMonsters, 1);
            if (line.fraction < 1) continue;  // out of his sight
            if (best < 0 || sh::length(d) < best) {
              best = sh::length(d);
              aim = d;
            }
          }
          if (best > 0) {
            namespace lm = ge::core::libm;
            const double flat = sh::length(sh::Vec3f{aim.x, aim.y, 0});
            l.view.y = float(lm::atan2(aim.y, aim.x) * 180 / 3.14159265358979);
            l.view.x = float(-lm::atan2(aim.z, flat) * 180 / 3.14159265358979);
            cmd.buttons = sh::kButtonAttack;
            fired++;
          }
        }
        frame(l, eyes, cmd);
        s = rules_of(l);
        const float now = float(i + 1) * float(sv::kCampaignStep);
        if (s != nullptr && alive && !walk.empty() && now >= walk_after)  // he walks away
          stand(*s, along(walk, (now - walk_after) * 300.0f));
        if (s != nullptr && lift < s->sv.num_edicts)
          lowest = std::min(lowest, s->sv.edicts[size_t(lift)].origin.z);
        if (s != nullptr) me_low = std::min(me_low, s->sv.edicts[1].origin.z);
        if (s == nullptr) continue;
        const float hp = s->sv.edicts[1].health;
        if (alive && hp < last_hp) {  // a blow or a shot landed on him this frame
          hits++;
          if (first_hit < 0) first_hit = float(i + 1) * float(sv::kCampaignStep);
        }
        last_hp = hp;
        if (hp <= 0 && alive) deaths++;
        alive = hp > 0;
        if (alive) lowest_hp = std::min(lowest_hp, hp);
        // A shot's noise, as firing makes it (the player's `show_hostile` a second on).
        if (shot_at >= 0 && now >= shot_at && now < shot_at + float(sv::kCampaignStep) * 1.5f) {
          const int hostile = mv::field_of(progs, "show_hostile");
          const int time = mv::global_of(progs, "time");
          if (hostile >= 0 && time >= 0)
            *ge::progs::entity_field(s->q.vm, 1, hostile) =
                ge::progs::as_word(ge::progs::as_float(s->q.vm.globals[size_t(time)]) + 1.0f);
        }
        // Quake II's monsters that lost him, that follow his trail and that go to a noise, this
        // frame (the fields are theirs alone).
        const auto count = [&](const char* field) {
          const int ofs = mv::field_of(progs, field);
          int n = 0;
          for (int e = 2; ofs >= 0 && e < s->sv.num_edicts; e++)
            if (!s->sv.edicts[size_t(e)].free && s->sv.edicts[size_t(e)].health > 0 &&
                mv::word_at(*s, e, ofs) != 0)
              n++;
          return n;
        };
        // Dodges begun and shots on the run begun this frame, and the most ducking at once.
        const auto begun = [&](const char* field, std::vector<int>& was) {
          const int ofs = mv::field_of(progs, field);
          was.resize(size_t(s->sv.num_edicts), 0);
          int n = 0, now_in = 0;
          for (int e = 2; ofs >= 0 && e < s->sv.num_edicts; e++) {
            const bool in = !s->sv.edicts[size_t(e)].free && mv::word_at(*s, e, ofs) != 0;
            if (in && was[size_t(e)] == 0) n++;
            now_in += in ? 1 : 0;
            was[size_t(e)] = in ? 1 : 0;
          }
          return std::pair{n, now_in};
        };
        side_steps += begun("sq_side_step", was_stepping).first;
        const auto [ducks_begun, ducks_now] = begun("sq_duck_frame", was_ducking);
        dodges += ducks_begun;
        ducking = std::max(ducking, ducks_now);
        run_shots += begun("sq_run_step", was_running).first;
        trailing = std::max(trailing, count("sq_on_trail"));
        lost = std::max(lost, count("sq_lost"));
        heard = std::max(heard, count("sq_sound"));
      }
      lift_dz = static_cast<int>(lowest - z0);
      // The monsters on the hunt at the end: an enemy of their own (the world's is none).
      if (s = rules_of(l); s != nullptr) {
        const int enemy = mv::field_of(progs, "enemy");
        for (int e = 2; e < s->sv.num_edicts; e++)
          if (!s->sv.edicts[size_t(e)].free && s->sv.edicts[size_t(e)].health > 0 &&
              mv::string_at(*s, e, f.classname).rfind("monster_", 0) == 0 &&
              mv::word_at(*s, e, enemy) != 0) {
            hunting++;
            const sh::Vec3f d = s->sv.edicts[size_t(e)].origin - s->sv.edicts[1].origin;
            const float dist = sh::length(d);
            if (nearest < 0 || dist < nearest) nearest = dist;
          }
      }
      t.rows.push_back({"lift ridden", "down " + std::to_string(-lift_dz) + " units, the map run " +
                                           std::to_string(frames_run) + " frames after"});
      t.rows.push_back({"the player", "rode down " + std::to_string(int(me0 - me_low)) +
                                          " units; health at its lowest " +
                                          std::to_string(int(lowest_hp)) + ", killed " +
                                          std::to_string(deaths) + " time(s); hit " +
                                          std::to_string(hits) + " times, first after " +
                                          std::to_string(first_hit).substr(0, 4) + " s"});
      t.rows.push_back(
          {"the monsters", std::to_string(hunting) + " hunting at the end, the nearest " +
                               std::to_string(int(nearest)) + " units off; " +
                               std::to_string(trailing) + " on his trail at most, " +
                               std::to_string(lost) + " after where they lost him, " +
                               std::to_string(side_steps) + " side steps round a block, " +
                               std::to_string(heard) + " to a noise"});
      t.rows.push_back({"dodging", std::to_string(fired) + " frames he fired; " +
                                       std::to_string(dodges) + " ducks begun, " +
                                       std::to_string(ducking) + " ducking at once at most; " +
                                       std::to_string(run_shots) + " shots on the run"});
    }
  } catch (const std::exception& e) {
    thrown = e.what();
  }
  for (const std::string& p : l.prints)
    if (p.find("remix") != std::string::npos)
      t.rows.push_back({"printed", p.substr(0, p.find_last_not_of('\n') + 1)});
  lab.table(t);
  lab.metric("remix_campaign.played", 1, "");
  lab.metric("remix_campaign.q2_monsters", q2, "count");
  lab.metric("remix_campaign.q1_monsters", q1, "count");
  lab.metric("remix_campaign.lift_dz", lift_dz, "units");
  lab.metric("remix_campaign.frames_after_lift", frames_run, "frames");
  lab.metric("remix_campaign.player_lowest_health", lowest_hp, "");
  lab.metric("remix_campaign.player_killed", deaths, "count");
  lab.metric("remix_campaign.hits_taken", hits, "count");
  lab.metric("remix_campaign.first_hit", first_hit, "s");
  lab.metric("remix_campaign.monsters_hunting", hunting, "count");
  lab.metric("remix_campaign.nearest_hunter", nearest, "units");
  lab.metric("remix_campaign.on_trail_most", trailing, "count");
  lab.metric("remix_campaign.lost_most", lost, "count");
  lab.metric("remix_campaign.side_steps", side_steps, "count");
  lab.metric("remix_campaign.heard_most", heard, "count");
  lab.metric("remix_campaign.fired_frames", fired, "frames");
  lab.metric("remix_campaign.dodges", dodges, "count");
  lab.metric("remix_campaign.ducking_most", ducking, "count");
  lab.metric("remix_campaign.run_shots", run_shots, "count");
  if (!thrown.empty())
    lab.fail("se-remix-campaign: the game threw " + thrown,
             "next: run under gdb with `catch throw` to see where");
}

const gl::LabSpec kSpec{
    "se-remix-campaign",
    gl::LabKind::scenario,
    "game:shooters_edge",
    "games/shooters_edge/client (listen.cpp's campaign_map, remix.cpp's load_remixed_map)",
    "shooter",
    "Does cr_monsters set during a campaign's map reach the map the level's own gate leads to, "
    "and does that map play on through its first lift?",
    {
        {"game", "", "the played Quake's folder (<folder>/id1)"},
        {"remix", "", "the other full game's base folder (the one holding baseq2)"},
        {"remix-game", "q2", "the other game's profile id"},
        {"map", "start", "the map the campaign starts on"},
        {"then", "e1m1", "the map its gate leads to"},
        {"skill", "1", "the skill (1: medium)"},
        {"monsters", "q2", "what cr_monsters is set to during the first map"},
        {"take", "1", "1: the window takes the settings before the next map (0: before the fix)"},
        {"button", "t1", "the target of the button that moves the first lift"},
        {"seconds", "20", "how long the map runs once the lift is set off, s"},
        {"walk", "",
         "points the player walks through once the lift is down (x,y,z;x,y,z), at 300 units/s"},
        {"walk-after", "4", "seconds after the button before the walk starts"},
        {"shot-at", "-1", "seconds after the button the player makes a shot's noise (-1 none)"},
        {"fire", "0",
         "a weapon of Quake's by its rank the player fires (7 rockets, 4 nails; 0 none)"},
        {"fire-at", "2", "seconds after the button the player starts firing at them"},
    },
    "",
    run,
};
GE_LAB(kSpec);

}  // namespace
