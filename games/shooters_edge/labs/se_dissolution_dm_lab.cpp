// LAB `se-dissolution-dm`: DO DISSOLUTION OF ETERNITY'S RUNES, GRAPPLING HOOK AND TEAM MODES WORK
// ON OUR SERVER? The pack's own program (its progs.dat, from `--packs`) runs on the QC server over
// the test arena with up to three players, driven frame by frame; the lab plays their clients'
// part by running the `color` lines the program stuffs. Runes (deathmatch on, `gamecfg 1`): four
// come out of the first four deathmatch spots, one is taken and named, a second refused, the held
// one falls where its holder dies, untaken ones move after two minutes, none at `gamecfg 0`. The
// hook (`teamplay 4`): impulse 22 and attack throw it, the chain goes as beams, it pulls the player
// to the wall and goes when attack is let go; none at `teamplay 0`. Capture the flag (`teamplay
// 4`): joiners put on red and blue, the enemy flag taken, captured for 15, sent home, dropped by
// a killed carrier and returned, a change of pants refused; three teams at 6; the neutral flag
// taken and captured at the enemy base at 5; the tag token at 3. A table of every check and what
// was seen. With no `--packs` it says so and runs nothing: the packs are the player's own.
//
// ge:summary Lab: Dissolution of Eternity's runes, grappling hook and team modes on the QC server
// ge:keywords lab se-dissolution-dm runes gamecfg grappling-hook grapple teamplay impulse-22
// ge:keywords te-beam mission-pack dissolution-of-eternity rog1 capture-the-flag ctf tag-token
// ge:keywords pants-colour team-change
// ge:skill progs labs
// ge:test tests/shooters_edge_qc_server_test.cpp
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <memory>
#include <span>
#include <string>
#include <vector>

#include "core/libm/libm.hpp"
#include "core/math.hpp"
#include "core/num_text.hpp"
#include "lab/lab.hpp"
#include "progs/file.hpp"
#include "progs/vm.hpp"
#include "shooter/client.hpp"
#include "shooters_edge/server/commands.hpp"
#include "shooters_edge/server/messages.hpp"
#include "shooters_edge/server/mod_match.hpp"
#include "shooters_edge/server/qc_server.hpp"
#include "vfs/pak.hpp"

namespace {

namespace gl = ge::lab;
namespace sv = shooters_edge::server;
namespace sh = ge::shooter;
namespace pg = ge::progs;
using ge::core::num::to_fixed;

// The arena with the four deathmatch spots runes come out of, in this order, a start for the
// program's single player and nothing to pick up.
const char* const kEntities = R"({
"classname" "worldspawn"
"message" "dissolution deathmatch"
}
{
"classname" "info_player_start"
"origin" "-416 0 24"
}
{
"classname" "info_player_deathmatch"
"origin" "-416 384 24"
}
{
"classname" "info_player_deathmatch"
"origin" "-416 -384 24"
}
{
"classname" "info_player_deathmatch"
"origin" "0 384 24"
}
{
"classname" "info_player_deathmatch"
"origin" "0 -384 24"
}
)";

constexpr int kPlayers = 3;  // slots the lab can fill
constexpr double kFrameS = 0.013;
constexpr double kPi = 3.14159265358979;

struct Run {
  pg::Progs progs;
  std::unique_ptr<sv::QcServer> s;
  sh::MoveCmd cmd[kPlayers];
  uint32_t sequence = 0;
  std::vector<sv::NetEvent> events;  // the last frame's
  int beams = 0;                     // beam temporary entities seen in the last frame
  int joined = 0;                    // the players in, slots 0..joined-1
};

bool open(Run& r, const std::vector<uint8_t>& program, const std::vector<std::string>& console,
          std::string& error, const char* entities = kEntities) {
  if (!pg::load_progs(program.data(), program.size(), r.progs, error, pg::every_form()))
    return false;
  sv::QcServerOptions o;
  o.loop.max_edicts = 1024;
  r.s = sv::make_qc_server(r.progs, o, error);
  if (!r.s) return false;
  for (const std::string& l : console) sv::server_command(*r.s, l);
  sv::MapSpec map;
  map.name = "arena";
  map.models = sv::arena_models();
  map.entities = entities;
  if (!sv::load_map(*r.s, map, error)) return false;
  return true;
}

void join(Run& r, int k) {
  sv::connect(*r.s, k, k == 0 ? "Ana" : k == 1 ? "Bo" : "Cy", false, false);
  r.joined = k + 1;
}

int count_beams(const std::vector<uint8_t>& b) {
  int n = 0;
  for (size_t i = 0; i + 1 < b.size(); i++)
    if (b[i] == 23 && b[i + 1] == 13) n++;  // svc_temp_entity, the beam
  return n;
}

void frame(Run& r) {
  sh::Packet p[kPlayers];
  r.sequence++;
  for (int k = 0; k < r.joined; k++) {
    p[k].client = k;
    p[k].sequence = r.sequence;
    for (sh::MoveCmd& c : p[k].cmds) {
      c = r.cmd[k];
      c.msec = 13;
    }
    r.cmd[k].impulse = 0;
  }
  sv::qc_frame(*r.s, p, r.joined);
  r.events = r.s->out.events;
  // A client runs what the server stuffs: `color <shirt> [pants]` sets its colours.
  for (const sv::NetEvent& e : r.events) {
    if (e.kind != sv::EventKind::stuffcmd) continue;
    for (int k = 0; k < r.joined; k++) {
      if ((e.to & (1u << k)) == 0) continue;
      size_t at = 0;
      while ((at = e.text.find("color ", at)) != std::string::npos) {
        at += 6;
        int top = 0, bottom = -1;
        const int n = std::sscanf(e.text.c_str() + at, "%d %d", &top, &bottom);
        if (n < 1) continue;
        if (n < 2) bottom = top;
        sv::set_user_info(*r.s, k, "topcolor", std::to_string(top));
        sv::set_user_info(*r.s, k, "bottomcolor", std::to_string(bottom));
      }
    }
  }
  r.beams = count_beams(r.s->out.unreliable) + count_beams(r.s->out.multicast);
  for (const sv::NetEvent& e : r.events)
    if (e.kind == sv::EventKind::multicast) r.beams += count_beams(e.bytes);
  sv::clear_frame(r.s->out);
}

sh::Edict& ed(Run& r, int e) { return r.s->sv.edicts[static_cast<size_t>(e)]; }

std::string model_of(Run& r, int e) {
  const int m = ed(r, e).modelindex;
  return m > 0 && m < static_cast<int>(r.s->models.size()) ? r.s->models[static_cast<size_t>(m)]
                                                           : "";
}

float health(Run& r, int e) {
  return pg::as_float(
      r.s->q.vm.entities[static_cast<size_t>(e) * static_cast<size_t>(r.s->q.vm.entityfields) +
                         static_cast<size_t>(pg::kFHealth)]);
}

std::vector<int> with_model(Run& r, const std::string& prefix) {
  std::vector<int> out;
  for (int e = sh::kMaxClients + 1; e < r.s->sv.num_edicts; e++)
    if (!ed(r, e).free && model_of(r, e).rfind(prefix, 0) == 0) out.push_back(e);
  return out;
}

// The centre prints to slot `k` in the last frame.
std::string centre(Run& r, int k) {
  std::string out;
  for (const sv::NetEvent& e : r.events)
    if (e.kind == sv::EventKind::centerprint && (e.to & (1u << k)) != 0) out += e.text;
  return out;
}

// Player `k` walks toward `to`; frames until `done` or `limit` seconds. Returns true if done.
template <class Done>
bool walk(Run& r, int k, sh::Vec3f to, double limit, Done done) {
  for (int i = 0; i < static_cast<int>(limit / kFrameS); i++) {
    const sh::Vec3f at = ed(r, k + 1).origin;
    const double dx = to.x - at.x, dy = to.y - at.y;
    r.cmd[k].angles = {0, static_cast<float>(ge::core::libm::atan2(dy, dx) * 180.0 / kPi), 0};
    r.cmd[k].forward = ge::core::hypot(dx, dy) > 4 ? 320 : 0;
    frame(r);
    if (done()) {
      r.cmd[k].forward = 0;
      return true;
    }
  }
  r.cmd[k].forward = 0;
  return false;
}

void idle(Run& r, double s) {
  for (int i = 0; i < static_cast<int>(s / kFrameS); i++) frame(r);
}

struct Checks {
  gl::LabTable t{"Dissolution of Eternity's runes, grappling hook and team modes on our server",
                 {"check", "seen", "as it should be"},
                 {}};
  int failed = 0;
  void row(const std::string& what, const std::string& seen, bool ok) {
    t.rows.push_back({what, seen, ok ? "yes" : "NO"});
    if (!ok) failed++;
  }
};

// The line of `s` holding `key`, or the last line when none does.
std::string line_of(const std::string& s, const std::string& key) {
  size_t at = s.find(key);
  if (at == std::string::npos) at = s.size() > 1 ? s.rfind('\n', s.size() - 2) + 1 : 0;
  const size_t from = s.rfind('\n', at) == std::string::npos ? 0 : s.rfind('\n', at) + 1;
  const size_t to = s.find('\n', at);
  return s.substr(from, to == std::string::npos ? std::string::npos : to - from);
}

std::string one_line(std::string s) {
  for (char& c : s)
    if (c == '\n') c = '|';
  return s;
}

void runes(const std::vector<uint8_t>& program, Checks& c, gl::Lab& lab) {
  {
    Run r;
    std::string error;
    if (!open(r, program, {"deathmatch 1", "gamecfg 0"}, error))
      return c.row("the map loads (gamecfg 0)", error, false);
    idle(r, 2);
    const size_t n = with_model(r, "progs/end").size();
    c.row("gamecfg 0: no runes", std::to_string(n) + " runes", n == 0);
  }
  Run r;
  std::string error;
  if (!open(r, program, {"deathmatch 1", "gamecfg 1"}, error))
    return c.row("the map loads (gamecfg 1)", error, false);
  idle(r, 2);
  std::vector<int> rs = with_model(r, "progs/end");
  c.row("gamecfg 1: four runes come out", std::to_string(rs.size()) + " runes", rs.size() == 4);
  lab.metric("runes.out", static_cast<double>(rs.size()), "count");
  if (rs.size() != 4) return;
  // Ana joins once they have landed, away from where they lie (a spawn spot may hold one).
  join(r, 0);
  std::string said;
  frame(r);
  said += centre(r, 0);
  // Ana walks to the nearest rune.
  auto nearest = [&](int k) {
    int best = -1;
    double bd = 1e18;
    const sh::Vec3f at = ed(r, k + 1).origin;
    for (const int e : with_model(r, "progs/end")) {
      const sh::Vec3f o = ed(r, e).origin;
      const double d = ge::core::hypot(o.x - at.x, o.y - at.y);
      if (d < bd) bd = d, best = e;
    }
    return best;
  };
  bool took = with_model(r, "progs/end").size() == 3;
  if (!took) {
    const int target = nearest(0);
    took = walk(r, 0, ed(r, target).origin, 8, [&] {
      said += centre(r, 0);
      return ed(r, target).free || model_of(r, target).rfind("progs/end", 0) != 0;
    });
  }
  c.row("a player walks onto a rune and takes it", took ? "taken" : "not reached in 8 s", took);
  c.row("the rune's name is centre printed", one_line(said),
        said.find("Magic") != std::string::npos);
  const size_t left = with_model(r, "progs/end").size();
  c.row("three runes are left on the ground", std::to_string(left), left == 3);
  // A second rune is refused.
  const int target = nearest(0);
  said.clear();
  if (target > 0) {
    walk(r, 0, ed(r, target).origin, 8, [&] {
      said += centre(r, 0);
      return said.find("already") != std::string::npos;
    });
    idle(r, 0.5);
  }
  c.row("a second rune is refused", one_line(said),
        said.find("You already have a rune") != std::string::npos &&
            with_model(r, "progs/end").size() == 3);
  // Ana dies: the rune falls where she was. Bo joins and shoots her (`kill` respawns at once,
  // which is no death).
  const size_t ground = with_model(r, "progs/end").size();
  join(r, 1);
  idle(r, 0.5);
  bool dead = false;
  std::string bo_said;
  for (int i = 0; i < static_cast<int>(30 / kFrameS) && !dead; i++) {
    const sh::Vec3f a = ed(r, 1).origin, b = ed(r, 2).origin;
    const double d = ge::core::hypot(a.x - b.x, a.y - b.y);
    r.cmd[1].angles = {
        0, static_cast<float>(ge::core::libm::atan2(a.y - b.y, a.x - b.x) * 180.0 / kPi), 0};
    // Bo keeps 120 to 160 units off, so the rune she drops does not land on him.
    r.cmd[1].forward = static_cast<int16_t>(d > 160 ? 320 : d < 120 ? -320 : 0);
    r.cmd[1].buttons = d < 200 ? sh::kButtonAttack : 0;
    frame(r);
    bo_said += centre(r, 1);
    dead = health(r, 1) <= 0;
  }
  r.cmd[1] = {};
  for (int i = 0; i < static_cast<int>(1.5 / kFrameS); i++) {
    frame(r);
    bo_said += centre(r, 1);
  }
  c.row("Ana dies to Bo's shotgun", dead ? "dead" : "alive after 30 s", dead);
  // Bo may walk over one on the way (he is told its name): a rune he holds is off the ground.
  const size_t bo_took = bo_said.find("Magic") != std::string::npos ? 1 : 0;
  const size_t after = with_model(r, "progs/end").size();
  c.row("the held rune falls on death",
        std::to_string(ground) + " on the ground before, " + std::to_string(after) +
            " after, Bo took " + std::to_string(bo_took),
        after + bo_took == ground + 1);
  // An untaken rune moves after 120 s: Ana comes back and stands still.
  std::vector<sh::Vec3f> before;
  for (const int e : with_model(r, "progs/end")) before.push_back(ed(r, e).origin);
  r.cmd[0].buttons = sh::kButtonJump;  // respawn
  frame(r);
  r.cmd[0].buttons = 0;
  idle(r, 125);
  int moved = 0;
  const std::vector<int> now = with_model(r, "progs/end");
  for (size_t i = 0; i < now.size() && i < before.size(); i++) {
    const sh::Vec3f o = ed(r, now[i]).origin;
    if (ge::core::hypot(o.x - before[i].x, o.y - before[i].y) > 1) moved++;
  }
  c.row("untaken runes move after two minutes",
        std::to_string(moved) + " of " + std::to_string(now.size()) + " moved",
        now.size() + bo_took == 4 && moved >= 1);
}

void hook(const std::vector<uint8_t>& program, Checks& c, gl::Lab& lab) {
  {
    Run r;
    std::string error;
    if (!open(r, program, {"deathmatch 1", "teamplay 0"}, error))
      return c.row("the map loads (teamplay 0)", error, false);
    join(r, 0);
    idle(r, 1);
    r.cmd[0].impulse = 22;
    frame(r);
    r.cmd[0].buttons = sh::kButtonAttack;
    idle(r, 1);
    r.cmd[0].buttons = 0;
    c.row("teamplay 0: impulse 22 gives no hook",
          std::to_string(with_model(r, "progs/hook").size()) + " hooks",
          with_model(r, "progs/hook").empty());
  }
  Run r;
  std::string error;
  if (!open(r, program, {"deathmatch 1", "teamplay 4"}, error))
    return c.row("the map loads (teamplay 4)", error, false);
  join(r, 0);
  idle(r, 1);
  // Ana goes to (-416, 384) and faces east: the dividing wall stands at x = 128.
  walk(r, 0, {-416, 384, 24}, 6, [&] {
    const sh::Vec3f o = ed(r, 1).origin;
    return ge::core::hypot(o.x + 416, o.y - 384) < 8;
  });
  idle(r, 0.5);
  r.cmd[0].angles = {0, 0, 0};
  r.cmd[0].impulse = 22;
  frame(r);
  idle(r, 0.5);
  const float x0 = ed(r, 1).origin.x;
  r.cmd[0].buttons = sh::kButtonAttack;
  bool flew = false;
  int beams = 0;
  float most = x0;
  for (int i = 0; i < static_cast<int>(2.0 / kFrameS); i++) {
    frame(r);
    if (!with_model(r, "progs/hook").empty()) flew = true;
    beams += r.beams;
    most = std::max(most, ed(r, 1).origin.x);
  }
  c.row("attack held throws the hook", flew ? "progs/hook.mdl in the world" : "no hook", flew);
  c.row("the chain is sent as beams", std::to_string(beams) + " beams in 2 s", beams > 5);
  c.row("the player is pulled to the wall",
        "x " + to_fixed(x0, 0) + " to " + to_fixed(most, 0) + " (the wall at 128)",
        most > 64 && most < 128);
  lab.metric("hook.pulled-to-x", most, "units");
  r.cmd[0].buttons = 0;
  idle(r, 0.5);
  c.row("letting go takes the hook away",
        std::to_string(with_model(r, "progs/hook").size()) + " hooks",
        with_model(r, "progs/hook").empty());
}

// The arena for the team modes: the red flag in the west room, the blue in the east, a neutral
// flag and a tag token in the doorway, each team's spots by its flag and the four deathmatch spots.
const char* const kTeamEntities = R"({
"classname" "worldspawn"
"message" "dissolution team modes"
}
{
"classname" "info_player_start"
"origin" "-416 0 24"
}
{
"classname" "info_player_deathmatch"
"origin" "-416 384 24"
}
{
"classname" "info_player_deathmatch"
"origin" "-416 -384 24"
}
{
"classname" "info_player_deathmatch"
"origin" "416 256 24"
}
{
"classname" "info_player_deathmatch"
"origin" "416 -96 24"
}
{
"classname" "info_player_team1"
"origin" "-416 384 24"
}
{
"classname" "info_player_team1"
"origin" "-416 -384 24"
}
{
"classname" "info_player_team2"
"origin" "416 256 24"
}
{
"classname" "info_player_team2"
"origin" "416 -96 24"
}
{
"classname" "item_flag_team1"
"origin" "-416 0 24"
}
{
"classname" "item_flag_team2"
"origin" "416 80 24"
}
{
"classname" "item_flag"
"origin" "-200 0 24"
}
{
"classname" "dmatch_tag_token"
"origin" "-200 0 24"
}
)";

// Everything printed or centre printed to slot `k` in the last frame, the coloured letters plain.
std::string heard(Run& r, int k) {
  std::string out;
  for (const sv::NetEvent& e : r.events)
    if ((e.kind == sv::EventKind::print || e.kind == sv::EventKind::centerprint) &&
        (e.to & (1u << k)) != 0)
      for (const char ch : e.text) out += static_cast<char>(ch & 127);
  return out;
}

float field(Run& r, int e, const char* name) {
  const pg::Def* d = pg::find_field(r.progs, name);
  return d == nullptr ? -1 : pg::as_float(*pg::entity_field(r.s->q.vm, e, d->offset));
}

float frags(Run& r, int e) { return field(r, e, "frags"); }

std::string classname(Run& r, int e) {
  const pg::Def* d = pg::find_field(r.progs, "classname");
  if (d == nullptr) return "";
  const char* n =
      pg::vm_string(r.s->q.vm, static_cast<int32_t>(*pg::entity_field(r.s->q.vm, e, d->offset)));
  return n == nullptr ? "" : n;
}

int find_class(Run& r, const std::string& cls) {
  for (int e = sh::kMaxClients + 1; e < r.s->sv.num_edicts; e++)
    if (!ed(r, e).free && classname(r, e) == cls) return e;
  return -1;
}

// Player `k` walks to `to`, through the doorway (x 128..144, y -64..64) when the dividing wall is
// between; the slot's prints gathered into `said`. True when `done` held within `limit` seconds.
template <class Done>
bool go(Run& r, int k, sh::Vec3f to, double limit, std::string& said, Done done) {
  const sh::Vec3f door{136, 0, 24};
  const double t0 = r.s->sv.time;
  while (r.s->sv.time - t0 < limit) {
    const sh::Vec3f at = ed(r, k + 1).origin;
    const bool across = (at.x < 128) != (to.x < 128);
    const sh::Vec3f aim =
        across && ge::core::hypot(at.x - door.x, at.y - door.y) > 24
            ? (std::abs(at.y) > 40 ? sh::Vec3f{at.x < 128 ? 60.0f : 220.0f, 0, 24} : door)
            : to;
    const double dx = aim.x - at.x, dy = aim.y - at.y;
    r.cmd[k].angles = {0, static_cast<float>(ge::core::libm::atan2(dy, dx) * 180.0 / kPi), 0};
    r.cmd[k].forward = ge::core::hypot(dx, dy) > 4 ? 320 : 0;
    frame(r);
    said += heard(r, k);
    if (done()) {
      r.cmd[k].forward = 0;
      return true;
    }
  }
  r.cmd[k].forward = 0;
  return false;
}

void teams(const std::vector<uint8_t>& program, Checks& c, gl::Lab& lab) {
  // Two-flag capture the flag.
  {
    Run r;
    std::string error;
    if (!open(r, program, {"deathmatch 1", "teamplay 4"}, error, kTeamEntities))
      return c.row("the map loads (teamplay 4)", error, false);
    std::string said[kPlayers];
    join(r, 0);
    frame(r);
    said[0] += heard(r, 0);
    join(r, 1);
    for (int i = 0; i < 40; i++) {
      frame(r);
      for (int k = 0; k < kPlayers; k++) said[k] += heard(r, k);
    }
    const float ta = field(r, 1, "team"), tb = field(r, 2, "team");
    c.row("teamplay 4: each joiner is put on a team",
          line_of(said[0], "assigned") + "; teams " + to_fixed(ta, 0) + " and " + to_fixed(tb, 0),
          said[0].find("You have been assigned to") != std::string::npos &&
              said[1].find("You have been assigned to") != std::string::npos && ta != tb &&
              (ta == 5 || ta == 14) && (tb == 5 || tb == 14));
    const int red = ta == 5 ? 0 : 1, blue = 1 - red;
    {
      const sh::Vec3f pr = ed(r, red + 1).origin, pb = ed(r, blue + 1).origin;
      const bool red_at = pr.x == -416 && (pr.y == 384 || pr.y == -384);
      const bool blue_at = pb.x == 416 && (pb.y == 256 || pb.y == -96);
      c.row("each starts at his team's spot",
            "red at " + to_fixed(pr.x, 0) + " " + to_fixed(pr.y, 0) + ", blue at " +
                to_fixed(pb.x, 0) + " " + to_fixed(pb.y, 0),
            red_at && blue_at);
    }
    const int red_flag = find_class(r, "item_flag_team1");
    const int blue_flag = find_class(r, "item_flag_team2");
    c.row("both team flags stand", std::to_string(red_flag) + ", " + std::to_string(blue_flag),
          red_flag > 0 && blue_flag > 0);
    if (red_flag <= 0 || blue_flag <= 0) return;
    const sh::Vec3f blue_home = ed(r, blue_flag).origin, red_home = ed(r, red_flag).origin;
    // The red player takes the blue flag.
    std::string s;
    go(r, red, blue_home, 10, s,
       [&] { return s.find("YOU GOT THE ENEMY FLAG") != std::string::npos; });
    c.row("the red player takes the blue flag", line_of(s, "got the"),
          s.find("got") != std::string::npos &&
              s.find("YOU GOT THE ENEMY FLAG") != std::string::npos);
    // And brings it home: a capture.
    const float before = frags(r, red + 1);
    s.clear();
    go(r, red, red_home, 15, s, [&] { return s.find("captured") != std::string::npos; });
    idle(r, 0.2);
    const float gained = frags(r, red + 1) - before;
    c.row("brought to his own flag, it is captured", line_of(s, "captured"),
          s.find("captured the BLUE flag") != std::string::npos &&
              s.find("Your team captured the flag!") != std::string::npos);
    c.row("the capture scores 15", to_fixed(gained, 0) + " frags", gained == 15);
    lab.metric("ctf.capture-frags", gained, "frags");
    const sh::Vec3f bf = ed(r, blue_flag).origin;
    c.row("the blue flag goes home", "at " + to_fixed(bf.x, 0) + " " + to_fixed(bf.y, 0),
          ge::core::hypot(bf.x - blue_home.x, bf.y - blue_home.y) < 2);
    // He takes it again and the blue player kills him: the flag falls; the blue player returns it.
    s.clear();
    go(r, red, blue_home, 15, s,
       [&] { return s.find("YOU GOT THE ENEMY FLAG") != std::string::npos; });
    std::string bs;
    bool dead = false;
    for (int i = 0; i < static_cast<int>(30 / kFrameS) && !dead; i++) {
      const sh::Vec3f a = ed(r, red + 1).origin, b = ed(r, blue + 1).origin;
      const double d = ge::core::hypot(a.x - b.x, a.y - b.y);
      r.cmd[blue].angles = {
          0, static_cast<float>(ge::core::libm::atan2(a.y - b.y, a.x - b.x) * 180.0 / kPi), 0};
      r.cmd[blue].forward = static_cast<int16_t>(d > 160 ? 320 : d < 120 ? -320 : 0);
      r.cmd[blue].buttons = d < 200 ? sh::kButtonAttack : 0;
      frame(r);
      bs += heard(r, blue);
      dead = field(r, red + 1, "health") <= 0;
    }
    r.cmd[blue] = {};
    frame(r);
    bs += heard(r, blue);
    c.row("the carrier killed drops the flag", line_of(bs, "lost"),
          dead && bs.find("lost the BLUE flag") != std::string::npos);
    const sh::Vec3f dropped = ed(r, blue_flag).origin;
    bs.clear();
    go(r, blue, dropped, 10, bs, [&] { return bs.find("returned") != std::string::npos; });
    c.row("the blue player returns his own flag", line_of(bs, "returned"),
          bs.find("returned the BLUE flag") != std::string::npos);
    // The red player changes his pants to blue: refused, killed, put back.
    {
      std::string ch;
      sv::set_user_info(*r.s, red, "bottomcolor", "13");
      for (int i = 0; i < 80; i++) {
        frame(r);
        ch += heard(r, red);
      }
      const float team = field(r, red + 1, "team");
      c.row("changing teams is refused",
            line_of(ch, "change teams") + "; team " + to_fixed(team, 0),
            ch.find("You cannot change teams.") != std::string::npos && team == 5);
    }
  }
  // Three teams: the third joiner is grey.
  {
    Run r;
    std::string error;
    if (!open(r, program, {"deathmatch 1", "teamplay 6"}, error, kTeamEntities))
      return c.row("the map loads (teamplay 6)", error, false);
    std::string said;
    join(r, 0);
    idle(r, 0.2);
    join(r, 1);
    idle(r, 0.2);
    join(r, 2);
    for (int i = 0; i < 40; i++) {
      frame(r);
      said += heard(r, 2);
    }
    std::string teams_seen;
    for (int k = 0; k < 3; k++)
      teams_seen += (k ? ", " : "") + to_fixed(field(r, k + 1, "team"), 0);
    const float t[3] = {field(r, 1, "team"), field(r, 2, "team"), field(r, 3, "team")};
    c.row("teamplay 6: three joiners on three teams", "teams " + teams_seen,
          t[0] != t[1] && t[1] != t[2] && t[0] != t[2] && (t[0] == 1 || t[1] == 1 || t[2] == 1));
  }
  // One flag.
  {
    Run r;
    std::string error;
    if (!open(r, program, {"deathmatch 1", "teamplay 5"}, error, kTeamEntities))
      return c.row("the map loads (teamplay 5)", error, false);
    join(r, 0);
    join(r, 1);
    idle(r, 0.5);
    const int flag = find_class(r, "item_flag");
    const int b1 = find_class(r, "item_flagbase_team1"), b2 = find_class(r, "item_flagbase_team2");
    c.row("teamplay 5: the neutral flag and both bases",
          std::to_string(flag) + ", " + std::to_string(b1) + ", " + std::to_string(b2) +
              (find_class(r, "item_flag_team1") > 0 ? "; a team flag stands" : ""),
          flag > 0 && b1 > 0 && b2 > 0 && find_class(r, "item_flag_team1") < 0);
    if (flag > 0 && b1 > 0 && b2 > 0) {
      const int red = field(r, 1, "team") == 5 ? 0 : 1;
      std::string s;
      go(r, red, ed(r, flag).origin, 10, s,
         [&] { return s.find("TAKE IT TO") != std::string::npos; });
      c.row("a player takes the neutral flag", line_of(s, "got the"),
            s.find("got the flag") != std::string::npos);
      s.clear();
      go(r, red, ed(r, b2).origin, 15, s, [&] { return s.find("captured") != std::string::npos; });
      c.row("carried to the enemy base, it is captured", line_of(s, "captured"),
            s.find("captured the flag") != std::string::npos);
    }
  }
  // Tag.
  {
    Run r;
    std::string error;
    if (!open(r, program, {"deathmatch 1", "teamplay 3"}, error, kTeamEntities))
      return c.row("the map loads (teamplay 3)", error, false);
    join(r, 0);
    idle(r, 0.5);
    const int token = find_class(r, "dmatch_tag_token");
    c.row("teamplay 3: the tag token stands", std::to_string(token), token > 0);
    if (token > 0) {
      std::string s;
      go(r, 0, ed(r, token).origin, 10, s,
         [&] { return s.find("tag token") != std::string::npos; });
      c.row("a player takes the tag token", line_of(s, "tag token"),
            s.find("got the tag token!") != std::string::npos);
    }
  }
}

void run(gl::Lab& lab) {
  const std::string root = lab.arg("packs");
  if (root.empty()) {
    lab.note(
        "se-dissolution-dm: no --packs, nothing run (the packs are the player's own; point "
        "--packs at the folder holding rogue/pak0.pak)");
    return;
  }
  const std::filesystem::path path = std::filesystem::path(root) / "rogue" / "pak0.pak";
  std::ifstream f(path, std::ios::binary);
  const std::vector<uint8_t> bytes((std::istreambuf_iterator<char>(f)),
                                   std::istreambuf_iterator<char>());
  ge::vfs::Pak pak;
  std::string error;
  const ge::vfs::PakEntry* e = nullptr;
  if (!bytes.empty() && ge::vfs::read_pak(bytes, &pak, &error))
    e = ge::vfs::pak_find(pak, "progs.dat");
  if (e == nullptr)
    return lab.fail("se-dissolution-dm: no progs.dat in " + path.string(),
                    "next: point --packs at the folder holding rogue/pak0.pak");
  const std::span<const uint8_t> p = ge::vfs::pak_file(bytes, *e);
  const std::vector<uint8_t> program(p.begin(), p.end());
  Checks c;
  runes(program, c, lab);
  hook(program, c, lab);
  teams(program, c, lab);
  lab.table(c.t);
  lab.metric("checks.failed", c.failed, "count");
  if (c.failed > 0)
    lab.fail("se-dissolution-dm: " + std::to_string(c.failed) + " checks failed (the table)",
             "next: make lab LAB=se-dissolution-dm ARGS=--packs=<folder> and read the rows");
}

const gl::LabSpec kSpec{
    "se-dissolution-dm",
    gl::LabKind::scenario,
    "game:shooters_edge",
    "games/shooters_edge/server: Dissolution of Eternity's program on the QC server (runes, the "
    "grappling hook, the team modes)",
    "progs",
    "Do Dissolution of Eternity's runes, grappling hook and team modes work on our server?",
    {
        {"packs", "", "a folder holding rogue/pak0.pak (the player's own Dissolution of Eternity)"},
    },
    "",
    run,
};
GE_LAB(kSpec);

}  // namespace
