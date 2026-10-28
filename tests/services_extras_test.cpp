// The handheld and the extras on engine/services/sim_store.hpp: the on-screen keyboard open only
// where the player has no keyboard and closed by typing or dismissing it; the machine sleeping
// and waking, its server calls answering offline meanwhile; the timeline's marks held to a
// store's limits; remote play for an invited friend only.
//
// ge:summary Test: the simulated store's on-screen keyboard, sleep, timeline and remote play
// ge:keywords test services on-screen-keyboard text_entered suspend resume timeline remote-play
// ge:skill services testing
#include <array>
#include <string>
#include <vector>

#include "check.hpp"
#include "services/sim_store.hpp"

using namespace ge::services;

namespace {

constexpr std::array<StatDef, 1> kStats{{{"RUNS", StatKind::integer, 0, 1000000, true}}};
const Catalog kCatalog{3001, {}, kStats, {}, {}};

std::vector<Event> poll(Backend& b, int64_t t) {
  std::vector<Event> ev;
  b.poll(t, ev);
  return ev;
}

}  // namespace

TEST(the_keyboard_opens_only_on_a_handheld_and_answers_with_the_text) {
  SimStore store(kCatalog, {});
  const UserId ana = store.add_user("ana");
  auto a = store.backend(ana);
  CHECK(a->show_keyboard(KeyboardMode::single_line, "Name", "", 16) == Result::unsupported);
  Environment env;
  env.handheld = true;
  store.set_environment(ana, env);
  CHECK(a->show_keyboard(KeyboardMode::single_line, std::string(257, 'p'), "", 16) ==
        Result::invalid);
  CHECK(a->show_keyboard(KeyboardMode::single_line, "Name", "", 0) == Result::invalid);
  CHECK(a->show_keyboard(KeyboardMode::single_line, "Name", "", 1025) == Result::invalid);
  CHECK(a->show_keyboard(KeyboardMode::single_line, "Name", "", 16) == Result::ok);
  CHECK(a->show_keyboard(KeyboardMode::single_line, "Again", "", 16) == Result::invalid);
  CHECK(!store.type_text(ana, std::string(17, 'x')));  // past the limit: still open
  CHECK(store.type_text(ana, "Ana the Bold"));
  std::vector<Event> ev = poll(*a, 0);
  CHECK(ev.size() == 1 && ev[0].kind == EventKind::text_entered && ev[0].changed &&
        ev[0].text == "Ana the Bold");
  CHECK(!store.type_text(ana, "again"));  // closed

  env = {};
  env.big_picture = true;
  store.set_environment(ana, env);
  CHECK(a->show_keyboard(KeyboardMode::numeric, "Seed", "38", 10) == Result::ok);
  CHECK(store.dismiss_keyboard(ana));
  ev = poll(*a, 0);
  CHECK(ev.size() == 1 && ev[0].kind == EventKind::text_entered && !ev[0].changed);
  CHECK(!store.dismiss_keyboard(ana));
}

TEST(a_sleeping_machine_answers_offline_and_wakes_with_the_time_asleep) {
  SimConfig config;
  config.latency_us = 1000;
  SimStore store(kCatalog, config);
  const UserId ana = store.add_user("ana");
  auto a = store.backend(ana);
  poll(*a, 5000000);
  store.suspend(ana);
  std::vector<Event> ev = poll(*a, 5000000);
  CHECK(ev.size() == 1 && ev[0].kind == EventKind::suspended);
  a->query_items({});
  ev = poll(*a, 65000000);
  CHECK(ev.size() == 1 && ev[0].result == Result::offline);
  store.resume(ana);
  ev = poll(*a, 65000000);
  CHECK(ev.size() == 1 && ev[0].kind == EventKind::resumed && ev[0].total == 60);
  a->query_items({});
  ev = poll(*a, 66000000);
  CHECK(ev.size() == 1 && ev[0].result == Result::ok);
}

TEST(the_timeline_keeps_marks_within_a_stores_limits) {
  SimStore store(kCatalog, {});
  const UserId ana = store.add_user("ana");
  auto a = store.backend(ana);
  TimelineMark m{"steam_crown", "Personal best", "1:42.31 on seed 38", 100, -30, 30};
  CHECK(a->timeline_mark(m) == Result::ok);
  TimelineMark bad = m;
  bad.title.clear();
  CHECK(a->timeline_mark(bad) == Result::invalid);
  bad = m;
  bad.title = std::string(129, 't');
  CHECK(a->timeline_mark(bad) == Result::invalid);
  bad = m;
  bad.priority = 1001;
  CHECK(a->timeline_mark(bad) == Result::invalid);
  bad = m;
  bad.start_offset_s = -601;
  CHECK(a->timeline_mark(bad) == Result::invalid);
  bad = m;
  bad.duration_s = -1;
  CHECK(a->timeline_mark(bad) == Result::invalid);
  CHECK(a->timeline_mode(TimelineMode::playing) == Result::ok);
  const std::vector<TimelineMark> marks = store.timeline(ana);
  CHECK(marks.size() == 1 && marks[0].title == "Personal best");
}

TEST(remote_play_is_for_an_invited_friend) {
  SimStore store(kCatalog, {});
  const UserId ana = store.add_user("ana"), bo = store.add_user("bo"), cy = store.add_user("cy");
  store.befriend(ana, bo);
  auto a = store.backend(ana);
  CHECK(a->invite_remote_play(cy) == Result::not_found);  // not a friend
  CHECK(a->invite_remote_play(999) == Result::not_found);
  CHECK(!store.remote_join(ana, bo, RemoteDevice::tv));  // not invited
  CHECK(a->invite_remote_play(bo) == Result::ok);
  CHECK(store.remote_join(ana, bo, RemoteDevice::tv));
  std::vector<Event> ev = poll(*a, 0);
  CHECK(ev.size() == 1 && ev[0].kind == EventKind::remote_player_joined && ev[0].user == bo &&
        ev[0].text == "bo");
  std::vector<RemotePlayer> in;
  a->remote_players(in);
  CHECK(in.size() == 1 && in[0].user == bo && in[0].device == RemoteDevice::tv);
  CHECK(!store.remote_join(ana, bo, RemoteDevice::tv));  // the invite was used
  CHECK(store.remote_leave(ana, bo));
  ev = poll(*a, 0);
  CHECK(ev.size() == 1 && ev[0].kind == EventKind::remote_player_left && ev[0].user == bo);
  a->remote_players(in);
  CHECK(in.empty() && !store.remote_leave(ana, bo));
}
