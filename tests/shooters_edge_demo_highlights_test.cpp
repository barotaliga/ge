// A demo's highlights and its playback (games/shooters_edge/client/demo_scan.hpp,
// demo_highlights.hpp, demo_playback.hpp, timedemo.hpp's timeline and seek): the scripted match
// (demo_write.hpp's `highlight_match`) scanned into its kills with the right killers, weapons,
// rocket flights, air, gibs, stacks and telefrags; the highlights found for the watched player and
// for another; the timeline's seek forward and back landing on the straight read; the playback's
// keys, skips, jumps, speeds and pause; the console's clock.
//
// ge:summary Tests: a demo's kills and highlights found, the timeline's seek, the playback controls
// ge:keywords test demo highlights scan kills multi-kill long-shot air-kill annihilation telefrag
// ge:keywords streak seek timeline playback skip jump speed pause parse_clock
// ge:skill shooter replay testing
// ge:test tests/shooters_edge_demo_highlights_test.cpp
#include <cmath>
#include <string>
#include <vector>

#include "check.hpp"
#include "shooters_edge/client/demo_highlights.hpp"
#include "shooters_edge/client/demo_playback.hpp"
#include "shooters_edge/client/demo_write.hpp"

namespace cl = shooters_edge::client;
namespace gr = ge::replay;

namespace {

cl::Timedemo opened() {
  cl::Timedemo t;
  std::string error;
  const bool ok = cl::open_demo(cl::write_multiview_match(cl::highlight_match()), t.demo, error);
  CHECK_MSG(ok, error.c_str());
  t.start = t.demo;
  t.active = true;
  return t;
}

const cl::DemoScan& scanned() {
  static const cl::DemoScan scan = [] {
    cl::DemoScan s;
    std::string error;
    const bool ok = cl::scan_demo(opened().start, s, error);
    CHECK_MSG(ok, error.c_str());
    return s;
  }();
  return scan;
}

bool near(double a, double b, double tol = 0.11) { return std::fabs(a - b) <= tol; }

const gr::Highlight* at(const gr::Highlights& h, double t) {
  for (const gr::Highlight& x : h.list)
    if (near(x.at_s, t)) return &x;
  return nullptr;
}

bool has(const gr::Highlight* h, const std::string& kind) {
  if (h == nullptr) return false;
  for (const std::string& k : h->kinds)
    if (k == kind) return true;
  return false;
}

}  // namespace

TEST(the_match_scans_into_its_kills) {
  const cl::DemoScan& s = scanned();
  CHECK(s.format == cl::DemoFormat::multiview);
  CHECK(near(s.length_s, 29.95, 0.06));
  CHECK(s.players == 0xFu);
  CHECK(s.names[3] == "charlie");
  CHECK(s.rockets == 4);
  CHECK(s.deaths == 8 && s.unmatched == 0);
  CHECK(s.kills.size() == 8);
  // 5 s: two by one rocket.
  CHECK(s.kills[0].killer == 0 && s.kills[1].killer == 0 && near(s.kills[0].t, 5));
  CHECK(s.kills[0].weapon == shooters_edge::hud::Weapon::rocket_launcher);
  // 10 s: the rocket's flight read off its path.
  CHECK(s.kills[2].victim == 3 && s.kills[2].rocket_flight > 1010 && s.kills[2].distance > 1000);
  // 15 s: in the air, gibbed.
  CHECK(s.kills[3].victim == 1 && s.kills[3].airborne && s.kills[3].gib);
  CHECK(!s.kills[0].airborne && !s.kills[2].airborne);
  // 20 s: a 200 stack lost in about 1.5 s, to the beam gun.
  CHECK(s.kills[4].victim == 2 && s.kills[4].stack == 200 && s.kills[4].stack_armour == 100);
  CHECK(s.kills[4].stack_s > 1.3 && s.kills[4].stack_s < 1.8);
  CHECK(s.kills[4].weapon == shooters_edge::hud::Weapon::beam_gun);
  CHECK(s.kills[5].weapon == shooters_edge::hud::Weapon::shotgun);
  // 25 s: the watched player killed by slot 3; 28 s: his telefrag.
  CHECK(s.kills[6].victim == 0 && s.kills[6].killer == 3 && s.kills[6].rocket_flight > 1000);
  CHECK(s.kills[7].telefrag && s.kills[7].killer == 0 && s.kills[7].victim == 1);
  CHECK(!s.kills[3].telefrag && !s.kills[6].telefrag);
}

TEST(the_watched_players_highlights) {
  const gr::Highlights h = cl::demo_highlights(scanned(), 0);
  CHECK(h.unruled == 0);
  CHECK(h.list.size() == 6);
  CHECK(has(at(h, 5), "multi-kill") && at(h, 5)->text.find("double kill") == 0);
  CHECK(has(at(h, 10), "long-shot"));
  CHECK(has(at(h, 10), "streak") && at(h, 10)->text.find("3 kills in 5 s") != std::string::npos);
  CHECK(has(at(h, 15), "air-kill"));
  CHECK(at(h, 15)->text.find("air gib: alpha") != std::string::npos);
  CHECK(has(at(h, 20), "annihilation"));
  CHECK(has(at(h, 28), "telefrag"));
  CHECK(has(at(h, 22), "streak") && !has(at(h, 22), "multi-kill"));  // 15, 20 and 22 s
  CHECK(at(h, 25) == nullptr);                                       // his own death
  for (const gr::Highlight& x : h.list) CHECK(x.subject == 0 && x.from_s < x.at_s);
  // Another player's: slot 3's long rocket at 25 s.
  const gr::Highlights other = cl::demo_highlights(scanned(), 3);
  CHECK(other.list.size() == 1 && has(at(other, 25), "long-shot"));
  CHECK(cl::demo_highlights(scanned(), -1).list.size() == 7);
}

TEST(a_seek_lands_on_the_straight_read_forward_and_back) {
  cl::Timedemo straight = opened();
  std::string error;
  while (straight.timeline.t < 12.0) cl::timedemo_step(straight, error);
  cl::Timedemo t = opened();
  CHECK(cl::demo_seek(t, 20, error) == cl::TimedemoStep::new_level);  // the first level loads
  CHECK(near(t.timeline.t, 20, 0.051));
  CHECK(cl::demo_seek(t, 12, error) == cl::TimedemoStep::frame);  // back: the same level
  CHECK(t.timeline.t == straight.timeline.t);
  CHECK(t.demo.state.frags == straight.demo.state.frags);
  for (size_t k = 0; k < 4; k++)
    CHECK(t.demo.state.players[k].origin.x == straight.demo.state.players[k].origin.x);
  CHECK(t.clock == t.demo.state.time);
  CHECK(cl::demo_seek(t, 99, error) == cl::TimedemoStep::end);
}

TEST(the_playback_keys_skip_jump_speed_and_pause) {
  cl::DemoPlayback p;
  cl::Timedemo t = opened();
  std::string error;
  CHECK(cl::playback_begin(p, t, error));
  std::vector<std::string> said;
  std::vector<bool> keys(512, false);
  bool none[512] = {};
  const auto press = [&](int scancode) {
    std::fill(keys.begin(), keys.end(), false);
    keys[static_cast<size_t>(scancode)] = true;
    bool b[512];
    for (size_t i = 0; i < 512; i++) b[i] = keys[i];
    return cl::playback_step(p, t, {std::span<const bool>(b, 512)}, 1280, 720, 0, 0, said, error);
  };
  const auto idle = [&](double dt) {
    return cl::playback_step(p, t, {std::span<const bool>(none, 512)}, 1280, 720, dt, 0, said,
                             error);
  };
  idle(0);  // the first frame
  CHECK(p.markers.size() == 6);
  press(79);  // Right: 15 s on
  CHECK(near(t.timeline.t, 15, 0.051));
  press(80);  // Left: 5 s back
  CHECK(near(t.timeline.t, 10, 0.051));
  press(78);  // Page Down: the next highlight's window (air gib at 15 s opens 3 s before)
  CHECK(near(t.timeline.t, 12, 0.051));
  CHECK(!said.empty() && said.back().find("air gib") != std::string::npos);
  press(75);  // Page Up: back to the last window opening before, the long rocket's and streak's
  CHECK(near(t.timeline.t, 2, 0.051));
  press(82);  // Up: faster
  CHECK(p.speed == 2);
  press(44);  // Space: paused, the clock stands
  CHECK(p.paused);
  const double here = t.timeline.t;
  idle(0.05);
  CHECK(t.timeline.t == here);
  press(14);  // K: playing again, at twice the speed
  idle(0.05);
  idle(0.05);
  CHECK(t.timeline.t > here + 0.1);
  for (int i = 0; i < 9; i++) press(81);  // Down past the slowest
  CHECK(p.speed == 0.25);
  p.skip_forward_s = 30;
  press(79);  // held a moment before the end
  CHECK(t.timeline.t <= p.scan.length_s && t.timeline.t > p.scan.length_s - 1);
}

TEST(the_bar_seeks_where_it_is_dragged_and_released) {
  cl::DemoPlayback p;
  cl::Timedemo t = opened();
  std::string error;
  CHECK(cl::playback_begin(p, t, error));
  std::vector<std::string> said;
  const ge::ui::PlayBarLayout l = ge::ui::play_bar_layout(1280, 720);
  const float y = l.track.y + 1;
  const float x_half = l.track.x + l.track.w / 2;
  cl::playback_step(p, t, {{}, 10, 10, false}, 1280, 720, 0, 0, said, error);
  cl::playback_step(p, t, {{}, x_half, y, true}, 1280, 720, 0, 0.1, said, error);
  CHECK(p.pointer.dragging);
  cl::playback_step(p, t, {{}, x_half, y, false}, 1280, 720, 0, 0.2, said, error);
  CHECK(!p.pointer.dragging);
  CHECK(near(t.timeline.t, p.scan.length_s / 2, 0.06));
}

TEST(the_console_clock) {
  CHECK(cl::parse_clock("83.5") == 83.5);
  CHECK(cl::parse_clock("1:23") == 83);
  CHECK(cl::parse_clock("1:02:03") == 3723);
  CHECK(cl::parse_clock("") < 0 && cl::parse_clock("a") < 0 && cl::parse_clock("1::2") < 0);
  CHECK(cl::parse_clock("-3") < 0);
}
