// engine/hub/ladder_page.hpp and the Account page's duo form: the Ladder page shows a ladder's
// season standings (the ranked first, the rest dimmed with the matches they lack), the all-time
// ratings and the past champions; a duo ladder names its duos with their players; the JSON says
// the same; a hub with no rated match says how a match is rated; the duo form asks, then makes a
// duo when the partner asks under the same name, and refuses a wrong password in words.
//
// ge:summary Test: the hub's Ladder page, its JSON, and the Account page's duo form
// ge:keywords test hub ladder-page duo-form season champions
// ge:skill net testing
#include "hub/ladder_page.hpp"

#include <string>

#include "account/matches.hpp"
#include "account/store.hpp"
#include "check.hpp"
#include "hub/account_page.hpp"
#include "hub/stats_page.hpp"

namespace ga = ge::account;
namespace gn = ge::net;
using namespace ge::hub;

namespace {

const int64_t kSeason10 = ga::kSeasonEpoch_s + int64_t{10} * 14 * 86400;
const int64_t kNow_s = kSeason10 + int64_t{15} * 86400;  // a day into season 11

HubSpec spec() {
  HubSpec s;
  s.game = "Test Game";
  s.id = "test_game";
  s.stats = true;
  return s;
}

void counter_fresh(void* ctx, uint8_t* out, size_t size) {
  auto* n = static_cast<uint64_t*>(ctx);
  for (size_t i = 0; i < size; i++) out[i] = static_cast<uint8_t>((*n * 131 + i * 17) >> 1);
  (*n)++;
}

gn::ReportPlayer seat(uint64_t account, int64_t score, const char* team = "") {
  gn::ReportPlayer p;
  p.name = "p";
  p.account = account;
  p.score = score;
  p.team = team;
  return p;
}

struct Hub {
  ge::net::Transport port;
  ga::Service svc;
  uint64_t n = 1;
  uint64_t ids[5] = {};
  gn::Key server{};
  uint64_t report_id = 1;
  Hub() {
    ga::Store store;
    ga::store_init(store, ga::HashCost{8, 1});
    ga::ServiceConfig c;
    c.seed.fill(3);
    ga::service_init(svc, c, std::move(store), port, counter_fresh, &n);
    const char* names[] = {"ann", "bob", "cid", "dee", "eve"};
    for (int i = 0; i < 5; i++) {
      ga::Fresh f{};
      f.fill(static_cast<uint8_t>(i + 1));
      ids[i] = ga::create(svc.store, names[i], "a long password", kSeason10 * 1000000, f).id;
    }
    server.fill(1);
    ga::matches_trust(svc.matches, server);
  }
  void duel(int a, int64_t sa, int b, int64_t sb, int64_t start) {
    gn::MatchReport r;
    r.id = report_id++;
    r.game = "test_game";
    r.mode = "1on1";
    r.start_s = start;
    r.players = {seat(ids[a], sa), seat(ids[b], sb)};
    ga::matches_add(svc.matches, server, r);
  }
  Answer get(const std::string& target) {
    Answer a;
    CHECK_MSG(stats_answer(spec(), &svc, target, kNow_s, a), target);
    return a;
  }
  Answer post(const std::string& path, const std::string& body) {
    AccountAsk ask{"POST", path, body, true, "198.51.100.7:51000", kNow_s * 1000000};
    Answer a;
    CHECK(account_answer(spec(), &svc, ask, a));
    return a;
  }
};

bool has(const Answer& a, const std::string& text) {
  return a.body.find(text) != std::string::npos;
}

}  // namespace

TEST(ladder_page_before_any_rated_match) {
  Hub h;
  const Answer a = h.get("/ladder");
  CHECK(a.code == 200 && has(a, "No rated matches yet") && has(a, "/account"));
  CHECK(has(h.get("/v1/ladder"), "\"ladders\":[]"));
  CHECK(h.get("/v1/ladder?mode=1on1").code == 404);
}

TEST(ladder_page_shows_the_season_all_time_and_champions) {
  Hub h;
  // Season 10: ann beats bob six times; cid and dee play twice. Season 11: ann and bob once.
  for (int i = 0; i < 6; i++) h.duel(0, 20, 1, 10, kSeason10 + 3600 + int64_t{i} * 60);
  for (int i = 0; i < 2; i++) h.duel(2, 20, 3, 10, kSeason10 + 3600 + int64_t{i} * 60);
  h.duel(1, 30, 0, 10, kNow_s - 600);
  const Answer a = h.get("/ladder?mode=1on1");
  CHECK(a.code == 200 && has(a, "Season 11") && has(a, "13 days left"));
  CHECK(has(a, "<a href=\"/ladder?game=test_game&mode=1on1\" class=\"on\">1on1</a>"));
  CHECK_MSG(has(a, "class=\"unranked\"") && has(a, "4 more to rank"), "one match this season");
  CHECK(has(a, "All time") && has(a, "Past champions"));
  // Season 10's champions: ann, then bob (cid and dee played too few).
  const size_t past = a.body.find("Past champions");
  CHECK(a.body.find("/player/ann", past) < a.body.find("/player/bob", past));
  CHECK(a.body.find("/player/cid", past) == std::string::npos);
  const Answer j = h.get("/v1/ladder?mode=1on1");
  CHECK(has(j, "\"kind\":\"duel\"") && has(j, "\"season\":11") && has(j, "\"past_seasons\":[{"));
  CHECK(has(j, "\"champions\":[{\"name\":\"ann\""));
  CHECK(has(h.get("/v1/ladder"), "\"entries\":4"));
}

TEST(ladder_page_duos_by_the_account_form) {
  Hub h;
  const Answer asked =
      h.post("/account/duo", "name=ann&password=a+long+password&partner=bob&duo=Night+Owls");
  CHECK(asked.code == 200 && has(asked, "when your partner asks"));
  const Answer made =
      h.post("/account/duo", "name=bob&password=a+long+password&partner=ann&duo=Night+Owls");
  CHECK(made.code == 200 && has(made, "Your duo is made"));
  const Answer nobody =
      h.post("/account/duo", "name=cid&password=a+long+password&partner=zed&duo=Larks");
  CHECK(nobody.code == 400 && has(nobody, "No player called zed"));
  CHECK(
      h.post("/account/duo", "name=cid&password=a+long+password&partner=dee&duo=Day+Larks").code ==
      200);
  CHECK(
      h.post("/account/duo", "name=dee&password=a+long+password&partner=cid&duo=Day+Larks").code ==
      200);
  gn::MatchReport r;
  r.id = 99;
  r.game = "test_game";
  r.mode = "2on2";
  r.start_s = kNow_s - 600;
  r.teams = {{"red", 30, 4}, {"blue", 20, 13}};
  r.players = {seat(h.ids[0], 15, "red"), seat(h.ids[1], 15, "red"), seat(h.ids[2], 10, "blue"),
               seat(h.ids[3], 10, "blue")};
  ga::matches_add(h.svc.matches, h.server, r);
  const Answer a = h.get("/ladder?mode=2on2");
  CHECK(has(a, "<b>Night Owls</b>") && has(a, "/player/ann") && has(a, "<b>Day Larks</b>"));
  CHECK(a.body.find("Night Owls") < a.body.find("Day Larks"));  // the winners above
  CHECK(has(h.get("/v1/ladder?mode=2on2"), "\"kind\":\"pair\""));
  // A wrong password is refused (last: the throttle then holds the address a while).
  const Answer wrong =
      h.post("/account/duo", "name=eve&password=nope+nope+nope&partner=ann&duo=Night+Owls");
  CHECK(wrong.code == 400);
}
