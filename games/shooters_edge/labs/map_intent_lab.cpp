// LAB `map-intent`: DOES A MAP PLAY AS ITS PLAN SAYS IT IS MEANT TO?
// A map plan (plugins/brush/compile/plan.hpp) says what each place is for (`area ... intent=`) and
// which ways a skilled player is meant to take (`trick ... kind=`). This lab compiles the plan,
// proves every trick on the move (plugins/shooter/nav.hpp: the jump, the rocket and grenade jumps
// with the rules' push, a teammate's boost), plays a match of bots on it for each mode the plan is
// made for (games/shooters_edge/net/classic_team.hpp: the baseline mod's rules, the fighters),
// reads the match's recording back (client/demo_scan.hpp: every player's track, every kill and
// where the killer and the victim stood) and holds each area to its intent:
//
//   camp    a spot one player holds: the stays long (a visit 5 s or more on the mean), one side
//           holds it (60 % of its time or more, in a team mode), and it kills at least as many as
//           die in it;
//   hold    a team's base: 8 % of the players' time or more, one side holding it as above;
//   fight   where the sides meet: deaths in it a minute at least the median of the map's areas,
//           and both sides there (neither under 30 % of its time, in a team mode);
//   route   a way through: one visit a minute or more, each over in 3 s or less on the mean;
//   power   a power's spot: someone comes at least once in two minutes;
//   perch   high ground: it kills half again as many as die in it, and some;
//   spawn   where players come in: tabled, not judged.
//
// Recordings of other matches on the map (`--demos`, people's or the bots') are read the same way
// and their areas tabled beside the bots'. FFA is played as two sides with teamplay off (the
// fighters still keep to their side's plan); midair is not played on the first game's rules yet.
//
// ge:summary Lab: a map plan's areas and tricks held to their intent by bots' matches and proofs
// ge:keywords lab map-intent map-design area intent camp hold fight route perch power trick
// ge:keywords analytics occupancy dwell kills deaths bots match duel 2on2 4on4 ffa
// ge:skill map-design labs
// ge:test tests/brush_compile_test.cpp
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <iterator>
#include <map>
#include <string>
#include <vector>

#include "brush/compile/compile.hpp"
#include "brush/compile/plan.hpp"
#include "brush/read.hpp"
#include "core/num_text.hpp"
#include "lab/lab.hpp"
#include "lab/raster.hpp"
#include "shooter/bsp_world.hpp"
#include "shooter/jump_finder.hpp"
#include "shooter/nav.hpp"
#include "shooters_edge/client/demo.hpp"
#include "shooters_edge/client/demo_scan.hpp"
#include "shooters_edge/client/listen.hpp"
#include "shooters_edge/labs/wire_view.hpp"
#include "shooters_edge/net/classic_team.hpp"

namespace {

namespace gl = ge::lab;
namespace sh = ge::shooter;
namespace mc = ge::brush::mapc;
namespace cl = shooters_edge::client;
namespace cn = shooters_edge::net;
using ge::core::num::to_fixed;

// The lab's sample: a small duel map with its intent written down.
constexpr const char* kSample = R"(world message="Intent" light=12
textures floor=floor wall=wall ceil=ceiling solid=wall
modes duel,2on2
symmetry rotate=640,384
room hall z=0..192 box 0,128 384,640
solid ledge z=0..32 box 0,128 96,640 top=ledge
stairs z=0..32 box 96,512 192,640 up=-x top=ledge
room run z=0..128 box 384,320 640,448
room mid z=-32..320 box 640,96 896,672 once
stairs z=-32..0 box 640,320 704,448 up=-x once
stairs z=-32..0 box 832,320 896,448 up=+x once
solid balcony z=-32..176 box 832,96 896,224 once
area ledge z=32..192 box 0,128 96,640 intent=perch
area balcony z=176..320 box 832,96 896,224 intent=perch once
area run z=0..128 box 384,320 640,448 intent=route
area mid z=-32..320 box 640,96 896,672 intent=fight once
area armour z=0..192 box 192,128 384,320 intent=camp
trick onto-ledge from 160,384,24 to 20,384,56 kind=jump
trick balcony-rj from 740,160,-8 to 864,160,200 kind=rj level=1 once
light 192,384,160 value=300
light 768,384,160 value=300 once
spawn 300,600,24 angle=180
spawn 48,200,56 angle=0
entity item_armor2 288,224,24
entity weapon_rocketlauncher 768,560,-8 once
entity item_health 512,384,24
)";

std::string read_text(const std::string& path) {
  std::ifstream in(path, std::ios::binary);
  if (!in) return {};
  return {std::istreambuf_iterator<char>(in), {}};
}

bool in_modes(const std::string& modes, const std::string& mode) {
  if (modes.empty()) return true;
  size_t at = 0;
  while (at <= modes.size()) {
    const size_t c = modes.find(',', at);
    if (modes.substr(at, c == std::string::npos ? std::string::npos : c - at) == mode) return true;
    if (c == std::string::npos) break;
    at = c + 1;
  }
  return false;
}

sh::Vec3f v3(const double p[3]) { return {float(p[0]), float(p[1]), float(p[2])}; }

struct Proof {
  bool reached = false;
  float health = 0;
  std::string miss;  // a rocket jump not made: where its nearest arc came down
};

// A trick flown on the move the way its kind says.
Proof prove(const sh::MoveWorld& world, const mc::PlanTrick& t) {
  const sh::MoveSettings move;
  sh::NavSettings set;
  set.limit_s = 4;
  // A plan writes a standing origin at its floor + 24; the move stands a box a 32nd over the floor
  // (an origin exactly on the plane starts in it and goes nowhere).
  sh::Vec3f from = v3(t.from);
  const sh::Vec3f to = v3(t.to);
  from.z += 0.03125f;
  float hp = 0;
  std::string miss;
  sh::NavProof p;
  // A jump that comes down on the target's floor short of it or past it is made when the rest is
  // a walk (a proof's arrival is 8 units: a jump's landing is never that exact).
  const auto walked = [&](sh::NavProof q) {
    const float dz = q.end.z - to.z;
    if (q.reached || dz > 18 || dz < -18) return q;
    sh::NavSettings w = set;
    w.limit_s = 2;
    q.reached =
        sh::prove_link(world, move, q.end + sh::Vec3f{0, 0, 0.03125f}, to, sh::NavKind::walk, 0, w)
            .reached;
    return q;
  };
  const auto jump = [&](float speed) {
    return walked(sh::prove_link(world, move, from, to, sh::NavKind::jump, speed, set));
  };
  const sh::JumpFinderSettings finder;
  // A rocket jump is CALCULATED first (the arc from the take-off and the blast,
  // shooter/jump_finder.hpp); flown command by command only when no arc lands.
  const auto rocket = [&](const sh::RocketJumpSpec& rj) {
    const sh::RocketArc arc = sh::calc_any_rocket_jump(world, move, from, to, rj);
    if (arc.landed) {
      p.reached = true;
      hp = arc.health;
      return;
    }
    if (arc.fired)
      miss = to_fixed(arc.end.x, 0) + " " + to_fixed(arc.end.y, 0) + " " + to_fixed(arc.end.z, 0);
    p = sh::prove_any_rocket_jump(world, move, from, to, set, rj, true, finder.runs, nullptr, &hp);
  };
  if (t.kind == "jump") {
    p = jump(0);
    if (!p.reached) p = jump(320);
  } else if (t.kind == "hop") {
    for (const float s : {480.f, 560.f, 640.f}) {
      p = jump(s);
      if (p.reached) break;
    }
  } else if (t.kind == "head") {  // from a teammate's head, he standing at `from`
    p = walked(sh::prove_head_jump(world, move, from, to, 0, set));
    if (!p.reached) p = walked(sh::prove_head_jump(world, move, from, to, 320, set));
  } else if (t.kind == "rj") {
    rocket({});
  } else if (t.kind == "flip") {
    sh::RocketJumpSpec rj;
    rj.back = true;
    rocket(rj);
  } else if (t.kind == "gj") {
    for (const float lead : {0.f, 16.f, 32.f}) {
      p = sh::prove_grenade_jump(world, move, from, to, lead, set, {}, &hp);
      if (p.reached) break;
    }
  } else if (t.kind == "boost-rocket") {
    sh::RocketJumpSpec rj;
    rj.self_share = 1;
    rocket(rj);
    hp = 0;
  } else if (t.kind == "boost-double") {
    p = jump(320 + 448);
  }
  return {p.reached, hp, p.reached ? std::string() : miss};
}

// How often the players of a recording made a trick: at its start (within 64 across, 40 up or
// down) and at its end within 3 s.
int takes(const std::array<std::vector<sh::TrackPoint>, 32>& tracks, const mc::PlanTrick& t) {
  int n = 0;
  const auto near = [](const sh::Vec3f& a, const double b[3]) {
    const double dx = a.x - b[0], dy = a.y - b[1];
    return dx * dx + dy * dy <= 64 * 64 && std::fabs(a.z - b[2]) <= 40;
  };
  for (const auto& tr : tracks) {
    double started = -1;
    for (const sh::TrackPoint& q : tr) {
      if (q.dead) continue;
      if (near(q.origin, t.from)) started = q.t;
      if (started >= 0 && q.t - started <= 3 && near(q.origin, t.to)) {
        ++n;
        started = -1;
      }
    }
  }
  return n;
}

struct AreaCount {
  double time = 0, team_time[2] = {}, stays = 0;
  int visits = 0, deaths = 0, kills = 0;
};

struct Played {
  std::string mode, from;  // the mode; "bots" or the recording's path
  double minutes = 0;
  bool teams = true;
  std::vector<AreaCount> areas;
  std::vector<int> trick_takes;
  std::array<std::vector<sh::TrackPoint>, 32> tracks;
  std::array<int, 32> team{};
  std::vector<cl::DemoKill> kills;
  std::string score;
};

// The areas counted off a recording's bytes (the slots' sides from `team`, -1 none).
bool count(std::vector<uint8_t> bytes, const mc::PlanIntent& in, Played& out, std::string& err) {
  cl::Demo d;
  if (!cl::open_demo(std::move(bytes), d, err)) return false;
  cl::DemoScan scan;
  if (!cl::scan_demo(d, scan, err, true)) return false;
  out.tracks = scan.tracks;
  out.kills = scan.kills;
  out.minutes = scan.length_s / 60;
  out.areas.assign(in.areas.size(), {});
  for (size_t a = 0; a < in.areas.size(); ++a) {
    const mc::PlanArea& ar = in.areas[a];
    if (!in_modes(ar.modes, out.mode)) continue;
    AreaCount& c = out.areas[a];
    for (size_t k = 0; k < 32; ++k) {
      const auto& tr = scan.tracks[k];
      bool inside = false;
      for (size_t i = 1; i < tr.size(); ++i) {
        const sh::TrackPoint& q = tr[i];
        const bool now = !q.dead && mc::area_holds(ar, q.origin.x, q.origin.y, q.origin.z);
        const double dt = std::min(0.2, q.t - tr[i - 1].t);
        if (now) {
          c.time += dt;
          const int side = out.team[k];
          if (side == 0 || side == 1) c.team_time[side] += dt;
          if (!inside) ++c.visits;
        }
        inside = now;
      }
    }
    for (const cl::DemoKill& kl : scan.kills) {
      if (mc::area_holds(ar, kl.victim_at.x, kl.victim_at.y, kl.victim_at.z)) ++c.deaths;
      if (kl.killer >= 0 && mc::area_holds(ar, kl.killer_at.x, kl.killer_at.y, kl.killer_at.z))
        ++c.kills;
    }
  }
  for (const mc::PlanTrick& t : in.tricks) out.trick_takes.push_back(takes(scan.tracks, t));
  return true;
}

// The verdict on an area, and why.
std::string judge(const mc::PlanArea& a, const AreaCount& c, const Played& pl, double total,
                  double median_deaths) {
  if (c.time <= 0) return "NO: nobody came";
  const double mins = std::max(pl.minutes, 1e-6);
  const double stay = c.visits > 0 ? c.time / c.visits : 0;
  const double share = total > 0 ? c.time / total : 0;
  const double top = std::max(c.team_time[0], c.team_time[1]) / c.time;
  const double low = std::min(c.team_time[0], c.team_time[1]) / c.time;
  const double deaths_min = c.deaths / mins;
  if (a.intent == "camp") {
    if (stay < 5) return "NO: stays " + to_fixed(stay, 1) + " s (a camp holds 5 or more)";
    if (pl.teams && top < 0.6) return "NO: no side holds it (" + to_fixed(100 * top, 0) + " %)";
    if (c.kills < c.deaths) return "NO: more die in it than it kills";
    return "yes";
  }
  if (a.intent == "hold") {
    if (share < 0.08) return "NO: " + to_fixed(100 * share, 1) + " % of the time (8 or more)";
    if (pl.teams && top < 0.6) return "NO: no side holds it (" + to_fixed(100 * top, 0) + " %)";
    return "yes";
  }
  if (a.intent == "fight") {
    if (deaths_min < median_deaths)
      return "NO: " + to_fixed(deaths_min, 2) + " deaths a minute, under the areas' median";
    if (pl.teams && low < 0.3) return "NO: one side keeps out (" + to_fixed(100 * low, 0) + " %)";
    return "yes";
  }
  if (a.intent == "route") {
    if (c.visits / mins < 1) return "NO: under one visit a minute";
    if (stay > 3) return "NO: stays " + to_fixed(stay, 1) + " s (a route is passed in 3)";
    return "yes";
  }
  if (a.intent == "power") return c.visits / mins >= 0.5 ? "yes" : "NO: hardly visited";
  if (a.intent == "perch") {
    if (c.kills == 0 || c.kills < 1.5 * c.deaths) return "NO: it kills too few from up there";
    return "yes";
  }
  return "-";
}

gl::Rgba kIntentColour(const std::string& intent) {
  if (intent == "camp") return {200, 60, 50, 255};
  if (intent == "hold") return {220, 120, 40, 255};
  if (intent == "fight") return {200, 40, 160, 255};
  if (intent == "route") return {60, 120, 210, 255};
  if (intent == "power") return {230, 190, 30, 255};
  if (intent == "perch") return {40, 160, 140, 255};
  return {120, 120, 120, 255};
}

gl::Picture picture(const ge::brush::Map& map, const mc::PlanIntent& in, const Played& pl,
                    const std::vector<Proof>& proofs) {
  const std::vector<shooters_edge::labs::WireEdge> edges = shooters_edge::labs::wire_edges(map);
  shooters_edge::labs::WireView v;
  v.width = 1100;
  v.height = 820;
  const std::vector<sh::Vec3f> pts = shooters_edge::labs::edge_points(edges);
  v.fit(pts);
  gl::Picture p = shooters_edge::labs::wire_picture(edges, v, {250, 249, 245, 255});
  // Where the players were: a dot every fifth point.
  for (size_t k = 0; k < 32; ++k) {
    const gl::Rgba c = pl.team[k] == 1 ? gl::Rgba{60, 100, 220, 40} : gl::Rgba{220, 70, 60, 40};
    for (size_t i = 0; i < pl.tracks[k].size(); i += 5) {
      if (pl.tracks[k][i].dead) continue;
      double x, y, far;
      v.pixel(pl.tracks[k][i].origin, x, y, far);
      gl::fill_disc(p, x, y, 1.5, c);
    }
  }
  for (const cl::DemoKill& k : pl.kills) {
    double x, y, far;
    v.pixel(k.victim_at, x, y, far);
    gl::draw_ring(p, x, y, 3, {30, 30, 30, 200});
  }
  for (const mc::PlanArea& a : in.areas) {
    if (!in_modes(a.modes, pl.mode)) continue;
    const gl::Rgba c = kIntentColour(a.intent);
    double cx = 0, cy = 0;
    for (size_t i = 0; i < a.poly.size(); ++i) {
      const auto& q = a.poly[i];
      const auto& r = a.poly[(i + 1) % a.poly.size()];
      double x0, y0, x1, y1, f;
      v.pixel({float(q.first), float(q.second), float(a.z0)}, x0, y0, f);
      v.pixel({float(r.first), float(r.second), float(a.z0)}, x1, y1, f);
      gl::draw_line(p, x0, y0, x1, y1, c, 2);
      cx += x0 / double(a.poly.size());
      cy += y0 / double(a.poly.size());
    }
    const std::string label = a.name + " (" + a.intent + ")";
    gl::draw_text(p, cx - gl::text_width(label) / 2.0, cy - 4, label, c);
  }
  for (size_t i = 0; i < in.tricks.size(); ++i) {
    const mc::PlanTrick& t = in.tricks[i];
    if (!in_modes(t.modes, pl.mode)) continue;
    double x0, y0, x1, y1, f;
    v.pixel(v3(t.from), x0, y0, f);
    v.pixel(v3(t.to), x1, y1, f);
    const gl::Rgba c = proofs[i].reached ? gl::Rgba{40, 150, 60, 255} : gl::Rgba{210, 40, 40, 255};
    gl::draw_line(p, x0, y0, x1, y1, c, 2);
    gl::fill_disc(p, x1, y1, 4, c);
    gl::draw_text(p, (x0 + x1) / 2 + 4, (y0 + y1) / 2 - 10, t.name + " " + t.kind, c);
  }
  return p;
}

void run(gl::Lab& lab) {
  std::string text = kSample, name = "sample";
  if (const std::string path = lab.arg("plan"); !path.empty()) {
    text = read_text(path);
    if (text.empty()) {
      lab.fail("map-intent: " + path + " does not open", "next: pass --plan=<a .plan file>");
      return;
    }
    const size_t slash = path.find_last_of('/');
    name = path.substr(slash == std::string::npos ? 0 : slash + 1);
    if (const size_t dot = name.rfind('.'); dot != std::string::npos) name.resize(dot);
  }
  mc::Source src;
  mc::PlanIntent in;
  std::string err;
  if (!mc::read_plan(text, &src, &err, &in)) {
    lab.fail("map-intent: the plan does not read:\n" + err, "next: fix the lines it names");
    return;
  }
  mc::CompileOptions opt;
  opt.light = false;
  std::vector<uint8_t> bytes;
  mc::CompileReport rep;
  if (!mc::compile_map(src, opt, &bytes, &rep, &err)) {
    lab.fail("map-intent: the plan does not compile: " + err, "next: fix the plan");
    return;
  }
  ge::brush::Map map;
  sh::BspWorld bsp;
  sh::MoveWorld world;
  if (!ge::brush::read_bsp(bytes, &map, &err) || !sh::bsp_world(map, &bsp, &err)) {
    lab.fail("map-intent: the map does not read back: " + err, "next: run brush_compile_test");
    return;
  }
  sh::map_world(bsp.models[0], world);

  // The tricks, proven.
  std::vector<Proof> proofs;
  gl::LabTable tt{
      name + ": the tricks the plan means, flown on the move",
      {"trick", "kind", "level", "from", "to", "made", "health", "nearest arc ended at"},
      {}};
  int made = 0;
  for (const mc::PlanTrick& t : in.tricks) {
    proofs.push_back(prove(world, t));
    made += proofs.back().reached ? 1 : 0;
    const auto at = [](const double q[3]) {
      return to_fixed(q[0], 0) + " " + to_fixed(q[1], 0) + " " + to_fixed(q[2], 0);
    };
    tt.rows.push_back({t.name + (t.copy ? "'" : ""), t.kind, std::to_string(t.level), at(t.from),
                       at(t.to), proofs.back().reached ? "yes" : "NO",
                       to_fixed(proofs.back().health, 0),
                       proofs.back().miss.empty() ? "-" : proofs.back().miss});
  }
  lab.table(tt);
  lab.metric("tricks", double(in.tricks.size()), "count");
  lab.metric("tricks-made", double(made), "count");

  // The matches.
  std::vector<std::string> modes;
  std::string list = lab.arg("modes").empty() ? in.modes : lab.arg("modes");
  if (list.empty()) list = "4on4";
  for (size_t at = 0; at <= list.size();) {
    const size_t c = list.find(',', at);
    modes.push_back(list.substr(at, c == std::string::npos ? std::string::npos : c - at));
    if (c == std::string::npos) break;
    at = c + 1;
  }
  ge::progs::Progs progs;
  if (!cl::baseline_progs(progs, err)) {
    lab.fail("map-intent: the baseline mod: " + err,
             "next: run tests/shooters_edge_baseline_mod_test.cpp");
    return;
  }
  const double minutes = lab.arg_num("minutes");
  std::vector<Played> played;
  for (const std::string& mode : modes) {
    if (mode == "midair") {
      lab.note("midair: not played; the first game's rules have no midair yet");
      continue;
    }
    cn::TeamMatchOptions o;
    o.map = bytes;
    o.map_name = name;
    o.progs = &progs;
    o.seed = uint32_t(lab.arg_num("seed"));
    o.timelimit = std::max(1, int(std::ceil(minutes)));
    o.frames = int(minutes * 60 * 1000 / 13);
    const sh::AimSkill ex = sh::AimSkill::expert, av = sh::AimSkill::average;
    if (mode == "duel") {
      o.teams[0].skills = {ex};
      o.teams[1].skills = {ex};
      o.settings.push_back("deathmatch 3");
    } else if (mode == "2on2") {
      o.teams[0].skills = {ex, av};
      o.teams[1].skills = {ex, av};
      o.settings.push_back("deathmatch 3");
    } else if (mode == "ffa") {
      o.teams[0].skills = {ex, av, av};
      o.teams[1].skills = {ex, av, av};
      o.settings.push_back("teamplay 0");
    }
    const cn::TeamMatchReport r = cn::run_team_match(o);
    if (!r.ok) {
      lab.fail("map-intent: the " + mode + " match: " + r.error,
               "next: run tests/shooters_edge_team_test.cpp");
      return;
    }
    Played pl;
    pl.mode = mode;
    pl.from = "bots";
    pl.teams = mode != "ffa" && mode != "duel";
    pl.team.fill(-1);
    float frags[2] = {};
    for (const cn::TeamPlayer& p : r.players) {
      if (p.slot >= 0 && p.slot < 32) pl.team[size_t(p.slot)] = p.team;
      frags[p.team] += p.frags;
    }
    pl.score = to_fixed(frags[0], 0) + " to " + to_fixed(frags[1], 0);
    if (!count(r.mvd, in, pl, err)) {
      std::string why = "map-intent: the " + mode;
      why += " recording: ";
      why += err;
      lab.fail(why, "next: run se-demo");
      return;
    }
    if (!lab.arg("save").empty()) {
      std::string file = name + "-";
      file += mode;
      file += ".mvd";
      lab.attach(file, r.mvd);
    }
    played.push_back(std::move(pl));
  }
  for (size_t at = 0; !lab.arg("demos").empty() && at <= lab.arg("demos").size();) {
    const std::string& all = lab.arg("demos");
    const size_t c = all.find(',', at);
    const std::string path = all.substr(at, c == std::string::npos ? std::string::npos : c - at);
    std::ifstream f(path, std::ios::binary);
    std::vector<uint8_t> b{std::istreambuf_iterator<char>(f), {}};
    Played pl;
    pl.mode = modes.empty() ? "4on4" : modes.front();
    pl.from = path;
    pl.teams = false;
    pl.team.fill(-1);
    if (b.empty() || !count(std::move(b), in, pl, err)) {
      std::string why = "map-intent: " + path;
      why += " does not read: ";
      why += err;
      lab.fail(why, "next: pass a recording");
      return;
    }
    played.push_back(std::move(pl));
    if (c == std::string::npos) break;
    at = c + 1;
  }

  int as_meant = 0, judged = 0;
  for (const Played& pl : played) {
    double total = 0;
    for (size_t k = 0; k < 32; ++k) {
      for (size_t i = 1; i < pl.tracks[k].size(); ++i) {
        if (!pl.tracks[k][i].dead)
          total += std::min(0.2, pl.tracks[k][i].t - pl.tracks[k][i - 1].t);
      }
    }
    std::vector<double> dm;
    for (size_t a = 0; a < in.areas.size(); ++a) {
      if (in_modes(in.areas[a].modes, pl.mode))
        dm.push_back(pl.areas[a].deaths / std::max(pl.minutes, 1e-6));
    }
    std::sort(dm.begin(), dm.end());
    const double median = dm.empty() ? 0 : dm[dm.size() / 2];
    gl::LabTable at{name + ", " + pl.mode + " (" + pl.from + ", " + to_fixed(pl.minutes, 1) +
                        " min" + (pl.score.empty() ? "" : ", " + pl.score) +
                        "): each area against its intent",
                    {"area", "intent", "time %", "visits/min", "stay s", "red %", "deaths/min",
                     "kills/min", "as meant"},
                    {}};
    for (size_t a = 0; a < in.areas.size(); ++a) {
      const mc::PlanArea& ar = in.areas[a];
      if (!in_modes(ar.modes, pl.mode)) continue;
      const AreaCount& c = pl.areas[a];
      const double mins = std::max(pl.minutes, 1e-6);
      const std::string v = judge(ar, c, pl, total, median);
      if (ar.intent != "spawn") {
        ++judged;
        as_meant += v == "yes" ? 1 : 0;
      }
      at.rows.push_back({ar.name + (ar.copy ? "'" : ""), ar.intent,
                         to_fixed(total > 0 ? 100 * c.time / total : 0, 1),
                         to_fixed(c.visits / mins, 1),
                         to_fixed(c.visits ? c.time / c.visits : 0, 1),
                         pl.teams && c.time > 0 ? to_fixed(100 * c.team_time[0] / c.time, 0) : "-",
                         to_fixed(c.deaths / mins, 2), to_fixed(c.kills / mins, 2), v});
    }
    lab.table(at);
    if (pl.from == "bots") {
      gl::LabTable tk{name + ", " + pl.mode + ": how often the bots made each trick",
                      {"trick", "kind", "made"},
                      {}};
      for (size_t i = 0; i < in.tricks.size(); ++i) {
        if (!in_modes(in.tricks[i].modes, pl.mode)) continue;
        tk.rows.push_back({in.tricks[i].name + (in.tricks[i].copy ? "'" : ""), in.tricks[i].kind,
                           std::to_string(pl.trick_takes[i])});
      }
      lab.table(tk);
      lab.shot({name + "-" + pl.mode,
                name + ": " + pl.mode + " against the plan's intent",
                "The map from above (its faces' outlines), every bot's track as faint dots in "
                "its side's colour, each death a dark ring, each area outlined in its intent's "
                "colour (camp red, hold orange, fight magenta, route blue, power gold, perch "
                "teal), each trick an arrow, green where the move made it and red where not.",
                "the dots thick where the plan holds or fights, thin on its routes; the rings "
                "where it means the fights; every arrow green",
                {{"mode", pl.mode}, {"minutes", to_fixed(pl.minutes, 1)}}},
               picture(map, in, pl, proofs));
    }
  }
  lab.metric("areas-judged", double(judged), "count");
  lab.metric("areas-as-meant", double(as_meant), "count");
}

const gl::LabSpec kSpec{
    "map-intent",
    gl::LabKind::scenario,
    "game:shooters_edge",
    "a map plan's areas and tricks (plugins/brush/compile/plan.hpp) against the move's proofs "
    "and bots' matches (games/shooters_edge/net/classic_team.hpp, client/demo_scan.hpp)",
    "map-design",
    "Does a map play as its plan says it is meant to: is every trick possible, are its camps "
    "camped, its fights fought and its routes run through?",
    {
        {"plan", "", "the map plan (empty: the lab's sample)"},
        {"modes", "", "the modes played, comma separated (empty: the plan's `modes`)"},
        {"minutes", "3", "each match's length"},
        {"seed", "1", "the matches' seed"},
        {"demos", "", "recordings of other matches on the map to table too, comma separated"},
        {"save", "", "1: attach each match's recording"},
    },
    "--minutes=0.3 --modes=duel",
    run,
};
GE_LAB(kSpec);

}  // namespace
