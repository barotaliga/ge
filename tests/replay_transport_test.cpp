// The transport (engine/replay/transport.hpp): play and the end pausing, play at the end starting
// again, the speed ladder held at its ends, skip and frame step held inside the replay, a frame
// step pausing, and a growing length.
//
// ge:summary Tests: the replay transport's play, pause, speeds, skip, frame step and length
// ge:keywords test replay transport play pause speed skip frame seek tick
// ge:skill replay testing
// ge:test tests/replay_transport_test.cpp
#include "check.hpp"
#include "replay/transport.hpp"

namespace gr = ge::replay;

TEST(plays_at_its_speed_and_pauses_at_the_end) {
  gr::Transport t(120, 600);
  CHECK(!t.playing() && t.at() == 0);
  t.play();
  CHECK(t.tick(1) == 120);
  t.faster(1);
  CHECK(t.speed() == 2);
  CHECK(t.tick(1) == 360);
  CHECK(t.tick(10) == 600 && !t.playing() && t.at_end());
  t.play();  // at the end: from the first step again
  CHECK(t.playing() && t.at() == 0);
}

TEST(speeds_hold_at_their_ends) {
  gr::Transport t(120, 600);
  for (int i = 0; i < 6; i++) t.faster(-1);
  CHECK(t.speed() == 0.25);
  t.play();
  t.tick(0.5);
  CHECK(t.position() == 15);
  for (int i = 0; i < 6; i++) t.faster(1);
  CHECK(t.speed() == 2);
}

TEST(skip_and_frame_stay_inside) {
  gr::Transport t(120, 1000);
  t.skip(1);
  CHECK(t.at() == 600);
  t.skip(1);
  CHECK(t.at() == 1000);
  t.seek(100);
  t.skip(-1);
  CHECK(t.at() == 0);
  t.play();
  t.frame(1);  // a frame step pauses first
  CHECK(!t.playing() && t.at() == 4);
  t.frame(-1);
  t.frame(-1);
  CHECK(t.at() == 0);
  gr::Transport slow(10, 10);
  slow.frame(1);  // never less than a step
  CHECK(slow.at() == 1);
}

TEST(a_growing_length) {
  gr::Transport t(120, 100);
  t.seek(100);
  t.set_length(50);
  CHECK(t.at() == 50);
  t.set_length(500);
  t.play();
  CHECK(t.tick(1) == 170);
  gr::Transport empty(120, 0);
  empty.play();
  CHECK(!empty.playing());
}
