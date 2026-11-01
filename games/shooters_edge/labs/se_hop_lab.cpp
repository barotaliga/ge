// LAB `se-hop`: DO THE BOTS GET ROUND A REAL MAP AS FAST AS A PRO, and where does the pro know a
// way the bots do not? A map (`--map=<a .bsp>`, or `--basedir` and `--mapname`, or the fixture
// halls) gets the bots' nav graph (shooter/nav.hpp), and a set of TRIPS is run on it by the pilot
// (games/shooters_edge/bots/pilot_run.hpp `run_route`, no server) five ways: walking (no strafe,
// no hop) and strafing (today's straight strafe jumps) with a person's hands, and hopping as an
// expert (shooter/pilot.hpp `hop`) with practised hands, with a person's late hands and with none.
// Every run is read by the same hop reader as a recorded player (shooter/hops.hpp), so a bot's hops
// and a pro's sit in one table.
//
// THE TRIPS come off a recorded player when `--file` (a demo) is given: `--player`'s track cut
// into stretches of up to `--trip-s` seconds over which he crossed at least `--trip-min` units
// with nothing cut (no death, no teleport), the longest `--trips` kept; his own time on each is
// beside the bots', and every way starts at his velocity there. Without a demo they are the map's
// far corners, between nav nodes.
//
// A way's TIME on a trip is to its first pass within 48 units of the trip's end (a recorded
// player's trip ends mid-run too, so a bot is not timed slowing to stand on a node).
//
// A TRICK CANDIDATE is a trip where the player beat the expert's hop by more than `--trick`
// (his time under that share of the bot's), or the bots found no way at all, or the route the
// bots plan is over 1.5 times his path's length: he knows a way (a jump, a drop, a shortcut)
// the graph does not. Each is listed with why, and the first ones drawn close up.
//
// What it shows: the trips' table (each way's time, arrival, top speed and chained hops), the
// methods' summary (the hop reader's numbers for each way and the player), the trick candidates,
// every trip over the map's wireframe from above (his path and the expert's), the first trips
// close up with every way's path, one trip from inside the map, and a trip's speed against the
// distance covered for every way. Deterministic: the digest holds every run.
//
// ge:summary Lab: the pilot walking, strafing and hopping on a real map's trips against a pro
// ge:keywords lab se-hop bunny-hop expert pilot trips pro compare tricks shortcuts nav wireframe
// ge:keywords hop-mode chained speed time real-map
// ge:skill shooter labs
// ge:test tests/shooter_pilot_test.cpp
#include <algorithm>
#include <cctype>
#include <chrono>
#include <cmath>
#include <string>
#include <vector>

#include "core/libm/libm.hpp"
#include "core/math.hpp"
#include "core/num_text.hpp"
#include "lab/chart.hpp"
#include "lab/lab.hpp"
#include "shooter/hops.hpp"
#include "shooter/nav.hpp"
#include "shooter/pilot.hpp"
#include "shooters_edge/bots/pilot_run.hpp"
#include "shooters_edge/labs/move_study.hpp"
#include "shooters_edge/labs/wire_inside.hpp"
#include "shooters_edge/labs/wire_view.hpp"

namespace {

namespace gl = ge::lab;
namespace sh = ge::shooter;
namespace la = shooters_edge::labs;
namespace bo = shooters_edge::bots;
using ge::core::num::to_fixed;

using In = std::vector<std::pair<std::string, std::string>>;

constexpr double kHz = 1000.0 / bo::kPilotMsec;

struct Way {
  const char* name;
  gl::Rgba colour;
  sh::PilotHands hands;
  bool strafe, hop;
};
constexpr Way kWays[] = {
    {"walk", {150, 150, 150, 255}, sh::PilotHands::human, false, false},
    {"strafe", {40, 90, 220, 255}, sh::PilotHands::human, true, false},
    {"hop", {240, 120, 20, 255}, sh::PilotHands::practised, true, true},
    {"hop-late", {200, 60, 60, 255}, sh::PilotHands::human, true, true},
    {"hop-sharp", {20, 160, 90, 255}, sh::PilotHands::none, true, true},
};
constexpr size_t kWayCount = std::size(kWays);
constexpr gl::Rgba kPro{170, 30, 170, 255};
constexpr float kPass = 48;  // a trip is done passing within this across of its end (and 40 up)

struct Trip {
  sh::Vec3f from, to;
  sh::Vec3f velocity;      // his velocity at the start: every way starts moving at it
  double pro_s = -1;       // the player's own time (-1: none)
  float pro_length = 0;    // his path's length across, units
  int32_t f0 = 0, f1 = 0;  // his frames
  float route_length = 0;  // the bots' first planned route, across
  bo::RunResult runs[kWayCount];
  float pass_s[kWayCount] = {};  // each way's time to first pass within kPass of the end (-1 never)
  sh::HopReading reads[kWayCount];
  std::string trick;  // why it is a trick candidate ("" none)
};

float across(sh::Vec3f a, sh::Vec3f b) {
  return static_cast<float>(ge::core::hypot(b.x - a.x, b.y - a.y));
}

std::string lower(std::string s) {
  for (char& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  return s;
}

std::vector<sh::TrackPoint> run_track(const bo::RunResult& r) {
  std::vector<sh::TrackPoint> t;
  for (size_t i = 0; i < r.trail.size(); i++)
    t.push_back({static_cast<double>(i) / kHz, r.trail[i], static_cast<float>(r.yaw[i]), 0, false});
  return t;
}

void run(gl::Lab& lab) {
  std::string error;
  la::StudyMap m;
  if (!la::load_study_map(lab.arg("map"), lab.arg("basedir"), lab.arg("game"), lab.arg("mapname"),
                          m, error)) {
    lab.fail("se-hop: " + error, "next: make lab LAB=se-hop ARGS=--help; skill: shooter");
    return;
  }
  const auto t0 = std::chrono::steady_clock::now();
  sh::NavGraph graph;
  sh::build_nav(*m.world, {}, sh::MoveSettings{}, sh::NavSettings{}, graph);
  lab.metric("nav.build-wall",
             std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count(), "s",
             gl::Det::measured);
  lab.metric("nav.nodes", static_cast<double>(graph.nodes.size()), "count");
  if (graph.nodes.empty()) {
    lab.fail("se-hop: the nav graph of " + m.name + " has no nodes",
             "next: run the se-nav lab; skill: shooter");
    return;
  }

  // The trips.
  std::vector<Trip> trips;
  sh::HopReading pro;
  std::string who;
  const std::string file = lab.arg("file");
  sh::HopSettings hs;
  if (!file.empty()) {
    la::DemoTracks dt;
    if (!la::read_demo_tracks(la::read_whole(file), dt, error)) {
      lab.fail("se-hop: the demo " + file + ": " + error, "next: run the se-moves lab on it");
      return;
    }
    int me = -1;
    for (int k = 0; k < 32; k++) {
      if (dt.tracks[static_cast<size_t>(k)].size() < 3) continue;
      const std::string& n = dt.names[static_cast<size_t>(k)];
      if (lab.arg("player").empty()
              ? (me < 0 || dt.frags[static_cast<size_t>(k)] > dt.frags[static_cast<size_t>(me)])
              : (lower(n) == lower(lab.arg("player")) || std::to_string(k) == lab.arg("player")))
        me = k;
    }
    if (me < 0) {
      lab.fail("se-hop: no player " + lab.arg("player") + " in the demo",
               "next: run se-moves on it for the players' names");
      return;
    }
    who = dt.names[static_cast<size_t>(me)];
    sh::read_hops(dt.tracks[static_cast<size_t>(me)], *m.world, hs, pro);
    const double span = lab.arg_num("trip-s");
    const float least = static_cast<float>(lab.arg_num("trip-min"));
    struct Cand {
      int32_t a, b;
      float d;
    };
    std::vector<Cand> cands;
    const auto& fr = pro.frames;
    for (size_t i = 0; i < fr.size();) {
      if (fr[i].phase == sh::HopPhase::cut || fr[i].phase != sh::HopPhase::ground) {
        i++;
        continue;
      }
      size_t j = i, best = i;
      while (j + 1 < fr.size() && fr[j + 1].phase != sh::HopPhase::cut &&
             fr[j + 1].t - fr[i].t <= span) {
        j++;
        if (fr[j].phase == sh::HopPhase::ground &&
            across(fr[i].origin, fr[j].origin) >= across(fr[i].origin, fr[best].origin))
          best = j;
      }
      const float d = across(fr[i].origin, fr[best].origin);
      if (d >= least) {
        cands.push_back({static_cast<int32_t>(i), static_cast<int32_t>(best), d});
        i = best + 1;
      } else {
        i += 10;
      }
    }
    std::stable_sort(cands.begin(), cands.end(),
                     [](const Cand& x, const Cand& y) { return x.d > y.d; });
    const auto want = static_cast<size_t>(lab.arg_num("trips"));
    for (size_t k = 0; k < cands.size() && trips.size() < want; k++) {
      Trip t;
      t.f0 = cands[k].a;
      t.f1 = cands[k].b;
      t.from = fr[static_cast<size_t>(t.f0)].origin;
      t.to = fr[static_cast<size_t>(t.f1)].origin;
      t.velocity = fr[static_cast<size_t>(t.f0)].velocity;
      t.velocity.z = 0;
      t.pro_s = fr[static_cast<size_t>(t.f1)].t - fr[static_cast<size_t>(t.f0)].t;
      for (int32_t i = t.f0 + 1; i <= t.f1; i++)
        t.pro_length +=
            across(fr[static_cast<size_t>(i - 1)].origin, fr[static_cast<size_t>(i)].origin);
      trips.push_back(t);
    }
    std::sort(trips.begin(), trips.end(), [](const Trip& x, const Trip& y) { return x.f0 < y.f0; });
  } else {
    // The far corners: the nodes nearest the graph's bounds' corners, crossed both ways.
    sh::Vec3f lo = graph.nodes[0].origin, hi = lo;
    for (const sh::NavNode& n : graph.nodes) {
      lo = {std::min(lo.x, n.origin.x), std::min(lo.y, n.origin.y), std::min(lo.z, n.origin.z)};
      hi = {std::max(hi.x, n.origin.x), std::max(hi.y, n.origin.y), std::max(hi.z, n.origin.z)};
    }
    auto near = [&](float x, float y) {
      int32_t best = 0;
      float bd = 1e30f;
      for (size_t i = 0; i < graph.nodes.size(); i++) {
        const float d = across(graph.nodes[i].origin, {x, y, 0});
        if (d < bd) bd = d, best = static_cast<int32_t>(i);
      }
      return graph.nodes[static_cast<size_t>(best)].origin;
    };
    const sh::Vec3f c[4] = {near(lo.x, lo.y), near(hi.x, hi.y), near(lo.x, hi.y), near(hi.x, lo.y)};
    for (int k = 0; k < 4; k += 2) {
      Trip t;
      t.from = c[k];
      t.to = c[k + 1];
      trips.push_back(std::move(t));
    }
  }
  if (trips.empty()) {
    lab.fail("se-hop: no trip found", "next: lower --trip-min or raise --trip-s");
    return;
  }

  // Every way on every trip.
  hs.cmd_hz = kHz;
  const auto seed = static_cast<uint32_t>(lab.arg_num("seed"));
  const float limit = static_cast<float>(lab.arg_num("limit-s"));
  const float trick = static_cast<float>(lab.arg_num("trick"));
  for (Trip& t : trips) {
    for (size_t w = 0; w < kWayCount; w++) {
      sh::PilotSettings ps;
      ps.strafe = kWays[w].strafe;
      ps.hop = kWays[w].hop;
      t.runs[w] =
          bo::run_route(*m.world, graph, t.from, t.to, kWays[w].hands, seed, ps, limit, t.velocity);
      const std::vector<sh::TrackPoint> tr = run_track(t.runs[w]);
      sh::read_hops(tr, *m.world, hs, t.reads[w]);
      t.pass_s[w] = -1;
      for (size_t i = 0; i < tr.size(); i++)
        if (across(tr[i].origin, t.to) <= kPass && std::fabs(tr[i].origin.z - t.to.z) <= 40) {
          t.pass_s[w] = static_cast<float>(tr[i].t);
          break;
        }
      if (lab.arg_bool("trace")) {
        // Every command of the run: what a reader charts (ge graph) to see a hop go wrong.
        std::string tsv =
            "t\tmode\tx\ty\tz\tspeed\tphase\tyaw\theading\tside\tforward\tyaw_wanted\n";
        const bo::RunResult& r = t.runs[w];
        for (size_t i = 0; i < r.trail.size(); i++) {
          const sh::HopFrame& f = t.reads[w].frames[i];
          tsv += to_fixed(f.t, 3) + "\t" +
                 sh::pilot_mode_name(static_cast<sh::PilotMode>(r.modes[i])) + "\t" +
                 to_fixed(r.trail[i].x, 1) + "\t" + to_fixed(r.trail[i].y, 1) + "\t" +
                 to_fixed(r.trail[i].z, 1) + "\t" + to_fixed(f.speed, 0) + "\t" +
                 std::to_string(static_cast<int>(f.phase)) + "\t" + to_fixed(r.yaw[i], 1) + "\t" +
                 to_fixed(f.heading, 1) + "\t" + to_fixed(r.side[i], 2) + "\t" +
                 to_fixed(r.forward[i], 2) + "\t" + to_fixed(r.yaw_wanted[i], 1) + "\n";
        }
        lab.attach("trip-" + std::to_string(&t - trips.data()) + "-" + kWays[w].name + ".tsv",
                   std::vector<uint8_t>(tsv.begin(), tsv.end()));
      }
    }
    const bo::RunResult& h = t.runs[2];
    for (size_t i = 1; i < h.route.size(); i++)
      t.route_length += across(graph.nodes[static_cast<size_t>(h.route[i - 1])].origin,
                               graph.nodes[static_cast<size_t>(h.route[i])].origin);
    if (t.pro_s > 0) {
      if (h.route.empty())
        t.trick = "no way on the bots' graph";
      else if (t.route_length > 1.5f * t.pro_length)
        t.trick = "the bots' route is " + to_fixed(t.route_length / t.pro_length, 1) + "x his path";
      else if (t.pass_s[2] < 0)
        t.trick = "the hop did not get there";
      else if (t.pro_s < trick * t.pass_s[2])
        t.trick = "he is " + to_fixed(t.pass_s[2] / t.pro_s, 2) + "x faster than the hop";
    }
  }

  const In in = {{"map", m.name + " (" + m.from + ")"},
                 {"file", file.empty() ? "(none: the far corners)" : file},
                 {"player", who},
                 {"trips", std::to_string(trips.size())},
                 {"seed", lab.arg("seed")}};

  // The trips' table.
  std::vector<std::string> cols = {"trip", "from", "to", "pro s", "pro length", "route length"};
  for (const Way& w : kWays) cols.push_back(std::string(w.name) + " s");
  cols.push_back("hop top u/s");
  cols.push_back("hop chained");
  cols.push_back("trick?");
  gl::LabTable tt{"the trips: every way's time to pass the end (- never)", cols, {}};
  auto pt = [](sh::Vec3f p) { return to_fixed(p.x, 0) + " " + to_fixed(p.y, 0); };
  double sum[kWayCount] = {}, pro_sum = 0;
  int all = 0;
  for (size_t k = 0; k < trips.size(); k++) {
    const Trip& t = trips[k];
    std::vector<std::string> row = {std::to_string(k),
                                    pt(t.from),
                                    pt(t.to),
                                    t.pro_s > 0 ? to_fixed(t.pro_s, 2) : "-",
                                    to_fixed(t.pro_length, 0),
                                    to_fixed(t.route_length, 0)};
    bool every = t.pro_s > 0 && t.trick.empty();
    for (size_t w = 0; w < kWayCount; w++) {
      row.push_back(t.pass_s[w] >= 0 ? to_fixed(t.pass_s[w], 2) : "-");
      every = every && t.pass_s[w] >= 0;
      lab.metric("trip-" + std::to_string(k) + "." + kWays[w].name + "-s", t.pass_s[w], "s");
    }
    if (every) {
      all++;
      pro_sum += t.pro_s;
      for (size_t w = 0; w < kWayCount; w++) sum[w] += t.pass_s[w];
    }
    row.push_back(to_fixed(t.runs[2].top_speed, 0));
    row.push_back(std::to_string(t.reads[2].summary.chained));
    row.push_back(t.trick.empty() ? "" : t.trick);
    tt.rows.push_back(row);
  }
  lab.table(tt);
  if (all > 0) {
    lab.metric("trips.compared", all, "count");
    if (pro_sum > 0) lab.metric("trips.pro-s", pro_sum, "s");
    for (size_t w = 0; w < kWayCount; w++)
      lab.metric(std::string("trips.") + kWays[w].name + "-s", sum[w], "s");
  }

  // The hop reader on every way and the player.
  gl::LabTable st{"the hop reader on every way (all trips) and the player (his whole match)",
                  {"who", "jumps", "chained %", "longest chain", "air %", "speed p50", "p90", "top",
                   "turn share", "lag deg", "gain u/s/s"},
                  {}};
  auto srow = [&](const std::string& name, const sh::HopSummary& s) {
    st.rows.push_back(
        {name, std::to_string(s.jumps), to_fixed(s.jumps > 0 ? 100.0 * s.chained / s.jumps : 0, 1),
         std::to_string(s.longest_chain), to_fixed(100.0 * s.air_share, 1),
         to_fixed(s.speed_p50, 0), to_fixed(s.speed_p90, 0), to_fixed(s.speed_top, 0),
         to_fixed(s.turn_share_p50, 2), to_fixed(s.lag_p50, 1), to_fixed(s.gain_per_s, 1)});
  };
  if (!who.empty()) srow(who, pro.summary);
  for (size_t w = 0; w < kWayCount; w++) {
    // All trips' runs end to end, cut between trips.
    std::vector<sh::TrackPoint> all_t;
    double t_off = 0;
    for (const Trip& t : trips) {
      std::vector<sh::TrackPoint> tr = run_track(t.runs[w]);
      for (sh::TrackPoint& p : tr) p.t += t_off;
      if (!tr.empty()) t_off = tr.back().t + 1;
      all_t.insert(all_t.end(), tr.begin(), tr.end());
    }
    sh::HopReading r;
    sh::read_hops(all_t, *m.world, hs, r);
    srow(std::string("bot ") + kWays[w].name, r.summary);
    lab.metric(std::string("way.") + kWays[w].name + ".chained", r.summary.chained, "count");
    lab.metric(std::string("way.") + kWays[w].name + ".speed-p90", r.summary.speed_p90, "u/s");
  }
  lab.table(st);

  // The trick candidates.
  gl::LabTable tk{"trick candidates: where he knows a faster way", {"trip", "t s", "why"}, {}};
  for (size_t k = 0; k < trips.size(); k++)
    if (!trips[k].trick.empty())
      tk.rows.push_back({std::to_string(k),
                         to_fixed(pro.frames[static_cast<size_t>(trips[k].f0)].t, 1),
                         trips[k].trick});
  lab.metric("tricks", static_cast<double>(tk.rows.size()), "count");
  if (!tk.rows.empty()) lab.table(tk);

  // Pictures.
  const std::vector<la::WireEdge> edges = la::wire_edges(m.map);
  const std::vector<sh::Vec3f> map_pts = la::edge_points(edges);
  auto pro_path = [&](const Trip& t) {
    std::vector<sh::Vec3f> v;
    for (int32_t i = t.f0; i <= t.f1; i++) v.push_back(pro.frames[static_cast<size_t>(i)].origin);
    return v;
  };
  auto line = [](gl::Picture& p, const la::WireView& v, const std::vector<sh::Vec3f>& pts,
                 gl::Rgba c, double th) {
    for (size_t i = 1; i < pts.size(); i++) {
      double ax, ay, af, bx, by, bf;
      v.pixel(pts[i - 1], ax, ay, af);
      v.pixel(pts[i], bx, by, bf);
      gl::draw_line(p, ax, ay, bx, by, c, th);
    }
  };
  auto legend = [&](gl::Picture& p) {
    int x = 10;
    if (!who.empty()) {
      gl::fill_rect(p, x, 6, 14, 6, kPro);
      gl::draw_text(p, x + 18, 5, who, {30, 30, 30, 255});
      x += 30 + gl::text_width(who);
    }
    for (const Way& w : kWays) {
      gl::fill_rect(p, x, 6, 14, 6, w.colour);
      gl::draw_text(p, x + 18, 5, w.name, {30, 30, 30, 255});
      x += 30 + gl::text_width(w.name);
    }
  };
  if (lab.wants("trips-top")) {
    la::WireView v;
    v.fit(map_pts);
    gl::Picture p = la::wire_picture(edges, v);
    for (size_t k = 0; k < trips.size(); k++) {
      const Trip& t = trips[k];
      if (t.pro_s > 0) line(p, v, pro_path(t), kPro, 2);
      line(p, v, t.runs[2].trail, kWays[2].colour, 2);
      double x, y, f;
      v.pixel(t.from, x, y, f);
      gl::draw_text(p, x + 4, y - 4, std::to_string(k), {0, 0, 0, 255}, 2);
    }
    legend(p);
    lab.shot({"trips-top", "THE TRIPS ON " + m.name + " FROM ABOVE",
              "Every trip over the map's wireframe: " +
                  (who.empty() ? std::string("") : who + "'s own path (purple) and ") +
                  "the expert's hop (orange, late hands), each trip's number at its start.",
              "the orange weaves round the purple; a trip with no orange is one the bots found no "
              "way along",
              in},
             p);
  }
  const auto close = static_cast<size_t>(lab.arg_num("close"));
  for (size_t k = 0; k < trips.size() && k < close; k++) {
    const std::string id = "trip-" + std::to_string(k);
    if (!lab.wants(id)) continue;
    const Trip& t = trips[k];
    std::vector<sh::Vec3f> span = t.pro_s > 0 ? pro_path(t) : std::vector<sh::Vec3f>{};
    for (const bo::RunResult& r : t.runs) span.insert(span.end(), r.trail.begin(), r.trail.end());
    span.push_back(t.from);
    span.push_back(t.to);
    la::WireView v;
    v.width = 900;
    v.height = 700;
    v.fit(span);
    gl::Picture p = la::wire_picture(edges, v);
    for (size_t w = 0; w < kWayCount; w++) line(p, v, t.runs[w].trail, kWays[w].colour, 2);
    if (t.pro_s > 0) line(p, v, pro_path(t), kPro, 3);
    legend(p);
    std::string times;
    if (t.pro_s > 0) times += who + " " + to_fixed(t.pro_s, 2) + " s";
    for (size_t w = 0; w < kWayCount; w++)
      times += std::string(times.empty() ? "" : ", ") + kWays[w].name + " " +
               (t.pass_s[w] >= 0 ? to_fixed(t.pass_s[w], 2) + " s" : "never got there");
    lab.shot({id, "TRIP " + std::to_string(k) + " CLOSE UP",
              "Every way's path on trip " + std::to_string(k) + ": " + times +
                  (t.trick.empty() ? "." : ". Trick candidate: " + t.trick + "."),
              "the hops weave in arcs round the walk's line and cut its corners; where the purple "
              "leaves every bot's line he knows a way they do not",
              in},
             p);
  }
  // One trip from inside, and its speed against distance.
  size_t show = 0;
  for (size_t k = 0; k < trips.size(); k++)
    if (trips[k].pass_s[2] >= 0 && trips[k].pro_s > 0) {
      show = k;
      break;
    }
  const Trip& t = trips[show];
  if (lab.wants("inside") && !t.runs[2].trail.empty()) {
    const sh::Vec3f a = t.from;
    const std::vector<sh::Vec3f>& hop = t.runs[2].trail;
    const sh::Vec3f b = hop[std::min(hop.size() - 1, static_cast<size_t>(40))];
    la::InsideCamera c;
    const float hd = static_cast<float>(ge::core::libm::atan2(b.y - a.y, b.x - a.x) * 180 / M_PI);
    const float r = static_cast<float>(hd * M_PI / 180);
    const sh::Vec3f eye0{a.x, a.y, a.z + 22};
    const sh::Vec3f want{a.x - 120 * static_cast<float>(ge::core::libm::cos(r)),
                         a.y - 120 * static_cast<float>(ge::core::libm::sin(r)), a.z + 70};
    const sh::HullTrace tr = sh::trace_hull(m.world->solids[0].point_hull, eye0, want);
    const float f = tr.fraction < 1 ? std::max(0.0f, tr.fraction - 0.08f) : 1.0f;
    c.eye = {eye0.x + (want.x - eye0.x) * f, eye0.y + (want.y - eye0.y) * f,
             eye0.z + (want.z - eye0.z) * f};
    c.yaw = hd;
    c.pitch = -12;
    c.width = 1200;
    c.height = 720;
    la::InsideView iv(c);
    iv.faces(m.map);
    iv.edges(edges);
    for (size_t w = 0; w < kWayCount; w++)
      for (size_t i = 1; i < t.runs[w].trail.size(); i++)
        iv.line(t.runs[w].trail[i - 1], t.runs[w].trail[i], kWays[w].colour, 2, false, 0);
    if (t.pro_s > 0) {
      const std::vector<sh::Vec3f> pp = pro_path(t);
      for (size_t i = 1; i < pp.size(); i++) iv.line(pp[i - 1], pp[i], kPro, 3, false, 0);
    }
    legend(iv.picture());
    lab.shot({"inside", "TRIP " + std::to_string(show) + " FROM INSIDE " + m.name,
              "A camera in the map behind the trip's start looking down it: the faces filled "
              "pale, the outlines, and every way's path as a line in the world (hidden behind "
              "walls).",
              "the hops rise and fall over the floor; the walk's grey runs flat along it", in},
             iv.picture());
  }
  if (lab.wants("speed")) {
    gl::LineChart ch;
    ch.spikes = false;
    ch.frame = {"SPEED ALONG TRIP " + std::to_string(show),
                m.name,
                "distance covered (u)",
                "speed (u/s)",
                960,
                420};
    auto add = [&](const std::string& name, const std::vector<sh::HopFrame>& fr, size_t a,
                   size_t b) {
      gl::Trace tr{name, {}, {}};
      double d = 0;
      for (size_t i = a; i <= b && i < fr.size(); i++) {
        if (i > a) d += across(fr[i - 1].origin, fr[i].origin);
        tr.x.push_back(d);
        tr.y.push_back(fr[i].speed);
      }
      ch.traces.push_back(tr);
    };
    if (t.pro_s > 0) add(who, pro.frames, static_cast<size_t>(t.f0), static_cast<size_t>(t.f1));
    for (size_t w = 0; w < kWayCount; w++)
      if (!t.reads[w].frames.empty())
        add(kWays[w].name, t.reads[w].frames, 0, t.reads[w].frames.size() - 1);
    ch.rules = {320};
    const gl::Chart c = gl::line_chart(ch);
    lab.shot({"speed", c.title,
              "Each way's speed against the distance it has covered on the trip (the dashed line "
              "is a run's 320).",
              "the hops climb past 320 hop by hop; the walk holds at 320", in},
             gl::to_picture(c.plot));
  }
}

const gl::LabSpec kSpec{
    "se-hop",
    gl::LabKind::scenario,
    "game:shooters_edge",
    "plugins/shooter/pilot.hpp's hop (an expert's bunny hop) against walking and strafing on a "
    "real map's trips (games/shooters_edge/bots/pilot_run.hpp run_route), read by "
    "plugins/shooter/hops.hpp beside a recorded pro",
    "bots",
    "Do the bots get round a real map as fast as a pro when they hop, and where does the pro know "
    "a way the bots do not?",
    {
        {"map", "", "the map's .bsp path; empty: --basedir, else the fixture halls"},
        {"basedir", "", "a folder holding a game folder, to read --mapname from"},
        {"game", "id1", "the game folder under --basedir"},
        {"mapname", "", "with --basedir: the map's name"},
        {"file", "", "a demo on that map: its player's trips (empty: the map's far corners)"},
        {"player", "", "the demo's player: a name or a slot (empty: the top fragger)"},
        {"trips", "8", "the most trips kept (the longest)"},
        {"trip-s", "3", "a trip's longest time, s"},
        {"trip-min", "800", "a trip's least distance across, units"},
        {"trick", "0.75",
         "a trip is a trick candidate when he takes under this share of the hop's time"},
        {"close", "4", "the trips drawn close up"},
        {"limit-s", "12", "a run gives up after this long, s"},
        {"seed", "1", "the hands' seed"},
        {"trace", "false", "write every run's commands as trip-<k>-<way>.tsv"},
    },
    "",
    run,
};
GE_LAB(kSpec);

}  // namespace
