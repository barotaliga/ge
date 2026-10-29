// engine/net/hosting.hpp and hosting_trial.hpp: an evening of hosted games over the simulated
// network. A listen server behind a home router is found by a friend on its LAN and by a player on
// the internet as one game each, its router forwards its port, both join it and chat; an
// invite-only game is never listed but joins by its invite; a host no stranger can reach is listed,
// unpinged and joined through the relay; a LAN-only game is seen by nobody; the game started shows
// as playing, and stopped is unlisted at once with its forward taken down; the trial replays.
//
// ge:summary Test: hosting from inside a game: found, merged, pinged, joined, started, unlisted
// ge:keywords test net hosting listen-server dedicated-server invite listed lan-only forward
// ge:keywords server-browser join chat unlist trial determinism
// ge:skill net testing
#include "net/hosting_trial.hpp"

#include <string>

#include "check.hpp"

using namespace ge::net;

namespace {

const TrialRow* row(const TrialRefresh& r, const std::string& name) {
  for (const TrialRow& x : r.rows) {
    if (x.name == name) return &x;
  }
  return nullptr;
}

const TrialJoin* join(const HostTrial& t, const std::string& browser, const std::string& host) {
  for (const TrialJoin& j : t.joins) {
    if (j.browser == browser && j.host == host) return &j;
  }
  return nullptr;
}

const HostTrial& lan_evening() {
  static const HostTrial t = run_host_trial({});
  return t;
}

}  // namespace

TEST(hosting_friend_and_stranger_find_each_game_once) {
  const HostTrial& t = lan_evening();
  CHECK(t.browsers.size() == 2);
  for (const TrialBrowser& b : t.browsers) {
    CHECK(b.refreshes.size() == 3);
    const TrialRefresh& r = b.refreshes[0];
    CHECK(r.done_us > 0);
    CHECK(r.rows.size() == 12);  // home, ten arenas, the attic
    CHECK(row(r, "basement") == nullptr);
    CHECK(row(r, "friends only") == nullptr);
    const TrialRow* home = row(r, "home");
    CHECK(home && home->seated == 1 && home->max_players == 8);
    CHECK(home && home->ping_ms >= 0);
    const TrialRow* attic = row(r, "attic");
    CHECK(attic && attic->ping_ms == -1 && (attic->from & kFoundListed));
    const TrialRow* locked = row(r, "arena 03");
    CHECK(locked && locked->locked);
    const TrialRow* busy = row(r, "arena 08");
    CHECK(busy && busy->phase == SessionPhase::playing && busy->seated == 3);
  }
  // The friend sees HOME on its own LAN and in the list, as one row; the stranger in the list.
  CHECK(row(t.browsers[0].refreshes[0], "home")->from == (kFoundLan | kFoundListed));
  CHECK(row(t.browsers[1].refreshes[0], "home")->from == kFoundListed);
  // By ping: the pinged first, the attic (unpinged) last.
  CHECK(t.browsers[1].refreshes[0].rows.back().name == "attic");
}

TEST(hosting_router_forwards_the_port_and_takes_it_down) {
  const HostTrial& t = lan_evening();
  CHECK(t.home_forward_ms > 0 && t.home_forward_ms < 3000);
  CHECK(t.home_forward_closed);
  CHECK(t.home.pings > 0);
  CHECK(t.home.lan_answers > 0);
  CHECK(t.home.advertised > 0);
}

TEST(hosting_joins_by_lan_by_list_and_by_invite) {
  const HostTrial& t = lan_evening();
  const TrialJoin* f = join(t, "friend", "home");
  CHECK(f && f->lan && f->seated_ms >= 0 && f->chat_heard);
  const TrialJoin* a = join(t, "away", "home");
  CHECK(a && !a->lan && a->path == ReachPath::direct && a->seated_ms >= 0 && a->chat_heard);
  const TrialJoin* attic = join(t, "away", "attic");
  CHECK(attic && attic->path == ReachPath::relay && attic->seated_ms >= 0 && attic->chat_heard);
  const TrialJoin* invite = join(t, "away", "friends only");
  CHECK(invite && invite->seated_ms >= 0 && invite->chat_heard);
}

TEST(hosting_started_shows_playing_and_stopped_is_unlisted) {
  const HostTrial& t = lan_evening();
  for (const TrialBrowser& b : t.browsers) {
    const TrialRow* home = row(b.refreshes[1], "home");
    CHECK(home && home->phase == SessionPhase::playing);
    CHECK(home && home->seated == 3);  // the host, the friend, the stranger
    CHECK(row(b.refreshes[2], "home") == nullptr);
  }
  CHECK(t.listed_at_server == 12);
  CHECK(t.listed_after_stop == 11);
}

TEST(hosting_without_a_router_still_reaches_everyone) {
  HostTrialConfig c;
  c.home_router = 0;
  const HostTrial t = run_host_trial(c);
  CHECK(t.home_forward_ms == -1);
  const TrialJoin* a = join(t, "away", "home");
  CHECK(a && a->seated_ms >= 0 && a->chat_heard);
  // No forward: a stranger's ping is filtered by the home NAT.
  CHECK(row(t.browsers[1].refreshes[0], "home")->ping_ms == -1);
  CHECK(row(t.browsers[0].refreshes[0], "home")->ping_ms >= 0);  // the friend is inside
}

TEST(hosting_trial_replays_to_the_bit) {
  HostTrialConfig c;
  conditions_preset("wifi", c.link);
  c.seed = 7;
  CHECK(run_host_trial(c).hash == run_host_trial(c).hash);
}
