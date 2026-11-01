// LAB `se-moves`: HOW A PLAYER MOVES THROUGH A REAL MAP, read off a recorded game and drawn over
// the map as a wireframe. A demo of any format (`--file`, plain or gzip; by default a multiview
// recording written in code, a scripted walk round the fixture halls) is read into every
// player's track (move_study.hpp), each track read as a bunny hopper's (shooter/hops.hpp) on the
// map's own hulls (`--map=<a .bsp>`, or `--basedir` and `--mapname`, or the fixture halls).
//
// What it shows: a table of every player's movement (jumps a minute, the share of jumps chained
// on the landing frame, the longest chain, the time in the air, the speeds, how hard he turns in
// the air against the most the move allows, how far his view sits off his velocity, the speed a
// chained hop gains a second); for the player followed (`--player=<name or slot>`, by default the
// top fragger) his whole path over the map's wireframe from above and at an angle, coloured by
// speed; his longest hop chain close up with his view every few frames; that chain's speed and
// turn against the move's most; his speed over the match; and every player's ground frames
// before a jump and view lag side by side. Deterministic: the digest holds the reading.
//
// ge:summary Lab: a recorded player's movement read as hops and drawn over the map's wireframe
// ge:keywords lab se-moves movement bunny-hop demo mvd wireframe path speed chain turn-share
// ge:keywords view-lag ground-frames players compare study
// ge:skill shooter labs
// ge:test tests/shooter_hops_test.cpp
#include <algorithm>
#include <cctype>
#include <cmath>
#include <string>
#include <vector>

#include "core/libm/libm.hpp"
#include "core/num_text.hpp"
#include "lab/chart.hpp"
#include "lab/gif.hpp"
#include "lab/lab.hpp"
#include "shooter/hops.hpp"
#include "shooters_edge/client/demo_write.hpp"
#include "shooters_edge/labs/move_study.hpp"
#include "shooters_edge/labs/wire_inside.hpp"
#include "shooters_edge/labs/wire_view.hpp"

namespace {

namespace gl = ge::lab;
namespace sh = ge::shooter;
namespace cl = shooters_edge::client;
namespace la = shooters_edge::labs;
using ge::core::num::to_fixed;

using In = std::vector<std::pair<std::string, std::string>>;

std::string lower(std::string s) {
  for (char& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  return s;
}

// The chain of hops with the most hops (the first such), as a frame range.
bool longest_chain(const sh::HopReading& r, int32_t& from, int32_t& to, int32_t& hops) {
  hops = 0;
  for (size_t k = 0; k < r.hops.size();) {
    size_t j = k;
    while (j + 1 < r.hops.size() && r.hops[j + 1].chain == r.hops[k].chain) j++;
    const auto n = static_cast<int32_t>(j - k + 1);
    if (n > hops) {
      hops = n;
      from = r.hops[k].takeoff;
      to = r.hops[j].land;
    }
    k = j + 1;
  }
  return hops > 0;
}

// A camera `back` units behind and `up` above `at` looking along `heading` (degrees), pulled in
// short of any wall between the player's eye and it (the map's point hull), as a chase camera.
la::InsideCamera chase_camera(const sh::MoveWorld& world, sh::Vec3f at, float heading, float back,
                              float up, int w, int h) {
  const float a = heading * static_cast<float>(M_PI / 180);
  const sh::Vec3f eye{at.x, at.y, at.z + 22};
  const sh::Vec3f want{at.x - back * static_cast<float>(ge::core::libm::cos(a)),
                       at.y - back * static_cast<float>(ge::core::libm::sin(a)), at.z + 22 + up};
  const sh::HullTrace tr = sh::trace_hull(world.solids[0].point_hull, eye, want);
  const float f = tr.fraction < 1 ? std::max(0.0f, tr.fraction - 0.08f) : 1.0f;
  la::InsideCamera c;
  c.eye = {eye.x + (want.x - eye.x) * f, eye.y + (want.y - eye.y) * f,
           eye.z + (want.z - eye.z) * f};
  c.yaw = heading;
  c.pitch = -ge::core::libm::atan2(up * f + 22, back * f + 60) * 180 / M_PI * 0.6;
  c.width = w;
  c.height = h;
  return c;
}

// The heading the chase camera looks along at frame i: the velocity's, held through standing.
float chase_heading(const sh::HopReading& r, size_t i, float last) {
  const sh::HopFrame& f = r.frames[i];
  return f.phase != sh::HopPhase::cut && f.speed > 60 ? f.heading : last;
}

void run(gl::Lab& lab) {
  std::string error;
  la::StudyMap m;
  if (!la::load_study_map(lab.arg("map"), lab.arg("basedir"), lab.arg("game"), lab.arg("mapname"),
                          m, error)) {
    lab.fail("se-moves: " + error, "next: make lab LAB=se-moves ARGS=--help; skill: shooter");
    return;
  }
  const std::string file = lab.arg("file");
  std::vector<uint8_t> bytes;
  if (file.empty()) {
    bytes = cl::write_multiview_demo(cl::loop_script("halls", {256, 192, 24}, 120, 600));
  } else {
    bytes = la::read_whole(file);
    if (bytes.empty()) {
      lab.fail("se-moves: " + file + " does not open", "next: pass --file=<a demo's path>");
      return;
    }
  }
  la::DemoTracks dt;
  if (!la::read_demo_tracks(std::move(bytes), dt, error)) {
    lab.fail("se-moves: the demo: " + error, "next: run the se-demo lab on it; skill: shooter");
    return;
  }
  if (!dt.map.empty() && dt.map != m.name)
    lab.note("the demo was recorded on " + dt.map + ", the map read is " + m.name);

  sh::HopSettings hs;
  hs.cmd_hz = lab.arg_num("hz");
  std::vector<int> slots;
  std::vector<sh::HopReading> reads(32);
  for (int k = 0; k < 32; k++) {
    if (dt.tracks[static_cast<size_t>(k)].size() < 3) continue;
    slots.push_back(k);
    sh::read_hops(dt.tracks[static_cast<size_t>(k)], *m.world, hs, reads[static_cast<size_t>(k)]);
  }
  if (slots.empty()) {
    lab.fail("se-moves: no player moves in the demo", "next: pass a multiplayer recording");
    return;
  }
  auto name_of = [&](int k) {
    const std::string& n = dt.names[static_cast<size_t>(k)];
    return n.empty() ? "slot " + std::to_string(k) : n;
  };
  // The player followed.
  int me = slots[0];
  const std::string want = lab.arg("player");
  if (want.empty()) {
    for (int k : slots)
      if (dt.frags[static_cast<size_t>(k)] > dt.frags[static_cast<size_t>(me)]) me = k;
  } else {
    bool found = false;
    for (int k : slots)
      if (lower(name_of(k)) == lower(want) || std::to_string(k) == want) {
        me = k;
        found = true;
      }
    if (!found) {
      std::string names;
      for (int k : slots) names += " " + name_of(k);
      lab.fail("se-moves: no player " + want + " in the demo",
               "next: pass --player= one of:" + names);
      return;
    }
  }
  const std::string who = name_of(me);
  const In in = {{"file", file.empty() ? "(written in code)" : file},
                 {"map", m.name + " (" + m.from + ")"},
                 {"player", who},
                 {"hz", lab.arg("hz")}};

  // Every player's table.
  gl::LabTable t{"every player's movement",
                 {"slot", "name", "frags", "jumps/min", "chained %", "longest chain", "air %",
                  "speed p50", "p90", "top", "turn share", "lag deg", "gain u/s/s"},
                 {}};
  for (int k : slots) {
    const sh::HopSummary& s = reads[static_cast<size_t>(k)].summary;
    const double minutes = s.seconds / 60;
    t.rows.push_back(
        {std::to_string(k), name_of(k), std::to_string(dt.frags[static_cast<size_t>(k)]),
         to_fixed(minutes > 0 ? s.jumps / minutes : 0, 1),
         to_fixed(s.jumps > 0 ? 100.0 * s.chained / s.jumps : 0, 1),
         std::to_string(s.longest_chain), to_fixed(100.0 * s.air_share, 1),
         to_fixed(s.speed_p50, 0), to_fixed(s.speed_p90, 0), to_fixed(s.speed_top, 0),
         to_fixed(s.turn_share_p50, 2), to_fixed(s.lag_p50, 1), to_fixed(s.gain_per_s, 1)});
  }
  lab.table(t);
  const sh::HopReading& r = reads[static_cast<size_t>(me)];
  const sh::HopSummary& s = r.summary;
  lab.metric("demo.frames", static_cast<double>(dt.frames), "count");
  lab.metric("player.jumps", s.jumps, "count");
  lab.metric("player.chained", s.chained, "count");
  lab.metric("player.longest-chain", s.longest_chain, "hops");
  lab.metric("player.air-share", s.air_share, "share");
  lab.metric("player.speed-p50", s.speed_p50, "u/s");
  lab.metric("player.speed-p90", s.speed_p90, "u/s");
  lab.metric("player.speed-top", s.speed_top, "u/s");
  lab.metric("player.turn-share-p50", s.turn_share_p50, "share");
  lab.metric("player.lag-p50", s.lag_p50, "deg");
  lab.metric("player.gain-per-s", s.gain_per_s, "u/s/s");

  // The wireframe and the path.
  const std::vector<la::WireEdge> edges = la::wire_edges(m.map);
  const std::vector<sh::Vec3f> map_pts = la::edge_points(edges);
  std::vector<sh::Vec3f> pts;
  std::vector<float> speeds;
  std::vector<uint8_t> cut;
  for (const sh::HopFrame& f : r.frames) {
    pts.push_back(f.origin);
    speeds.push_back(f.speed);
    cut.push_back(f.phase == sh::HopPhase::cut);
  }
  auto takeoffs = [&](gl::Picture& p, const la::WireView& v, int32_t from, int32_t to) {
    for (const sh::Hop& h : r.hops) {
      if (h.takeoff < from || h.takeoff > to) continue;
      double x, y, far;
      v.pixel(r.frames[static_cast<size_t>(h.takeoff)].origin, x, y, far);
      gl::fill_disc(p, x, y, 2.5,
                    h.ground_before >= 0 && h.ground_before <= hs.chain_frames
                        ? gl::Rgba{20, 20, 20, 255}
                        : gl::Rgba{255, 255, 255, 255});
      gl::draw_ring(p, x, y, 2.5, {20, 20, 20, 255});
    }
  };
  for (const auto& [id, elevation] : {std::pair<std::string, double>{"map-top", 90.0},
                                      std::pair<std::string, double>{"map-angle", 40.0}}) {
    if (!lab.wants(id)) continue;
    la::WireView v;
    v.elevation = elevation;
    v.yaw = elevation == 90 ? 0 : lab.arg_num("yaw");
    v.fit(map_pts);
    gl::Picture p = la::wire_picture(edges, v);
    la::draw_path(p, v, pts, speeds, cut, 1.5);
    takeoffs(p, v, 0, static_cast<int32_t>(r.frames.size()));
    la::speed_legend(p, 10, 6);
    lab.shot(
        {id, who + (elevation == 90 ? " FROM ABOVE ON " : " AT AN ANGLE ON ") + m.name,
         "The map as a wireframe (every face's outline of the world, darker nearer), and " + who +
             "'s whole path through the recording coloured by his speed: grey "
             "standing, blue at a run's 320, through green and yellow to red past 700. A "
             "dot is a jump: black when it came on the landing's frame (a chained hop), "
             "white when he stood first.",
         "the path stays inside the map's rooms and corridors; the fast colours sit on the "
         "long ways and the curves between rooms, the blue in fights",
         in},
        p);
  }

  // The longest chain close up.
  int32_t c0 = 0, c1 = 0, chain_hops = 0;
  if (longest_chain(r, c0, c1, chain_hops)) {
    c0 = std::max(0, c0 - 30);
    c1 = std::min(static_cast<int32_t>(r.frames.size()) - 1, c1 + 20);
    lab.metric("chain.hops", chain_hops, "hops");
    lab.metric("chain.from-s", r.frames[static_cast<size_t>(c0)].t, "s");
    if (lab.wants("chain")) {
      std::vector<sh::Vec3f> span(pts.begin() + c0, pts.begin() + c1 + 1);
      la::WireView v;
      v.width = 1000;
      v.height = 800;
      v.fit(span);
      // Zoomed out a little round the chain, so the walls it passes show.
      v.scale *= 0.8;
      double cx = 0, cy = 0, far = 0;
      sh::Vec3f mid = span[span.size() / 2];
      v.ox = 0;
      v.oy = 0;
      v.pixel(mid, cx, cy, far);
      v.ox = v.width / 2.0 - cx;
      v.oy = v.height / 2.0 - cy;
      gl::Picture p = la::wire_picture(edges, v);
      la::draw_path(p, v, span,
                    std::span<const float>(speeds.data() + c0, static_cast<size_t>(c1 - c0 + 1)),
                    std::span<const uint8_t>(cut.data() + c0, static_cast<size_t>(c1 - c0 + 1)), 3);
      // The view every fourth frame: a short line along the yaw.
      for (int32_t i = c0; i <= c1; i += 4) {
        const sh::HopFrame& f = r.frames[static_cast<size_t>(i)];
        const float a = f.yaw * static_cast<float>(M_PI / 180);
        const sh::Vec3f tip{f.origin.x + 40 * static_cast<float>(ge::core::libm::cos(a)),
                            f.origin.y + 40 * static_cast<float>(ge::core::libm::sin(a)),
                            f.origin.z};
        double ax, ay, af, bx, by, bf;
        v.pixel(f.origin, ax, ay, af);
        v.pixel(tip, bx, by, bf);
        gl::draw_line(p, ax, ay, bx, by, {120, 40, 160, 255});
      }
      takeoffs(p, v, c0, c1);
      la::speed_legend(p, 10, 6);
      lab.shot({"chain", who + "'S LONGEST HOP CHAIN: " + std::to_string(chain_hops) + " HOPS",
                "The stretch round his longest run of chained hops, close up: his path coloured "
                "by speed, a purple tick along his view every fourth frame (40 units long), a "
                "dot at each jump (black: on the landing frame).",
                "in the air the ticks lie along the path (the view on the velocity, the side "
                "key alone); the path curves one way, then the other, with no straight between",
                in},
               p);
    }
    if (lab.wants("chain-turn")) {
      gl::LineChart c;
      c.spikes = false;
      c.frame = {"THE CHAIN'S SPEED AND TURN",
                 who + " from t=" + to_fixed(r.frames[static_cast<size_t>(c0)].t, 2) + " s",
                 "t (s)",
                 "u/s and deg/s",
                 960,
                 420};
      gl::Trace sp{"speed u/s", {}, {}}, tu{"|turn| deg/s", {}, {}},
          mx{"the move's most turn deg/s", {}, {}};
      for (int32_t i = c0; i <= c1; i++) {
        const sh::HopFrame& f = r.frames[static_cast<size_t>(i)];
        if (f.phase == sh::HopPhase::cut) continue;
        sp.x.push_back(f.t), sp.y.push_back(f.speed);
        if (f.phase == sh::HopPhase::air) {
          tu.x.push_back(f.t), tu.y.push_back(std::fabs(f.turn));
          mx.x.push_back(f.t), mx.y.push_back(sh::max_turn(f.speed, hs.cmd_hz));
        }
      }
      c.traces = {sp, tu, mx};
      for (const sh::Hop& h : r.hops)
        if (h.takeoff >= c0 && h.takeoff <= c1)
          c.marks.push_back({r.frames[static_cast<size_t>(h.takeoff)].t, "jump"});
      const gl::Chart ch = gl::line_chart(c);
      lab.shot({"chain-turn", ch.title,
                "His speed over the chain, and in the air how fast his velocity turns against "
                "the most the move lets it at that speed (the wish at right angles, " +
                    lab.arg("hz") + " commands a second). A grey line is a jump.",
                "the turn rides near the move's most between switches: the turn is the gain", in},
               gl::to_picture(ch.plot));
    }

    // From inside the map: a chase camera at the chain's middle, the chain as a line in the world.
    if (lab.wants("inside")) {
      const size_t mid = static_cast<size_t>((c0 + c1) / 2);
      float hd = r.frames[mid].yaw;
      hd = chase_heading(r, mid, hd);
      la::InsideView iv(chase_camera(*m.world, r.frames[mid].origin, hd, 160, 70, 1200, 720));
      iv.faces(m.map);
      iv.edges(edges);
      const int32_t p0 = std::max(0, c0 - 120);
      const int32_t p1 = std::min(static_cast<int32_t>(r.frames.size()) - 1, c1 + 120);
      for (int32_t i = p0 + 1; i <= p1; i++) {
        if (cut[static_cast<size_t>(i)] || cut[static_cast<size_t>(i - 1)]) continue;
        iv.line(pts[static_cast<size_t>(i - 1)], pts[static_cast<size_t>(i)],
                la::speed_colour(speeds[static_cast<size_t>(i)]), 3, false, 0);
      }
      gl::Picture& p = iv.picture();
      la::speed_legend(p, 10, 6);
      lab.shot({"inside", who + "'S CHAIN SEEN FROM INSIDE " + m.name,
                "A camera in the map behind him at the middle of his longest chain, looking "
                "along his velocity: the map's faces filled pale (floors lighter), every face "
                "outline drawn where no wall is nearer, and his path from 1.5 s before the chain "
                "to 1.5 s after it as a line in the world coloured by speed (hidden where a wall "
                "stands between).",
                "the line weaves over the floor in arcs and rises and falls with each hop", in},
               p);
    }
    // The chase as an animation: every third frame of the chain, the path so far behind him.
    if (lab.wants("chase")) {
      std::vector<gl::Picture> frames;
      float hd = r.frames[static_cast<size_t>(c0)].yaw;
      float shown = hd;
      for (int32_t i = c0; i <= c1; i += 3) {
        hd = chase_heading(r, static_cast<size_t>(i), hd);
        shown = shown + std::remainder(hd - shown, 360.0f) * 0.35f;  // the camera eases round
        la::InsideView iv(chase_camera(*m.world, r.frames[static_cast<size_t>(i)].origin, shown,
                                       150, 60, 480, 300));
        iv.faces(m.map);
        iv.edges(edges);
        for (int32_t j = std::max(c0 + 1, i - 400); j <= i; j++) {
          if (cut[static_cast<size_t>(j)] || cut[static_cast<size_t>(j - 1)]) continue;
          iv.line(pts[static_cast<size_t>(j - 1)], pts[static_cast<size_t>(j)],
                  la::speed_colour(speeds[static_cast<size_t>(j)]), 2, false, 0);
        }
        double x, y;
        if (iv.pixel(r.frames[static_cast<size_t>(i)].origin, x, y))
          gl::fill_disc(iv.picture(), x, y, 4, {20, 20, 20, 255});
        gl::draw_text(iv.picture(), 8, 8,
                      to_fixed(r.frames[static_cast<size_t>(i)].speed, 0) + " U/S",
                      {20, 20, 20, 255}, 2);
        frames.push_back(std::move(iv.picture()));
      }
      if (!frames.empty()) {
        lab.attach("chase.gif", gl::encode_gif_frames(frames, 4));
        lab.shot({"chase-first", who + "'S CHAIN: THE CHASE'S FIRST FRAME",
                  "The first frame of chase.gif beside this report: a camera following him "
                  "through his longest chain, his path so far as a line, his speed top left.",
                  "the camera stands inside the map; open chase.gif for the run", in},
                 frames.front());
      }
    }
  }

  if (lab.wants("speed")) {
    gl::LineChart c;
    c.spikes = false;
    c.frame = {"SPEED OVER THE MATCH", who + " on " + m.name, "t (s)", "speed (u/s)", 960, 360};
    gl::Trace sp{who, {}, {}};
    for (size_t i = 0; i < r.frames.size(); i += 4) {
      const sh::HopFrame& f = r.frames[i];
      sp.x.push_back(f.t);
      sp.y.push_back(f.phase == sh::HopPhase::cut ? std::nan("") : f.speed);
    }
    c.traces = {sp};
    c.rules = {320};
    const gl::Chart ch = gl::line_chart(c);
    lab.shot({"speed", ch.title,
              "His speed across the plan every fourth frame; the dashed line is a run's 320.",
              "time above the line is hopping; dips to 0 are deaths and standing fights", in},
             gl::to_picture(ch.plot));
  }

  // Every player's ground frames before a jump, and the view's lag in the air.
  std::vector<gl::Sample> ground, lag;
  for (int k : slots) {
    gl::Sample g{name_of(k), {}}, l{name_of(k), {}};
    for (const sh::Hop& h : reads[static_cast<size_t>(k)].hops)
      if (h.ground_before >= 0 && h.ground_before <= 10) g.values.push_back(h.ground_before);
    for (const sh::HopFrame& f : reads[static_cast<size_t>(k)].frames)
      if (f.phase == sh::HopPhase::air && f.speed > hs.fast && f.turn != 0 &&
          std::fabs(f.lag) <= 60)
        l.values.push_back(f.lag);
    if (!g.values.empty()) ground.push_back(g);
    if (!l.values.empty()) lag.push_back(l);
  }
  if (!ground.empty() && lab.wants("ground-before")) {
    const gl::Chart ch =
        gl::histogram_chart({"FRAMES ON THE GROUND BEFORE A JUMP", "every player, " + m.name,
                             "frames", "jumps", 960, 420},
                            ground, 11);
    lab.shot({"ground-before", ch.title,
              "How many recorded frames each player stood between landing and jumping again "
              "(10 and under): 1 is a jump on the landing's own command, no friction taken.",
              "a hopper's bar at 1 stands out; walking players spread to the right", in},
             gl::to_picture(ch.plot));
  }
  if (!lag.empty() && lab.wants("lag")) {
    const gl::Chart ch =
        gl::histogram_chart({"THE VIEW OFF THE VELOCITY IN THE AIR",
                             "every player, above " + to_fixed(hs.fast, 0) + " u/s",
                             "yaw - "
                             "heading (deg)",
                             "frames", 960, 420},
                            lag, 48);
    lab.shot({"lag", ch.title,
              "In the air and fast, the view's yaw less the velocity's heading (within 60 "
              "degrees). Near 0: the view on the velocity and the side key alone, the wish at "
              "right angles; near 45: the forward key held too.",
              "a narrow peak near 0 for a hopper (the recording shows the view a frame late, "
              "so it sits a few degrees outside the turn)",
              in},
             gl::to_picture(ch.plot));
  }
}

const gl::LabSpec kSpec{
    "se-moves",
    gl::LabKind::sim,
    "game:shooters_edge",
    "plugins/shooter/hops.hpp (read_hops) on a recorded game's players over a real map "
    "(games/shooters_edge/labs/move_study.hpp, wire_view.hpp)",
    "shooter",
    "How does a player move through a real map: how often does he jump, does he chain his hops on "
    "the landing frame, how hard does he turn in the air against the most the move allows, and "
    "what path does he take?",
    {
        {"file", "", "a demo's path (plain or gzip); empty: a walk written in code"},
        {"map", "", "the map's .bsp path (the demo's map); empty: --basedir, else the fixture"},
        {"basedir", "", "a folder holding a game folder, to read --mapname from"},
        {"game", "id1", "the game folder under --basedir"},
        {"mapname", "", "with --basedir: the map's name"},
        {"player", "", "the player followed: a name or a slot (empty: the top fragger)"},
        {"hz", "77", "the commands a second the air's most turn is read at"},
        {"yaw", "30", "the angled view's turn about z, degrees"},
    },
    "",
    run,
};
GE_LAB(kSpec);

}  // namespace
