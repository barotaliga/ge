// LAB `se-pilot`: does the pilot (plugins/shooter/pilot.hpp) play a nav route the way the graph
// proved it, through the same player move, and does it look like hands did it? On the nav course
// each route is played with plain hands and with human hands over a few seeds: the time against
// the route's proven time, the jumps and run-ups, and every trail drawn from above, coloured by
// the pilot's mode. The east floor's straight is run walking and strafe jumping, its speed charted.
// The human runs' forward and side axes are measured against a person's bands (engine/ai/human).
// On the server's arena the pilot fills a bot slot and visits its goals: through the door, up the
// plat to the ledge, and back to a door that stays shut from the east; its trail from above and
// its height against the plat's and the door's from the side.
//
// ge:summary Lab: the pilot's routes on the nav course and the arena: times, trails, humanness
// ge:keywords lab se-pilot pilot route trail time proven run-up strafe-jump ride plat door
// ge:keywords barred humanness hands top-view arena
// ge:skill shooter bots labs
// ge:test tests/shooter_pilot_test.cpp
#include <memory>
#include <string>
#include <vector>

#include "ai/human.hpp"
#include "core/num_text.hpp"
#include "lab/chart.hpp"
#include "lab/lab.hpp"
#include "lab/raster.hpp"
#include "nav_top.hpp"
#include "shooter/pilot.hpp"
#include "shooters_edge/bots/pilot_run.hpp"
#include "shooters_edge/sim/arena.hpp"

namespace {

namespace gl = ge::lab;
namespace sh = ge::shooter;
namespace pr = shooters_edge::bots;
namespace sl = shooters_edge::labs;
using ge::core::num::to_fixed;
using In = std::vector<std::pair<std::string, std::string>>;

std::string key_of(std::string s) {
  for (char& c : s)
    if (c == ' ' || c == '_' || c == '\'') c = '-';
  return s;
}

// The pilot's modes as drawn (PilotMode's order).
const gl::Rgba kModeColour[9] = {
    {120, 120, 120, 255},  // idle
    {30, 30, 30, 255},     // walk
    {40, 90, 220, 255},    // run_up
    {220, 40, 40, 255},    // jump (running at it)
    {240, 140, 20, 255},   // air
    {160, 50, 200, 255},   // wait
    {20, 150, 80, 255},    // ride
    {0, 170, 190, 255},    // strafe
    {30, 30, 30, 255},     // arrived
};

void legend(gl::Picture& p) {
  const sh::PilotMode shown[] = {sh::PilotMode::walk,  sh::PilotMode::run_up, sh::PilotMode::jump,
                                 sh::PilotMode::air,   sh::PilotMode::wait,   sh::PilotMode::ride,
                                 sh::PilotMode::strafe};
  int x = sl::kTopMargin;
  for (const sh::PilotMode m : shown) {
    const std::string name = sh::pilot_mode_name(m);
    gl::fill_rect(p, x, 8, 14, 6, kModeColour[static_cast<int>(m)]);
    gl::draw_text(p, x + 18, 7, name, {30, 30, 30, 255});
    x += 18 + gl::text_width(name) + 16;
  }
}

void draw_trail(gl::Picture& p, const sl::TopView& v, const std::vector<sh::Vec3f>& trail,
                const std::vector<uint8_t>& modes, double width) {
  for (size_t i = 0; i + 1 < trail.size(); i++)
    gl::draw_line(p, v.px(trail[i].x), v.py(trail[i].y), v.px(trail[i + 1].x), v.py(trail[i + 1].y),
                  kModeColour[modes[i]], width);
  if (trail.empty()) return;
  gl::draw_ring(p, v.px(trail.front().x), v.py(trail.front().y), 6, {30, 30, 30, 255});
  gl::fill_disc(p, v.px(trail.back().x), v.py(trail.back().y), 4, {30, 30, 30, 255});
}

const char* hands_name(sh::PilotHands h) { return h == sh::PilotHands::human ? "human" : "none"; }

void run_course(gl::Lab& lab, const std::vector<sh::PilotHands>& hands, int seeds, const In& in) {
  const std::unique_ptr<pr::Course> cp = pr::make_course();
  const pr::Course& c = *cp;
  const sl::TopView v{c.graph.lo, c.graph.hi};
  gl::LabTable runs{"the nav course's routes played",
                    {"route", "hands", "seed", "arrived", "time s", "proven s", "ratio", "jumps",
                     "run-ups", "plans", "stuck", "lost"},
                    {}};
  int arrived = 0, played = 0;
  std::vector<double> fw, fw_want, side, side_want;
  for (const sh::PilotHands h : hands) {
    gl::Picture p = sl::floor_picture(c.world, v);
    for (const pr::CourseRoute& r : pr::course_routes(c))
      for (int seed = 1; seed <= seeds; seed++) {
        const pr::RunResult run = pr::run_course(c, r.from, r.to, h, static_cast<uint32_t>(seed));
        played++;
        if (run.arrived) arrived++;
        draw_trail(p, v, run.trail, run.modes, h == sh::PilotHands::human ? 1.5 : 2.5);
        runs.rows.push_back({r.name, hands_name(h), std::to_string(seed),
                             run.arrived ? "yes" : "NO", to_fixed(run.time_s, 2),
                             to_fixed(run.proven_s, 2), to_fixed(run.time_s / run.proven_s, 2),
                             std::to_string(run.stats.jumps), std::to_string(run.stats.run_ups),
                             std::to_string(run.stats.plans), std::to_string(run.stats.stuck),
                             std::to_string(run.stats.lost)});
        if (seed == 1)
          lab.metric(std::string("course.") + key_of(r.name) + "." + hands_name(h) + ".time",
                     run.arrived ? run.time_s : -1, "s");
        if (h == sh::PilotHands::human) {
          fw.insert(fw.end(), run.forward.begin(), run.forward.end());
          fw_want.insert(fw_want.end(), run.forward_wanted.begin(), run.forward_wanted.end());
          side.insert(side.end(), run.side.begin(), run.side.end());
          side_want.insert(side_want.end(), run.side_wanted.begin(), run.side_wanted.end());
        }
        if (!run.arrived)
          lab.fail(std::string("se-pilot: ") + hands_name(h) + " hands did not arrive on " +
                       r.name + " (seed " + std::to_string(seed) + ")",
                   "next: run tests/shooter_pilot_test.cpp and read the run-ups and stuck "
                   "counts in the table");
      }
    legend(p);
    const bool human = h == sh::PilotHands::human;
    lab.shot({std::string("course-") + hands_name(h),
              human ? "THE COURSE'S ROUTES, HUMAN HANDS" : "THE COURSE'S ROUTES, PLAIN HANDS",
              "To scale from above, a pixel a unit. Every route played on the course (" +
                  std::to_string(seeds) +
                  " seeds each), the trail coloured by the pilot's mode at each command (ring: "
                  "start, dot: where it stopped).",
              human ? "the same ways as plain hands but looping wide at corners (the view turns "
                      "late and the pilot does not yet slow for a corner); blue where a run at a "
                      "jump came in off its line and went back to stand behind it; every trail "
                      "ending on its goal's dot"
                    : "black walks cutting the corners of the graph's grid; one straight red run "
                      "into each jump, orange arcs over the pit and onto the crate",
              in},
             p);
  }
  lab.table(runs);
  lab.metric("course.arrived", arrived, "count");
  lab.metric("course.played", played, "count");

  if (!fw.empty()) {
    const double hz = 1000.0 / pr::kPilotMsec;
    const ge::ai::AxisHumanness f = ge::ai::axis_humanness(fw, fw_want, -1, 1, 0, hz);
    const ge::ai::AxisHumanness s = ge::ai::axis_humanness(side, side_want, -1, 1, 0, hz);
    std::vector<ge::ai::OutOfBand> out;
    // Forward is held flat out to run, as a trigger is: a stick's rim band is for steering.
    ge::ai::check_axis("forward", ge::ai::Feel::trigger, f, out);
    ge::ai::check_axis("side", ge::ai::Feel::stick, s, out);
    gl::LabTable t{"human hands on the course: each axis against a person's",
                   {"axis", "saturated", "jumps", "reversals/min", "hold s", "lag s"},
                   {}};
    auto row = [&](const char* name, const ge::ai::AxisHumanness& a) {
      t.rows.push_back({name, to_fixed(a.saturated, 3), to_fixed(a.jumps, 3),
                        to_fixed(a.reversals_min, 1), to_fixed(a.hold_s, 2), to_fixed(a.lag_s, 3)});
      lab.metric(std::string("human.") + name + ".saturated", a.saturated, "share");
      lab.metric(std::string("human.") + name + ".jumps", a.jumps, "share");
      lab.metric(std::string("human.") + name + ".lag", a.lag_s, "s");
    };
    row("forward", f);
    row("side", s);
    lab.table(t);
    lab.metric("human.out-of-band", static_cast<double>(out.size()), "count");
    for (const ge::ai::OutOfBand& o : out) lab.note(ge::ai::out_of_band_text(o));
  }

  // The straight, walked and strafed.
  const pr::CourseRoute r = pr::course_long_straight(c);
  sh::PilotSettings walk = pr::course_strafe_settings();
  walk.strafe = false;
  const pr::RunResult a = pr::run_course(c, r.from, r.to, sh::PilotHands::none, 1, walk);
  const pr::RunResult b =
      pr::run_course(c, r.from, r.to, sh::PilotHands::none, 1, pr::course_strafe_settings());
  gl::LineChart ch;
  ch.spikes = false;
  ch.frame = {"THE EAST FLOOR'S STRAIGHT, WALKED AND STRAFE JUMPED",
              "plain hands, " + std::to_string(static_cast<int>(r.to.x - r.from.x)) + " units",
              "t (s)", "speed across (units/s)"};
  for (const auto* run : {&a, &b}) {
    gl::Trace t{run == &a ? "walked" : "strafe jumped", {}, {}};
    for (size_t i = 1; i < run->trail.size(); i++) {
      const sh::Vec3f d = run->trail[i] - run->trail[i - 1];
      t.x.push_back(static_cast<double>(i) * pr::kPilotMsec * 0.001);
      t.y.push_back(sh::root_of(d.x * d.x + d.y * d.y) * 1000.0 / pr::kPilotMsec);
    }
    ch.traces.push_back(t);
  }
  ch.rules = {320};
  lab.metric("strafe.walk.top", a.top_speed, "u/s");
  lab.metric("strafe.strafe.top", b.top_speed, "u/s");
  lab.metric("strafe.walk.time", a.time_s, "s");
  lab.metric("strafe.strafe.time", b.time_s, "s");
  lab.shot({"strafe", ch.frame.title,
            "The pilot's speed across at each command on the straight: walking (forward held), "
            "and strafe jumping (a jump on every landing, the view along the velocity, the side "
            "axis full). The dashed line is the walk's top speed, 320.",
            "the walk flat at 320; the strafe jump climbing past it in steps, one a hop, then "
            "falling back as it stops strafing before the wall",
            in},
           gl::to_picture(gl::line_chart(ch).plot));
}

void run_arena(gl::Lab& lab, const std::vector<sh::PilotHands>& hands, int frames, const In& in) {
  gl::LabTable t{"the arena on the server",
                 {"hands", "through the door", "on the ledge", "goals reached", "rides", "waits",
                  "barred", "strafes", "stuck", "lost"},
                 {}};
  for (const sh::PilotHands h : hands) {
    pr::Arena a;
    if (!pr::make_arena(a, h, 1)) {
      lab.fail("se-pilot: the arena's host did not start",
               "next: run tests/shooters_edge_server_test.cpp");
      return;
    }
    const pr::ArenaResult r = pr::run_arena(a, frames);
    const std::string k = std::string("arena.") + hands_name(h);
    lab.metric(k + ".arrivals", r.arrivals, "count");
    lab.metric(k + ".rides", r.stats.rides, "count");
    lab.metric(k + ".barred", r.stats.barred, "count");
    t.rows.push_back({hands_name(h), r.through_door ? "yes" : "NO", r.on_ledge ? "yes" : "NO",
                      std::to_string(r.arrivals), std::to_string(r.stats.rides),
                      std::to_string(r.stats.waits), std::to_string(r.stats.barred),
                      std::to_string(r.stats.strafes), std::to_string(r.stats.stuck),
                      std::to_string(r.stats.lost)});
    if (!r.through_door || !r.on_ledge)
      lab.fail(std::string("se-pilot: on the arena ") + hands_name(h) +
                   " hands missed the door or the ledge",
               "next: run tests/shooter_pilot_test.cpp's arena case");

    sh::MoveWorld world;
    sh::map_world(a.host.models[0], world);
    // Traced from under the server's roof (z 256), which would cover the floor.
    sl::TopView v{a.graph.lo, a.graph.hi};
    v.hi.z = 248;
    gl::Picture p = sl::floor_picture(world, v);
    draw_trail(p, v, r.trail, r.modes, 2);
    legend(p);
    lab.shot({std::string("arena-") + hands_name(h),
              h == sh::PilotHands::human ? "THE ARENA ON THE SERVER, HUMAN HANDS"
                                         : "THE ARENA ON THE SERVER, PLAIN HANDS",
              "To scale from above. The pilot in a bot slot of the real server for " +
                  std::to_string(frames) +
                  " frames, visiting its goals in turn: the west room's corners, the east room "
                  "through the door, the ledge in the south-east up the plat, the west room "
                  "again. The trail coloured by its mode.",
              "a cyan strafe up the west room's long side; black through the doorway; purple "
              "where it waits beside the plat and at the door shut from the east; green on the "
              "plat; the trail going on along the ledge and dropping off it for the west room",
              in},
             p);

    gl::LineChart ch;
    ch.spikes = false;
    ch.frame = {"THE PILOT'S HEIGHT AGAINST THE MOVERS'", std::string(hands_name(h)) + " hands",
                "t (s)", "z (units)"};
    gl::Trace me{"the pilot's feet", {}, {}}, plat{"the plat's bottom", {}, {}},
        door{"the door's bottom", {}, {}};
    for (size_t i = 0; i < r.trail.size(); i++) {
      const double ts = static_cast<double>(i) / 77.0;
      me.x.push_back(ts);
      me.y.push_back(r.trail[i].z + sh::kPlayerMins.z);
      plat.x.push_back(ts);
      plat.y.push_back(r.plat_z[i]);
      door.x.push_back(ts);
      door.y.push_back(r.door_z[i]);
    }
    ch.traces = {me, plat, door};
    lab.shot({std::string("arena-height-") + hands_name(h), ch.frame.title,
              "The pilot's feet, the plat's bottom and the door's bottom at every server frame.",
              "the door rising as the pilot reaches it and falling after; the plat cycling; the "
              "pilot's feet riding up with it once to the ledge's 104",
              in},
             gl::to_picture(gl::line_chart(ch).plot));
  }
  lab.table(t);
}

void run(gl::Lab& lab) {
  const std::string hands = lab.arg("hands");
  std::vector<sh::PilotHands> hs;
  if (hands == "none" || hands == "both") hs.push_back(sh::PilotHands::none);
  if (hands == "human" || hands == "both") hs.push_back(sh::PilotHands::human);
  const int seeds = static_cast<int>(lab.arg_num("seeds"));
  const int frames = static_cast<int>(lab.arg_num("frames"));
  if (hs.empty() || seeds < 1 || frames < 1) {
    lab.fail("se-pilot: --hands is none, human or both; --seeds and --frames at least 1",
             "next: make lab LAB=se-pilot ARGS=--help");
    return;
  }
  const In in = {{"hands", hands}, {"seeds", lab.arg("seeds")}, {"frames", lab.arg("frames")}};
  const std::string map = lab.arg("map");
  if (map == "course" || map == "both") run_course(lab, hs, seeds, in);
  if (map == "arena" || map == "both") run_arena(lab, hs, frames, in);
}

const gl::LabSpec kSpec{
    "se-pilot",
    gl::LabKind::scenario,
    "game:shooters_edge",
    "plugins/shooter/pilot.hpp (pilot_step, pilot_source) on the nav course and on the "
    "server's arena (games/shooters_edge/bots/pilot_run.hpp)",
    "bots",
    "Does the pilot play a nav route through the player move about as fast as the graph proved "
    "it, jump, strafe jump, ride the plat and handle a shut door, and do its human hands press "
    "the axes as a person's would?",
    {
        {"map", "both", "course, arena or both"},
        {"hands", "both", "none, human or both"},
        {"seeds", "3", "the hands' seeds each course route is played with"},
        {"frames", "2400", "the arena's server frames"},
    },
    "--seeds=1 --frames=1800",
    run,
};
GE_LAB(kSpec);

}  // namespace
