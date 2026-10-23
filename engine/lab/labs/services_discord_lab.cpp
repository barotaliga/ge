// LAB `discord`: a game's Discord presence played against the simulated Discord app on one
// script: the game starts before Discord does and finds it when it starts, shows the menu, then
// a run whose gate changes the profile's line every `--update-ms` (far faster than Discord's 5
// updates in 20 s, so the client coalesces), takes two friends' asks to join (one let in, one
// turned away when the party is full), joins a friend's game from Discord, answers a ping,
// loses the app when it quits and shows the run again when it comes back, and clears the line at
// the end. It answers whether a game that updates freely stays inside Discord's limits (the
// sim refuses a sixth update in 20 s, so a refusal fails the lab) and gets every event it is
// owed, and how long the profile lags the game. Everything runs on a virtual clock.
//
// ge:summary Lab: Discord presence against the simulated Discord app: limits, asks, joins, quits
// ge:keywords lab services discord rich-presence activity rate-limit coalesce ask-to-join join
// ge:keywords reconnect journal stale
// ge:skill services labs
// ge:test tests/services_discord_test.cpp
#include <algorithm>
#include <cstdio>
#include <map>
#include <string>
#include <vector>

#include "lab/lab.hpp"
#include "services/discord.hpp"
#include "services/sim_discord.hpp"

namespace {

namespace gl = ge::lab;
namespace gs = ge::services;

constexpr int64_t kFrame = 16667;  // the game polls once a 60 Hz frame
constexpr int64_t kSecond = 1000000;
constexpr int64_t kStart = 1792760000000;  // the run's start, unix ms (a fixed day)

std::string at(int64_t t) {
  char buf[16];
  std::snprintf(buf, sizeof buf, "%.3f", static_cast<double>(t) / 1e6);
  return buf;
}

void run(gl::Lab& lab) {
  const int64_t update_us = static_cast<int64_t>(lab.arg_num("update-ms") * 1000.0);
  if (update_us < kFrame)
    lab.fail("discord: --update-ms is under a frame", "next: pass 17 or more; skill: services");
  gs::SimDiscord app(1000000000000001, "ana");
  app.set_running(false);
  gs::Discord discord({"1300000000000000038", 1 * kSecond, 8 * kSecond}, app.link());

  gs::Activity menu;
  menu.details = "In the menu";
  menu.large_image = "mountain";
  discord.set_activity(menu);

  gl::LabTable events{
      "Every event the game's poll delivered", {"t s", "event", "user", "what"}, {}};
  std::map<std::string, int> counts;
  int party = 1;
  std::string wanted_state;  // the run's line the game last set
  int64_t stale_since = -1;  // when the profile last started showing a line the game had left
  int64_t stale_max = 0;
  const int64_t end_us = 90 * kSecond;
  for (int64_t now = 0; now <= end_us; now += kFrame) {
    const int64_t frame = now / kFrame;
    app.tick(now);
    if (frame == 4 * 60) app.set_running(true);
    const bool racing = now >= 10 * kSecond && now < 70 * kSecond;
    if (racing && (now - 10 * kSecond) % update_us < kFrame) {
      const int gate = static_cast<int>((now - 10 * kSecond) / update_us) + 1;
      gs::Activity a;
      a.details = "Downhill, seed 38";
      a.state = "Gate " + std::to_string(gate) + " of 120";
      a.start_ms = kStart;
      a.large_image = "mountain";
      a.party_id = "party-38";
      a.party_size = party;
      a.party_max = 2;
      if (party < 2) a.join_secret = "join-38";
      if (discord.set_activity(a) != gs::Result::ok)
        lab.fail("discord: the run's activity was refused: " + gs::check_activity(a),
                 "next: fix the lab's activity; skill: services");
      wanted_state = a.state;
    }
    if (now >= 70 * kSecond && now < 70 * kSecond + kFrame) {
      discord.clear_activity();
      wanted_state.clear();
    }
    if (frame == 20 * 60) app.ask_to_join(2000000000000002, "bo");
    if (frame == 22 * 60) app.ask_to_join(3000000000000003, "cy");
    if (frame == 25 * 60) app.join("join-friend-77");
    if (frame == 30 * 60) app.ping();
    if (frame == 45 * 60) app.set_running(false);
    if (frame == 50 * 60) app.set_running(true);

    std::vector<gs::Event> out;
    discord.poll(now, out);
    for (const gs::Event& e : out) {
      const std::string kind(gs::event_name(e.kind));
      counts[kind]++;
      events.rows.push_back(
          {at(now), kind, e.user ? std::to_string(e.user) : "", e.id + " " + e.text});
      if (e.kind == gs::EventKind::join_asked) {
        const bool room = party < 2;
        discord.answer_join(e.user, room);
        if (room) party++;
      }
    }
    // How long the profile shows a line the game has moved past, while Discord is there to show
    // it: the rate limit's cost.
    const bool behind = discord.connected() && !wanted_state.empty() &&
                        app.shown().find("\"" + wanted_state + "\"") == std::string::npos;
    if (!behind) {
      stale_since = -1;
    } else {
      if (stale_since < 0) stale_since = now;
      stale_max = std::max(stale_max, now - stale_since);
    }
  }

  const auto owed = [&](const char* kind, int n) {
    if (counts[kind] != n)
      lab.fail("discord: " + std::string(kind) + " arrived " + std::to_string(counts[kind]) +
                   " times, the script is owed " + std::to_string(n),
               "read the events table and the app's journal; skill: services");
  };
  owed("connection_restored", 2);
  owed("connection_lost", 1);
  owed("join_asked", 2);
  owed("join_requested", 1);
  owed("call_refused", 0);
  if (app.refusals() != 0)
    lab.fail("discord: the app refused " + std::to_string(app.refusals()) + " frames",
             "next: read the journal's ERROR lines; skill: services");
  const auto answers = app.answers();
  if (answers.size() != 2 || !answers[0].second || answers[1].second)
    lab.fail("discord: the asks were not answered let-in then turned-away",
             "next: read the journal; skill: services");
  if (!app.shown().empty())
    lab.fail("discord: the profile still shows a line after the run was cleared",
             "next: check Discord::clear_activity; skill: services");
  if (app.pongs() != 1)
    lab.fail("discord: the ping went unanswered", "next: check the ping op; skill: services");

  const gs::DiscordCounts c = discord.counts();
  lab.metric("attempts", c.attempts, "count");
  lab.metric("connects", c.connects, "count");
  lab.metric("updates-sent", c.sent, "count");
  lab.metric("updates-coalesced", c.coalesced, "count");
  lab.metric("polls-held", c.held, "count");
  lab.metric("updates-shown", app.updates(), "count");
  lab.metric("stale-max", static_cast<double>(stale_max) / 1000.0, "ms");
  lab.table(std::move(events));
  gl::LabTable journal{"The app's journal: every frame either side sent", {"line"}, {}};
  for (const std::string& line : app.journal()) journal.rows.push_back({line});
  lab.table(std::move(journal));
}

const gl::LabSpec kSpec{
    "discord",
    gl::LabKind::scenario,
    "engine",
    "engine/services",
    "services",
    "Does a game that updates its Discord presence freely stay inside Discord's limits and get "
    "every event it is owed (asks, joins, the app quitting and returning), and how long does the "
    "profile show a line the game has moved past?",
    {
        {"update-ms", "500", "how often the run changes the profile's line"},
    },
    "",
    run,
};
GE_LAB(kSpec);

}  // namespace
