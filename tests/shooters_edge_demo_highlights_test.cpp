// A demo's highlights and its playback (games/shooters_edge/client/demo_scan.hpp,
// demo_highlights.hpp, demo_playback.hpp, timedemo.hpp's timeline and seek): the scripted match
// (demo_write.hpp's `highlight_match`) scanned into its kills with the right killers, weapons,
// rocket flights, air, gibs, stacks and telefrags; the highlights found for the watched player and
// for another; the timeline's seek forward and back landing on the straight read; the playback's
// keys, skips, jumps, speeds and pause; the console's clock and `demo_jump`. The scan also keeps
// where killer and victim stood and every rocket's flight with its shooter (the `se-kills` lab's
// input).
//
// ge:summary Tests: a demo's kills and highlights found, the timeline's seek, the playback controls
// ge:keywords test demo highlights scan kills multi-kill long-shot air-kill annihilation telefrag
// ge:keywords streak seek timeline playback skip jump speed pause parse_clock flights shooter
// ge:keywords demo_jump demo_snapshots cl_demosnaps seek-snapshot
// ge:skill shooter replay testing
// ge:test tests/shooters_edge_demo_highlights_test.cpp
#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

#include "check.hpp"
#include "console/console.hpp"
#include "shooter/demo_transport.hpp"
#include "shooters_edge/client/demo_autotrack.hpp"
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

TEST(the_scan_keeps_where_each_kill_stood_and_every_flight_with_its_shooter) {
  const cl::DemoScan& s = scanned();
  CHECK(s.map == "halls");
  // 10 s: from the hall's south-west corner onto the far end of the vault.
  const cl::DemoKill& k = s.kills[2];
  CHECK(near(k.killer_at.x, 40, 1) && near(k.killer_at.y, 40, 1));
  CHECK(near(k.victim_at.x, 735, 1) && near(k.victim_at.y, 795, 1));
  CHECK_MSG(k.shot && near(k.shot_from.x, 40, 40) && near(k.shot_to.x, 730, 60) &&
                near(k.shot_to.y, 790, 60),
            "the rocket matched went from " + std::to_string(k.shot_from.x) + " to " +
                std::to_string(k.shot_to.x) + " " + std::to_string(k.shot_to.y));
  CHECK(!s.kills[4].shot && !s.kills[5].shot);  // the beam and the shotgun fly nothing
  // Every rocket flown is kept with the player it left from: slot 0's three, slot 3's one.
  CHECK(s.flights.size() == 4);
  int by0 = 0, by3 = 0;
  for (const cl::DemoFlight& f : s.flights) {
    CHECK(!f.grenade && f.t1 > f.t0);
    by0 += f.shooter == 0 ? 1 : 0;
    by3 += f.shooter == 3 ? 1 : 0;
  }
  CHECK_MSG(by0 == 3 && by3 == 1, "the shooters read " + std::to_string(by0) + " for slot 0 and " +
                                      std::to_string(by3) + " for slot 3");
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

TEST(a_seek_back_starts_from_the_last_snapshot_and_lands_as_the_straight_read) {
  cl::Timedemo straight = opened();
  std::string error;
  while (straight.timeline.t < 17.0) cl::timedemo_step(straight, error);
  cl::Timedemo t = opened();
  t.snap_every_s = 5;
  CHECK(cl::demo_seek(t, 26, error) == cl::TimedemoStep::new_level);
  // One at the level's first frame, then one each 5 s of the timeline after the last.
  CHECK_MSG(t.snaps.size() >= 5, "snapshots kept: " + std::to_string(t.snaps.size()));
  for (size_t i = 1; i < t.snaps.size(); i++) {
    CHECK(t.snaps[i].timeline.t >= t.snaps[i - 1].timeline.t + 5);
    CHECK(t.snaps[i].demo.bytes.empty());  // the bytes are the demo's own, never copied
  }
  CHECK(t.demo.bytes.size() == t.start.bytes.size());
  CHECK(cl::demo_seek(t, 17, error) == cl::TimedemoStep::frame);  // back: from a snapshot
  CHECK(!t.snaps.empty() && t.snaps.back().timeline.t <= 17.0);   // the later ones let go
  CHECK(t.timeline.t == straight.timeline.t);
  CHECK(t.demo.state.frags == straight.demo.state.frags);
  CHECK(t.demo.pos == straight.demo.pos);
  for (size_t k = 0; k < 4; k++)
    CHECK(t.demo.state.players[k].origin.x == straight.demo.state.players[k].origin.x);
  const double at17 = straight.timeline.t;
  const size_t pos17 = straight.demo.pos;
  // Read on from there, it is the straight read still.
  while (straight.timeline.t < 22.0) cl::timedemo_step(straight, error);
  CHECK(cl::demo_seek(t, straight.timeline.t, error) == cl::TimedemoStep::frame);
  CHECK(t.demo.pos == straight.demo.pos && t.demo.state.frags == straight.demo.state.frags);
  // None kept (demo_snapshots 0): a seek back reads from the start, to the same place.
  cl::Timedemo none = opened();
  CHECK(cl::demo_seek(none, 26, error) == cl::TimedemoStep::new_level);
  CHECK(none.snaps.empty());
  CHECK(cl::demo_seek(none, 17, error) == cl::TimedemoStep::frame);
  CHECK(none.timeline.t == at17 && none.demo.pos == pos17);
}

TEST(a_share_seek_with_no_known_length_says_so) {
  cl::DemoPlayback p;
  cl::Timedemo t = opened();
  std::string error;
  CHECK(cl::playback_begin(p, t, error));
  p.scan.length_s = 0;  // as when the demo could not be scanned
  p.share_ask = 0.5;
  std::vector<std::string> said;
  cl::playback_step(p, t, {}, 1280, 720, 0, 0, said, error);
  CHECK(said.size() == 1 && said[0].find("length is not known") != std::string::npos);
  CHECK(t.timeline.t < 1);  // not moved
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

TEST(demo_jump_goes_to_a_time_or_on_and_back_by_one) {
  ge::console::Console con{false};
  ge::shooter::DemoTransport t;
  ge::shooter::add_demo_transport(con, t, {});
  con.run_now("demo_jump 1:30");
  CHECK(t.seek_ask == 90);
  con.run_now("demo_jump +15");
  con.run_now("demo_jump -0:05");
  CHECK(t.skip_ask == 10);
  t.seek_ask = -1;
  con.run_now("demo_jump 1:2:3");  // no hours
  con.run_now("demo_jump x");
  CHECK(t.seek_ask < 0 && t.skip_ask == 10);
}

TEST(the_demo_clock_says_hours_minutes_and_seconds) {
  CHECK(cl::demo_clock_text(0) == "00:00:00");
  CHECK(cl::demo_clock_text(-3) == "00:00:00");
  CHECK(cl::demo_clock_text(59.99) == "00:00:59");
  CHECK(cl::demo_clock_text(3725.5) == "01:02:05");
  CHECK(cl::demo_clock_text(100 * 3600 + 61) == "00:01:01");  // the hours start again past 99
}

TEST(cl_demowait_holds_the_last_frame_paused_and_a_seek_back_plays_again) {
  cl::DemoPlayback p;
  cl::Timedemo t = opened();
  std::string error;
  CHECK(cl::playback_begin(p, t, error));
  std::vector<std::string> said;
  const auto step = [&](double dt) {
    return cl::playback_step(p, t, {{}, -1, -1, false}, 1280, 720, dt, 0, said, error);
  };
  step(0);
  p.seek_ask = p.scan.length_s;  // a moment before the end
  step(0);
  cl::TimedemoStep got = cl::TimedemoStep::frame;
  for (int i = 0; i < 400 && got != cl::TimedemoStep::end; i++) got = step(0.05);
  CHECK(got == cl::TimedemoStep::end);  // 0: the demo ends
  cl::Timedemo held = opened();
  CHECK(cl::playback_begin(p, held, error));
  p.hold_end = 1;
  cl::playback_step(p, held, {{}, -1, -1, false}, 1280, 720, 0, 0, said, error);
  p.seek_ask = p.scan.length_s;
  for (int i = 0; i < 400; i++) {
    got = cl::playback_step(p, held, {{}, -1, -1, false}, 1280, 720, 0.05, 0, said, error);
    CHECK(got != cl::TimedemoStep::end);
  }
  CHECK(p.paused);  // held on the last frame
  p.seek_ask = 5;
  got = cl::playback_step(p, held, {{}, -1, -1, false}, 1280, 720, 0, 0, said, error);
  CHECK(got != cl::TimedemoStep::end && near(held.timeline.t, 5, 0.051));
}

TEST(quake_ii_s_seek_takes_tenths_of_a_second_and_shares_of_the_demo) {
  CHECK(ge::shooter::parse_tenths_clock("12") == 12);
  CHECK(ge::shooter::parse_tenths_clock("1.25") == 3.5);  // 1 s and 25 tenths
  CHECK(ge::shooter::parse_tenths_clock("1:05.3") == 65.3);
  CHECK(ge::shooter::parse_tenths_clock("1:2:3") < 0 && ge::shooter::parse_tenths_clock("") < 0);
  CHECK(ge::shooter::parse_tenths_clock("1.2.3") < 0 && ge::shooter::parse_tenths_clock("a") < 0);
  CHECK(ge::shooter::parse_tenths_clock("1.") < 0 && ge::shooter::parse_tenths_clock(".5") < 0);
  ge::console::Console con{false};
  std::vector<std::string> printed;
  con.set_sink([&printed](std::string_view l) { printed.emplace_back(l); });
  ge::shooter::DemoTransport t;
  ge::shooter::add_demo_transport(con, t, {});
  con.add_command_synonym("seek", "demo_jump");
  con.run_now("seek 1.25");
  CHECK(t.seek_ask == 3.5);
  con.run_now("seek -1:00");
  CHECK(t.skip_ask == -60);
  con.run_now("seek 50%");
  CHECK(t.share_ask == 0.5);
  con.run_now("seek +10%");
  CHECK(near(t.share_skip_ask, 0.1, 1e-12));
  con.run_now("demo_jump 1.25");  // our own spelling: seconds
  CHECK(t.seek_ask == 1.25);
  printed.clear();
  con.run_now("seek x%");
  con.run_now("seek 1:x");
  con.run_now("seek +1:x");
  con.run_now("seek");
  CHECK(printed.size() == 4 && printed[0] == "Invalid percentage." &&
        printed[1] == "Invalid absolute timespec." && printed[2] == "Invalid relative timespec." &&
        printed[3] == "Usage: seek [+-]<timespec|percent>[%]");
  {  // no demo playing: said so, nothing asked
    ge::console::Console idle{false};
    std::vector<std::string> said;
    idle.set_sink([&said](std::string_view l) { said.emplace_back(l); });
    ge::shooter::DemoTransport u;
    ge::shooter::DemoCalls none;
    none.playing = [] { return false; };
    ge::shooter::add_demo_transport(idle, u, none);
    idle.add_command_synonym("seek", "demo_jump");
    idle.run_now("seek 1:00");
    CHECK(said.size() == 1 && said[0] == "Not playing a demo." && u.seek_ask < 0);
  }
}

TEST(demo_jump_end_lands_two_seconds_before_the_end_and_shares_seek) {
  cl::DemoPlayback p;
  cl::Timedemo t = opened();
  std::string error;
  CHECK(cl::playback_begin(p, t, error));
  std::vector<std::string> said;
  const auto step = [&] {
    return cl::playback_step(p, t, {{}, -1, -1, false}, 1280, 720, 0, 0, said, error);
  };
  step();
  const double length = std::max(p.scan.length_s, t.timeline.t);
  p.end_ask = true;
  step();
  CHECK_MSG(near(t.timeline.t, std::ceil(length) - 2, 0.051), std::to_string(t.timeline.t));
  CHECK(p.jumped);
  said.clear();
  p.end_ask = true;  // already there
  step();
  CHECK(said.size() == 1 && said[0] == "Too close to the end of the demo.");
  p.share_ask = 0.5;
  step();
  CHECK_MSG(near(t.timeline.t, length / 2, 0.051), std::to_string(t.timeline.t));
}

TEST(demo_jump_skip_messages_keeps_or_drops_the_lines_a_jump_passes) {
  // The single-player recording prints "halfway" at its middle frame.
  const std::vector<uint8_t> bytes =
      cl::write_single_demo(cl::loop_script("halls", {0, 0, 0}, 100, 60));
  std::string error;
  for (const bool keep : {false, true}) {
    cl::Timedemo t;
    CHECK(cl::open_demo(bytes, t.demo, error));
    t.start = t.demo;
    t.hear = true;
    t.seek_prints = keep;
    cl::demo_seek(t, 1e9, error);
    CHECK_MSG(t.heard.prints.size() == (keep ? 1u : 0u), std::to_string(t.heard.prints.size()));
  }
}

TEST(demo_jump_end_stops_paused_at_an_intermission_on_the_way) {
  cl::MatchScript m = cl::highlight_match();
  m.intermission = 400;  // 20 s
  cl::Timedemo t;
  std::string error;
  CHECK(cl::open_demo(cl::write_multiview_match(m), t.demo, error));
  t.start = t.demo;
  t.active = true;
  cl::DemoPlayback p;
  CHECK(cl::playback_begin(p, t, error));
  std::vector<std::string> said;
  cl::playback_step(p, t, {{}, -1, -1, false}, 1280, 720, 0, 0, said, error);
  p.end_ask = true;
  cl::playback_step(p, t, {{}, -1, -1, false}, 1280, 720, 0, 0, said, error);
  CHECK(t.demo.state.intermission && p.paused);
  CHECK_MSG(near(t.timeline.t, 20, 0.06), std::to_string(t.timeline.t));
}

TEST(cl_hightrack_rides_the_top_fragger_and_changes_only_for_more) {
  cl::DemoState s;
  s.players.resize(32);
  s.names[0] = "a";
  s.names[1] = "b";
  s.names[2] = "watcher";
  s.spectator[2] = true;
  s.frags[0] = 3;
  s.frags[1] = 5;
  s.frags[2] = 99;  // a spectator is never ridden
  CHECK(cl::high_track_slot(s, 0) == 1);
  s.frags[0] = 5;
  CHECK(cl::high_track_slot(s, 1) == 1 && cl::high_track_slot(s, 0) == 0);  // a tie keeps him
  s.frags[0] = 6;
  CHECK(cl::high_track_slot(s, 1) == 0);
  CHECK(cl::high_track_slot(s, 2) == 0 && cl::high_track_slot(s, -1) == 0);
  CHECK(cl::high_track_slot(cl::DemoState{}, 0) == -1);
}

TEST(mvd_autotrack_follows_the_plan_and_your_own_pick_turns_it_off) {
  ge::console::Console con{false};
  cl::DemoPlayback p;
  cl::Timedemo t = opened();
  cl::add_playback_commands(con, p, t);
  std::string error;
  CHECK(cl::playback_begin(p, t, error));
  con.run_now("autotrack");
  CHECK(p.autotrack == 1);
  std::vector<std::string> said;
  bool followed = false;
  for (int i = 0; i < 200; i++) {
    cl::playback_step(p, t, {{}, -1, -1, false}, 1280, 720, 0.05, 0, said, error);
    const int slot = cl::autotrack_at(p.plan, t.timeline.t);
    if (slot >= 0) {
      CHECK(t.demo.track == slot);
      followed = true;
    }
  }
  CHECK(p.plan_state == 1 && followed);
  con.run_now("demo_track 1");
  CHECK(p.autotrack == 0 && t.demo.track == 1);
}
