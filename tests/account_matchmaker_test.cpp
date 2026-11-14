// Tests: the matchmaker (engine/account/matchmaker.hpp): two close ratings paired at once on an
// offered server; far ratings wait until their windows widen; no pair without a server's seats;
// duos paired two-on-two by their pair rating; a duo asked for that the asker is not in refused;
// a lapsed entry dropped; the oldest paired with the nearest rating first.
//
// ge:summary Tests: the matchmaker's queue, window, offers, duos and lapses
// ge:keywords test matchmaker queue rating-window offer duo
// ge:skill net testing
#include "account/matchmaker.hpp"

#include "check.hpp"

namespace ga = ge::account;

namespace {

constexpr int64_t kT = 1794500000;

ge::net::Key server_key(uint8_t b) {
  ge::net::Key k{};
  k[0] = b;
  return k;
}

// A ladder where `who` has `rating` (tenths) on the game's duel ladder.
void rate(ga::Ladder& l, uint64_t who, int64_t rating) {
  l.tables[{"se", "duel", ga::LadderKind::duel}].all_time[who].rating = rating;
}

}  // namespace

TEST(matchmaker_pairs_close_ratings_at_once_on_an_offered_server) {
  ga::Ladder l;
  rate(l, 1, 15000);
  rate(l, 2, 15400);
  ga::Matchmaker mm;
  CHECK(ga::matchmaker_join(mm, l, 1, "se", "duel", "", kT) == ga::QueueAnswer::queued);
  CHECK(ga::matchmaker_join(mm, l, 2, "se", "duel", "", kT) == ga::QueueAnswer::queued);
  CHECK(ga::matchmaker_pair(mm, kT).empty());  // no server yet
  CHECK(ga::matchmaker_look(mm, 1, kT + 5).state == ga::LookState::waiting);
  ga::matchmaker_offer(mm, server_key(1), "se", "duel", "10.0.0.7:27500", 8, kT + 5);
  const std::vector<uint64_t> made = ga::matchmaker_pair(mm, kT + 5);
  CHECK(made.size() == 1);
  const ga::Look a = ga::matchmaker_look(mm, 1, kT + 6);
  CHECK(a.state == ga::LookState::matched);
  if (a.match == nullptr) return;
  CHECK(a.match->address == "10.0.0.7:27500");
  CHECK(a.match->side_a.size() == 1 && a.match->side_b.size() == 1);
  CHECK(ga::matchmaker_look(mm, 2, kT + 6).match == a.match);
  CHECK(mm.offers[0].seats == 6);
  CHECK(mm.queue.empty());
}

TEST(matchmaker_far_ratings_wait_until_their_windows_widen) {
  ga::Ladder l;
  rate(l, 1, 15000);
  rate(l, 2, 17000);  // 200 points apart
  ga::Matchmaker mm;
  ga::matchmaker_offer(mm, server_key(1), "se", "duel", "10.0.0.7:27500", 8, kT);
  for (int64_t t = kT; t < kT + 20; t++) {
    ga::matchmaker_join(mm, l, 1, "se", "duel", "", t);
    ga::matchmaker_join(mm, l, 2, "se", "duel", "", t);
    CHECK_MSG(ga::matchmaker_pair(mm, t).empty(), "paired too soon at +" + std::to_string(t - kT));
  }
  // At +20 s the window is 100 + 2 * 50 = 200 points: they meet.
  ga::matchmaker_offer(mm, server_key(1), "se", "duel", "10.0.0.7:27500", 8, kT + 20);
  ga::matchmaker_join(mm, l, 1, "se", "duel", "", kT + 20);
  ga::matchmaker_join(mm, l, 2, "se", "duel", "", kT + 20);
  CHECK(ga::queue_window(kT, kT + 20) == 2000);
  CHECK(ga::matchmaker_pair(mm, kT + 20).size() == 1);
}

TEST(matchmaker_the_oldest_gets_the_nearest_rating) {
  ga::Ladder l;
  rate(l, 1, 15000);
  rate(l, 2, 15800);
  rate(l, 3, 15100);
  ga::Matchmaker mm;
  ga::matchmaker_offer(mm, server_key(1), "se", "duel", "a:1", 2, kT);
  for (uint64_t who = 1; who <= 3; who++) ga::matchmaker_join(mm, l, who, "se", "duel", "", kT);
  CHECK(ga::matchmaker_pair(mm, kT).size() == 1);
  const ga::Look one = ga::matchmaker_look(mm, 1, kT);
  CHECK(one.match != nullptr && one.match->side_b[0] == 3);
  // The server's two seats are taken: the third waits.
  CHECK(ga::matchmaker_look(mm, 2, kT).state == ga::LookState::waiting);
}

TEST(matchmaker_pairs_duos_two_on_two_and_refuses_a_duo_not_yours) {
  ga::Ladder l;
  CHECK(ga::ladder_duo_ask(l, 1, 2, "Reds") == ga::DuoAnswer::asked);
  CHECK(ga::ladder_duo_ask(l, 2, 1, "Reds") == ga::DuoAnswer::made);
  CHECK(ga::ladder_duo_ask(l, 3, 4, "Blues") == ga::DuoAnswer::asked);
  CHECK(ga::ladder_duo_ask(l, 4, 3, "Blues") == ga::DuoAnswer::made);
  ga::Matchmaker mm;
  CHECK(ga::matchmaker_join(mm, l, 5, "se", "2on2", "Reds", kT) == ga::QueueAnswer::no_duo);
  CHECK(ga::matchmaker_join(mm, l, 2, "se", "2on2", "Reds", kT) == ga::QueueAnswer::queued);
  CHECK(ga::matchmaker_join(mm, l, 3, "se", "2on2", "Blues", kT) == ga::QueueAnswer::queued);
  ga::matchmaker_offer(mm, server_key(1), "se", "2on2", "a:1", 3, kT);
  CHECK(ga::matchmaker_pair(mm, kT).empty());  // 3 seats are not 4
  ga::matchmaker_offer(mm, server_key(1), "se", "2on2", "a:1", 4, kT);
  CHECK(ga::matchmaker_pair(mm, kT).size() == 1);
  const ga::Look a = ga::matchmaker_look(mm, 1, kT);  // the partner who did not ask is in it too
  CHECK(a.state == ga::LookState::matched);
  if (a.match == nullptr) return;
  CHECK(a.match->side_a.size() == 2 && a.match->side_b.size() == 2);
}

TEST(matchmaker_drops_an_entry_not_asked_for_again) {
  ga::Ladder l;
  ga::Matchmaker mm;
  ga::matchmaker_join(mm, l, 1, "se", "duel", "", kT);
  ga::matchmaker_pair(mm, kT + ga::kQueueLife_s + 1);
  CHECK(ga::matchmaker_look(mm, 1, kT + ga::kQueueLife_s + 1).state == ga::LookState::none);
  ga::matchmaker_join(mm, l, 1, "se", "duel", "", kT);
  CHECK(ga::matchmaker_leave(mm, 1));
  CHECK(!ga::matchmaker_leave(mm, 1));
  CHECK(ga::matchmaker_join(mm, l, 1, "se", "", "", kT) == ga::QueueAnswer::bad_mode);
}
