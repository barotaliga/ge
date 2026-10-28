// engine/services/discord.hpp against the simulated Discord client, and over a real local pipe:
// frames cut anywhere come back whole; an activity past Discord's limits is refused before it is
// sent; the client is found after it starts (with backoff), the activity is sent, paced and
// coalesced under the rate limit, asks to join and joins arrive as events and are answered; a quit
// app is noticed and the activity is shown again when it returns; a bad client id is refused with a
// line that says what to check. A machine with no local socket (a sandbox) skips the pipe test.
//
// ge:summary Test: Discord presence over the simulated client and over a real local pipe
// ge:keywords test services discord sim ipc frame activity rate-limit ask-to-join join local-pipe
// ge:skill services testing
#include <filesystem>
#include <string>
#include <vector>

#include "check.hpp"
#include "net/local_pipe.hpp"
#include "services/discord.hpp"
#include "services/sim_discord.hpp"

using namespace ge::services;

namespace {

constexpr int64_t kFrame = 16667;

int count(const std::vector<Event>& events, EventKind k) {
  int n = 0;
  for (const Event& e : events) n += e.kind == k;
  return n;
}

Activity racing() {
  Activity a;
  a.details = "Downhill, seed 38";
  a.state = "Gate 1 of 40";
  a.start_ms = 1792760000000;
  a.large_image = "mountain";
  a.party_id = "party-38";
  a.party_size = 1;
  a.party_max = 4;
  a.join_secret = "join-38-blue";
  return a;
}

// Poll from `from` to `to` (exclusive), a frame at a time, collecting the events.
int64_t run(Discord& d, int64_t from, int64_t to, std::vector<Event>& events) {
  for (int64_t t = from; t < to; t += kFrame) d.poll(t, events);
  return to;
}

}  // namespace

TEST(frames_come_back_whole_however_they_are_cut) {
  const std::string a = discord_frame(DiscordOp::frame, "{\"cmd\":\"DISPATCH\"}");
  const std::string b = discord_frame(DiscordOp::ping, "");
  const std::string stream = a + b;
  for (size_t cut = 0; cut <= stream.size(); cut++) {
    std::string buf = stream.substr(0, cut);
    DiscordOp op{};
    std::string json;
    int got = 0;
    while (discord_next_frame(buf, op, json) == FrameRead::whole) got++;
    buf += stream.substr(cut);
    while (discord_next_frame(buf, op, json) == FrameRead::whole) got++;
    CHECK(got == 2 && buf.empty() && op == DiscordOp::ping && json.empty());
  }
  std::string bad = discord_frame(DiscordOp::frame, "{}");
  bad[0] = 9;
  DiscordOp op{};
  std::string json;
  CHECK(discord_next_frame(bad, op, json) == FrameRead::bad);
  std::string huge;
  for (const uint32_t v : {1u, static_cast<uint32_t>(kDiscordMaxFrame + 1)})
    for (int i = 0; i < 4; i++) huge.push_back(static_cast<char>((v >> (8 * i)) & 0xff));
  CHECK(discord_next_frame(huge, op, json) == FrameRead::bad);
  CHECK(json_quote("a\"b\\c\n") == "\"a\\\"b\\\\c\\u000a\"");
}

TEST(an_activity_past_discords_limits_is_refused_before_it_is_sent) {
  CHECK(check_activity(racing()).empty());
  CHECK(check_activity(Activity{}).empty());
  Activity a = racing();
  a.details = "x";
  CHECK(check_activity(a).find("details") != std::string::npos);
  a = racing();
  a.state = std::string(kActivityTextMax + 1, 'y');
  CHECK(check_activity(a).find("next:") != std::string::npos);
  a = racing();
  a.party_size = 5;
  CHECK(!check_activity(a).empty());
  a = racing();
  a.party_id.clear();
  CHECK(check_activity(a).find("join secret") != std::string::npos);
  a = racing();
  a.end_ms = a.start_ms - 1;
  CHECK(!check_activity(a).empty());
  SimDiscord app;
  Discord d({"1234567890"}, app.link());
  a = racing();
  a.large_text = "z";
  CHECK(d.set_activity(a) == Result::invalid);
  CHECK(d.set_activity(racing()) == Result::ok);
  CHECK(activity_json(racing()) ==
        "{\"details\":\"Downhill, seed 38\",\"state\":\"Gate 1 of 40\",\"timestamps\":{\"start\":"
        "1792760000000},\"assets\":{\"large_image\":\"mountain\"},\"party\":{\"id\":\"party-38\","
        "\"size\":[1,4]},\"secrets\":{\"join\":\"join-38-blue\"},\"instance\":false}");
}

TEST(presence_reaches_the_client_and_asks_and_joins_come_back) {
  SimDiscord app(77, "ana");
  app.set_running(false);
  Discord d({"1234567890", 1000000, 8000000}, app.link());
  std::vector<Event> ev;
  CHECK(d.set_activity(racing()) == Result::ok);
  int64_t t = run(d, 0, 5000000, ev);
  CHECK(!d.connected() && ev.empty());
  const int tries = d.counts().attempts;
  CHECK_MSG(tries >= 3 && tries <= 4, "the backoff doubles: 0, 1, 3 s (and 7 s past the window)");
  app.set_running(true);
  t = run(d, t, 14000000, ev);
  CHECK(d.connected() && d.user().id == 77 && d.user().name == "ana");
  CHECK(count(ev, EventKind::connection_restored) == 1);
  CHECK(app.shown() == activity_json(racing()) && app.updates() == 1);

  // Every frame a new state: the first goes at once, the rest wait for the pace (one update in
  // 4 s) and are coalesced into the last.
  for (int gate = 2; gate <= 40; gate++) {
    Activity a = racing();
    a.state = "Gate " + std::to_string(gate) + " of 40";
    CHECK(d.set_activity(a) == Result::ok);
    d.poll(t, ev);
    t += kFrame;
  }
  CHECK(app.refusals() == 0 && app.updates() == 2);          // gate 2 went at once
  CHECK(d.counts().coalesced == 37 && d.counts().held > 0);  // gates 3 to 39 replaced, 40 waits
  t = run(d, t, t + kDiscordUpdateGapUs, ev);
  CHECK(app.updates() == 3 && app.shown().find("Gate 40 of 40") != std::string::npos);

  ev.clear();
  app.ask_to_join(88, "bo");
  app.ask_to_join(99, "cy");
  t = run(d, t, t + kFrame * 2, ev);
  CHECK(count(ev, EventKind::join_asked) == 2 && ev[0].user == 88 && ev[0].text == "bo");
  CHECK(d.answer_join(88, true) == Result::ok);
  CHECK(d.answer_join(99, false) == Result::ok);
  CHECK(d.answer_join(55, true) == Result::ok);  // never asked: the client refuses it
  app.join("their-secret");
  app.ping();
  t = run(d, t, t + kFrame * 3, ev);
  CHECK(app.answers().size() == 2 && app.answers()[0].first == 88 && app.answers()[0].second &&
        !app.answers()[1].second);
  CHECK(count(ev, EventKind::call_refused) == 1 && count(ev, EventKind::join_requested) == 1);
  for (const Event& e : ev)
    if (e.kind == EventKind::join_requested) CHECK(e.text == "their-secret");
  CHECK(app.pongs() == 1);

  // The app quits and comes back: the loss is noticed, and the activity is shown again.
  ev.clear();
  app.set_running(false);
  t = run(d, t, t + kFrame * 2, ev);
  CHECK(!d.connected() && count(ev, EventKind::connection_lost) == 1 && app.shown().empty());
  CHECK(d.answer_join(88, true) == Result::offline);
  app.set_running(true);
  t = run(d, t, t + 3000000, ev);
  CHECK(d.connected() && count(ev, EventKind::connection_restored) == 1);
  CHECK(app.shown().find("Gate 40 of 40") != std::string::npos);
  d.clear_activity();
  run(d, t, t + kDiscordUpdateGapUs + kFrame, ev);
  CHECK(app.shown().empty() && app.refusals() == 1);  // the one refusal: the answer never asked
}

TEST(a_bad_client_id_is_refused_with_what_to_check) {
  SimDiscord app;
  Discord d({"not-a-number", 1000000, 1000000}, app.link());
  std::vector<Event> ev;
  run(d, 0, 500000, ev);
  CHECK(!d.connected() && count(ev, EventKind::call_refused) == 1);
  CHECK(ev[0].id == "handshake" && ev[0].text.find("client_id") != std::string::npos &&
        ev[0].text.find("4000") != std::string::npos);
}

TEST(the_os_link_talks_to_a_client_over_a_real_local_pipe) {
  const std::string path = ge::net::local_pipe_path(std::filesystem::temp_directory_path().string(),
                                                    "ge-discord-ipc-test");
  ge::net::LocalListener server;
  std::string err;
  if (!ge::net::local_listen(server, path, &err)) {
    ge::test::skip("no local pipe here: " + err);
    return;
  }
  Discord d({"1234567890"}, discord_os_link({path + "-missing", path}));
  CHECK(d.set_activity(racing()) == Result::ok);
  std::vector<Event> ev;
  ge::net::LocalPipe peer;
  bool accepted = false;
  std::string got;  // what the game sent, as frames
  int64_t t = 0;
  int handshakes = 0, sets = 0;
  for (int i = 0; i < 20000 && sets == 0; i++, t += kFrame) {
    d.poll(t, ev);
    if (!accepted) {
      accepted = ge::net::local_accept(server, peer);
      continue;
    }
    ge::net::local_pump(peer);
    got += peer.in;
    peer.in.clear();
    DiscordOp op{};
    std::string json;
    while (discord_next_frame(got, op, json) == FrameRead::whole) {
      if (op == DiscordOp::handshake) {
        handshakes++;
        CHECK(json == "{\"v\":1,\"client_id\":\"1234567890\"}");
        ge::net::local_send(
            peer, discord_frame(DiscordOp::frame,
                                "{\"cmd\":\"DISPATCH\",\"data\":{\"user\":{\"id\":\"5\","
                                "\"username\":\"dee\"}},\"evt\":\"READY\",\"nonce\":null}"));
      } else if (json.find("SET_ACTIVITY") != std::string::npos) {
        sets++;
        CHECK(json.find(activity_json(racing())) != std::string::npos);
        CHECK(json.find("\"pid\":" + std::to_string(ge::net::local_process_id())) !=
              std::string::npos);
      }
    }
  }
  CHECK(accepted && handshakes == 1 && sets == 1);
  CHECK(d.connected() && d.user().name == "dee");
  // The client goes away: the game notices.
  ge::net::local_close(peer);
  for (int i = 0; i < 20000 && d.connected(); i++, t += kFrame) d.poll(t, ev);
  CHECK(!d.connected() && count(ev, EventKind::connection_lost) == 1);
  ge::net::local_close_listener(server);
}

TEST(the_os_link_looks_where_discord_listens) {
  const std::vector<std::string> paths = discord_ipc_paths();
  CHECK(!paths.empty());
  bool zero = false, nine = false;
  for (const std::string& p : paths) {
    zero = zero || p.ends_with("discord-ipc-0");
    nine = nine || p.ends_with("discord-ipc-9");
  }
  CHECK(zero && nine);
}
