// LAB `se-aim-spread`: the aim spread (plugins/shooter/aim_spread.hpp), the bot-likeness number
// the classic server keeps for each player and the match report carries. Two parts.
//
// SHOOTERS ON A RUNNING TARGET: a target strafes left and right at running speed, walking away
// and back between 200 and 1200 units, and four shooters fire at it every frame: an aim bot on
// the head, an aim bot on the middle, a hand on a mouse (the view chases the target a reaction
// late, drifting off and pulled back) and a flicker (each burst at a new spot on the box). Each
// line's crossings are drawn on the box, edge to edge, and its spread is tabled. The lab fails if
// an aim bot is not tight (spread under 0.1) or a hand is (over 0.3).
//
// A BOT MATCH: the server's own bots play a match on the arena map and the report's
// `aim_spread_min` column is read back for each, the number a hub would show beside the player.
//
// ge:summary Lab: the aim spread of aim bots against hands, and of the server's bots in a match
// ge:keywords lab aim-spread aimbot bot-likeness anti-cheat match-report column
// ge:skill shooter net labs
// ge:test tests/shooter_aim_spread_test.cpp
#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

#include "core/prng.hpp"
#include "lab/chart.hpp"
#include "lab/lab.hpp"
#include "net/report.hpp"
#include "server/server.hpp"
#include "shooter/aim_spread.hpp"
#include "shooters_edge/net/qw_game.hpp"
#include "shooters_edge/net/qw_report.hpp"

namespace {

namespace gl = ge::lab;
namespace gn = ge::net;
namespace gs = ge::server;
namespace sh = ge::shooter;
namespace se = shooters_edge::net;

std::string fmt(double v, int digits) {
  char b[32];
  std::snprintf(b, sizeof b, "%.*f", digits, v);
  return b;
}

sh::Vec3f unit(sh::Vec3f v) {
  sh::normalize(v);
  return v;
}

enum class Shooter : uint8_t { head, middle, mouse, flick, count };
const char* shooter_name(Shooter s) {
  switch (s) {
    case Shooter::head:
      return "aim bot, head";
    case Shooter::middle:
      return "aim bot, middle";
    case Shooter::mouse:
      return "hand on a mouse";
    case Shooter::flick:
      return "flicker";
    case Shooter::count:
      break;
  }
  return "?";
}

// The target's spot (its origin, the box -16..16 by -24..32 about it) at time t, s.
sh::Vec3f target_at(double t) {
  const double phase = t / 3.0 - static_cast<double>(static_cast<int64_t>(t / 3.0));  // 0..1
  const double tri = phase < 0.5 ? phase * 2 : 2 - phase * 2;
  const double away = 200 + 1000 * tri;
  // Strafing: 320 units a second, turning about every 0.7 s.
  const double leg = t / 0.7 - static_cast<double>(static_cast<int64_t>(t / 0.7));
  const auto legs = static_cast<int64_t>(t / 0.7);
  const double across = (legs % 2 == 0 ? leg : 1 - leg) * 224 - 112;
  return {static_cast<float>(away), static_cast<float>(across), 24};
}

struct Fired {
  sh::AimSpread spread;
  std::vector<double> x, y;  // where the line crossed, half-boxes
};

void shooters(gl::Lab& lab, double seconds, uint32_t seed) {
  const sh::Vec3f eye{0, 0, 46};
  const double dt = 0.013;
  const int steps = static_cast<int>(seconds / dt);
  Fired fired[static_cast<int>(Shooter::count)];
  ge::core::Rng rng = ge::core::make_rng(seed);
  sh::Vec3f drift{0, 0, 0};  // the mouse hand's own wander off the target, units
  sh::Vec3f flick{0, 0, 0};
  for (int i = 0; i < steps; i++) {
    const double t = i * dt;
    const sh::Vec3f at = target_at(t);
    const sh::AimBox box{at + sh::Vec3f{-16, -16, -24}, at + sh::Vec3f{16, 16, 32}};
    // The hand: where the target was a reaction ago, plus a wander pulled back toward it.
    drift = drift * 0.96f + sh::Vec3f{0, static_cast<float>(ge::core::range(rng, -3, 3)),
                                      static_cast<float>(ge::core::range(rng, -2.5, 2.5))};
    if (i % 40 == 0)  // each burst of the flicker lands at a new spot
      flick = {0, static_cast<float>(ge::core::range(rng, -15, 15)),
               static_cast<float>(ge::core::range(rng, -22, 30))};
    for (int k = 0; k < static_cast<int>(Shooter::count); k++) {
      sh::Vec3f aim;
      switch (static_cast<Shooter>(k)) {
        case Shooter::head:
          aim = at + sh::Vec3f{0, 0, 26};
          break;
        case Shooter::middle:
          aim = at + sh::Vec3f{0, 0, 4};
          break;
        case Shooter::mouse:
          aim = target_at(t - 0.12 < 0 ? 0 : t - 0.12) + sh::Vec3f{0, 0, 8} + drift;
          break;
        default:
          aim = at + flick;
          break;
      }
      const sh::Vec3f forward = unit(aim - eye);
      Fired& f = fired[k];
      if (!sh::aim_spread_sample(f.spread, eye, forward, {&box, 1}) || f.x.size() > 4000) continue;
      // The crossing drawn: the same measure the sample took.
      sh::Vec3f side{forward.y, -forward.x, 0};
      sh::normalize(side);
      const sh::Vec3f mid = (box.lo + box.hi) * 0.5f;
      const sh::Vec3f miss = eye + forward * sh::dot(mid - eye, forward) - mid;
      f.x.push_back(sh::dot(miss, side) / 16.0);
      f.y.push_back(miss.z / 28.0);
    }
  }
  gl::LabTable table{"Each shooter on the running target: its samples and its aim spread",
                     {"shooter", "samples", "spread", "reads as"},
                     {}};
  gl::LineChart chart{
      {"WHERE EACH SHOOTER'S LINE CROSSED THE TARGET", "",
       "across, half-boxes (-1 left edge, 1 right)", "up, half-boxes (-1 feet, 1 head)"},
      {},
      {},
      {},
      {},
      false,
      true,
      false};
  for (int k = 0; k < static_cast<int>(Shooter::count); k++) {
    const auto who = static_cast<Shooter>(k);
    const Fired& f = fired[k];
    const double rms = sh::aim_spread_rms(f.spread);
    const bool bot = who == Shooter::head || who == Shooter::middle;
    table.rows.push_back({shooter_name(who), std::to_string(f.spread.samples), fmt(rms, 3),
                          rms >= 0 && rms < 0.15 ? "held on a point" : "a hand's"});
    chart.traces.push_back({shooter_name(who), f.x, f.y});
    std::string key = "se-aim-spread-";
    key += shooter_name(who);
    lab.metric(key, rms, "half-box");
    if (bot && !(rms >= 0 && rms < 0.1))
      lab.fail(std::string("the ") + shooter_name(who) + " is not tight: spread " + fmt(rms, 3),
               "Read aim_spread_sample in plugins/shooter/aim_spread.cpp; skill: shooter.");
    if (!bot && !(rms > 0.3))
      lab.fail(std::string("the ") + shooter_name(who) + " reads as held: spread " + fmt(rms, 3),
               "Read aim_spread_rms in plugins/shooter/aim_spread.cpp; skill: shooter.");
  }
  lab.table(std::move(table));
  chart.frame.subtitle = "a target strafing at 320 u/s, 200 to 1200 units off";
  const gl::Chart drawn = gl::line_chart(chart);
  lab.shot({"crossings",
            drawn.title,
            "Where each shooter's line crossed the target's box while firing, as a share of the "
            "box from its middle, so every range is drawn on one box.",
            "each aim bot a single dot (the head's high, the middle's at the centre) whatever the "
            "range and the strafe; the mouse hand a cloud wandering about the chest; the flicker "
            "a scatter of tight knots, one a burst, over the whole box.",
            {{"seconds", lab.arg("seconds")}, {"seed", lab.arg("seed")}}},
           gl::to_picture(drawn.plot));
}

void bot_match(gl::Lab& lab, double seconds, uint32_t seed) {
  gs::ServerConfig c;
  c.hostname = "Spread Night";
  c.map = "arena";
  c.maxclients = 8;
  c.bots = 3;
  c.seed = seed;
  c.rules["timelimit"] = fmt(seconds / 60.0, 3);
  gs::Server s;
  std::string error;
  if (!gs::server_open(s, se::qw_game(), c, nullptr, nullptr, error)) {
    lab.fail("the bot match did not open: " + error,
             "Read server_open in engine/server/server.cpp; skill: net.");
    return;
  }
  const int frames = static_cast<int>((seconds + 14) * 77);
  for (int f = 0; f < frames && s.reports.empty(); f++) gs::server_frame(s);
  if (s.reports.empty()) {
    lab.fail("the bot match left no report",
             "Read end_match in games/shooters_edge/net/qw_game.cpp; skill: net.");
    return;
  }
  const gn::MatchReport& r = s.reports.back();
  size_t col = r.columns.size();
  for (size_t i = 0; i < r.columns.size(); i++)
    if (r.columns[i] == se::kAimSpreadColumn) col = i;
  gl::LabTable table{"The server's bots in a match: the report's aim spread for each",
                     {"player", "frags", "deaths", "aim spread"},
                     {}};
  for (const gn::ReportPlayer& p : r.players) {
    const int64_t v = col < p.columns.size() ? p.columns[col] : -2;
    table.rows.push_back({p.name, std::to_string(p.frags), std::to_string(p.deaths),
                          v < 0 ? "too few shots" : fmt(static_cast<double>(v) / 1000.0, 3)});
    lab.metric("se-aim-spread-match-" + p.name, static_cast<double>(v), "thousandths");
  }
  if (col == r.columns.size())
    lab.fail("the report has no aim_spread_min column",
             "Read tally_end in games/shooters_edge/net/qw_report.cpp; skill: net.");
  lab.table(std::move(table));
}

void run(gl::Lab& lab) {
  const double seconds = lab.arg_num("seconds");
  const auto seed = static_cast<uint32_t>(lab.arg_num("seed"));
  if (seconds < 10) {
    lab.fail("--seconds is under 10", "Pass --seconds=10 or more; skill: shooter.");
    return;
  }
  shooters(lab, seconds, seed);
  bot_match(lab, seconds, seed);
  lab.note(
      "spread: the root mean square distance of the crossings from their own mean point, in "
      "half-boxes (a line falling evenly over the box is about 0.8; one held on a point near 0). "
      "Under 30 crossings it says nothing. It is a number to read beside the player, never a "
      "ban: a steady hand at long range is tight too.");
}

const gl::LabSpec kSpec{
    "se-aim-spread",
    gl::LabKind::sim,
    "game:shooters_edge",
    "plugins/shooter/aim_spread (the aim spread), its column in the classic match report",
    "shooter",
    "Does an aim bot's line sit on one point of its target while a hand's falls all over it, "
    "and what do the server's bots read as in a match?",
    {
        {"seconds", "40", "how long the shooters fire and the match lasts"},
        {"seed", "3", "the hands' stream and the match's"},
    },
    "--seconds=20",
    run,
};
GE_LAB(kSpec);

}  // namespace
