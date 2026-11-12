// Tests: the account store (engine/account/store.hpp, store_text.hpp): sign-up and the name's
// reservation, sign-in, the throttle's holds, login codes and the game password (and that a
// classic login never takes the password), recovery, blocking, removal and the name freed, and
// the log and snapshot read back into the same store. A cheap hash cost keeps it fast.
//
// ge:summary Tests: account sign-up, sign-in, throttle, codes, recovery, the log
// ge:keywords test account store create sign_in login_game make_code recover throttle hold
// ge:keywords store_text store_read log snapshot
// ge:skill net testing
#include <string>

#include "account/store.hpp"
#include "account/store_text.hpp"
#include "check.hpp"

namespace ga = ge::account;

namespace {

constexpr int64_t kSecond = 1000000;
constexpr int64_t kT0 = 1794500000ll * kSecond;

ga::Fresh fresh(uint8_t seed) {
  ga::Fresh f{};
  for (size_t i = 0; i < f.size(); i++) f[i] = static_cast<uint8_t>(size_t{seed} * 31 + i * 7);
  return f;
}

ga::Store small_store() {
  ga::Store s;
  ga::store_init(s, ga::HashCost{8, 1});
  return s;
}

}  // namespace

TEST(sign_up_reserves_the_name_and_its_look_alikes) {
  ga::Store s = small_store();
  const ga::Answer a = ga::create(s, "Niclas", "correct horse", kT0, fresh(1));
  CHECK(a.refusal == ga::Refusal::none && a.id == 1);
  CHECK(a.code.size() == 19 && a.code[4] == '-');  // XXXX-XXXX-XXXX-XXXX
  CHECK(ga::create(s, "n1clas", "another pass", kT0, fresh(2)).refusal == ga::Refusal::name_taken);
  CHECK(ga::create(s, "Rocket", "short", kT0, fresh(3)).refusal == ga::Refusal::bad_secret);
  CHECK(ga::create(s, "two words", "long enough", kT0, fresh(3)).refusal == ga::Refusal::bad_name);
  CHECK(ga::find_by_name(s, "NICLAS") && ga::find_by_name(s, "NICLAS")->id == 1);
}

TEST(sign_in_takes_the_password_and_wrong_ones_hold_the_account_longer_each_time) {
  ga::Store s = small_store();
  ga::create(s, "Niclas", "correct horse", kT0, fresh(1));
  CHECK(ga::sign_in(s, "niclas", "correct horse", "a", kT0).refusal == ga::Refusal::none);
  int64_t t = kT0;
  int64_t last_wait = 0;
  for (int i = 0; i < 6; i++) {
    const ga::Answer bad = ga::sign_in(s, "niclas", "wrong one", "b" + std::to_string(i), t);
    CHECK(bad.refusal == ga::Refusal::wrong);
    const ga::Answer held = ga::sign_in(s, "niclas", "correct horse", "c", t);
    CHECK(held.refusal == ga::Refusal::held && held.wait_us > last_wait);
    last_wait = held.wait_us;
    t += held.wait_us;  // wait it out
  }
  CHECK(last_wait == 32 * kSecond);
  CHECK(ga::sign_in(s, "niclas", "correct horse", "c", t).refusal == ga::Refusal::none);
  // the hold is cleared by the right password: one wrong try holds a second again
  ga::sign_in(s, "niclas", "nope nope", "d", t);
  CHECK(ga::sign_in(s, "niclas", "correct horse", "e", t).wait_us == kSecond);
}

TEST(the_hold_tops_out_at_fifteen_minutes_and_an_unknown_name_holds_only_the_address) {
  ga::Store s = small_store();
  ga::create(s, "Niclas", "correct horse", kT0, fresh(1));
  int64_t t = kT0;
  for (int i = 0; i < 20; i++) {
    ga::sign_in(s, "niclas", "wrong one", "x" + std::to_string(i), t);
    t += ga::kHoldMax_us;
  }
  CHECK(ga::sign_in(s, "niclas", "x", "y", t - ga::kHoldMax_us + 1).wait_us <= ga::kHoldMax_us);
  ga::Store u = small_store();
  CHECK(ga::sign_in(u, "nobody", "whatever1", "z", kT0).refusal == ga::Refusal::unknown);
  CHECK(ga::sign_in(u, "nobody", "whatever1", "z", kT0).refusal == ga::Refusal::held);
  CHECK(ga::refusal_words(ga::Refusal::unknown, 0) == ga::refusal_words(ga::Refusal::wrong, 0));
}

TEST(a_classic_login_takes_a_code_once_and_never_the_password) {
  ga::Store s = small_store();
  const uint64_t id = ga::create(s, "Niclas", "correct horse", kT0, fresh(1)).id;
  CHECK(ga::login_game(s, "niclas", "correct horse", "a", kT0).refusal == ga::Refusal::wrong);
  const ga::Answer code = ga::make_code(s, id, kT0, fresh(5));
  CHECK(code.code.size() == 9 && code.code[4] == '-');
  std::string typed = code.code;  // a player types it in lower case without the dash
  typed.erase(4, 1);
  for (char& c : typed) c = static_cast<char>(c >= 'A' && c <= 'Z' ? c - 'A' + 'a' : c);
  const int64_t t = kT0 + 10 * kSecond;
  CHECK(ga::login_game(s, "Niclas", typed, "b", t).refusal == ga::Refusal::none);
  CHECK(ga::login_game(s, "Niclas", typed, "c", t + 2 * kSecond).refusal == ga::Refusal::wrong);
  const ga::Answer late = ga::make_code(s, id, t, fresh(6));
  CHECK(ga::login_game(s, "Niclas", late.code, "d", t + ga::kCodeLife_us + 4 * kSecond).refusal ==
        ga::Refusal::wrong);
  const int64_t later = t + 3600 * kSecond;
  for (int i = 0; i < 4; i++) ga::make_code(s, id, later, fresh(static_cast<uint8_t>(10 + i)));
  CHECK(ga::make_code(s, id, later, fresh(20)).refusal == ga::Refusal::too_many_codes);
  CHECK(ga::sign_in(s, "niclas", "correct horse", "e", later).refusal == ga::Refusal::none);
}

TEST(recovery_sets_a_new_password_and_a_new_code_and_removal_frees_the_name_later) {
  ga::Store s = small_store();
  const ga::Answer made = ga::create(s, "Niclas", "correct horse", kT0, fresh(1));
  const ga::Answer back = ga::recover(s, "niclas", made.code, "battery staple", "a", kT0, fresh(9));
  CHECK(back.refusal == ga::Refusal::none && back.code != made.code);
  CHECK(ga::sign_in(s, "niclas", "battery staple", "a", kT0).refusal == ga::Refusal::none);
  CHECK(ga::recover(s, "niclas", made.code, "third password", "b", kT0, fresh(9)).refusal ==
        ga::Refusal::wrong);
  CHECK(ga::set_blocked(s, made.id, true) == ga::Refusal::none);
  CHECK(ga::sign_in(s, "niclas", "battery staple", "a", kT0 + kSecond).refusal ==
        ga::Refusal::blocked);
  CHECK(ga::remove(s, made.id, kT0) == ga::Refusal::none);
  CHECK(!ga::find_by_name(s, "niclas"));
  CHECK(ga::create(s, "niclas", "new player", kT0 + kSecond, fresh(2)).refusal ==
        ga::Refusal::name_taken);
  const int64_t after = kT0 + (ga::kNameFreeAfter_s + 1) * kSecond;
  CHECK(ga::create(s, "niclas", "new player", after, fresh(2)).id == 2);
}

TEST(the_log_and_the_snapshot_read_back_into_the_same_store) {
  ga::Store s = small_store();
  const uint64_t a = ga::create(s, "Niclas", "correct horse", kT0, fresh(1)).id;
  const uint64_t b = ga::create(s, "Rocket", "rocket jump", kT0, fresh(2)).id;
  const ga::Answer mine = ga::make_code(s, a, kT0, fresh(3));
  const ga::Answer code = ga::make_code(s, b, kT0, fresh(4));
  ga::make_code(s, b, kT0, fresh(5));
  ga::login_game(s, "rocket", code.code, "x", kT0 + kSecond);
  ga::set_blocked(s, b, true);
  const int64_t now = kT0 + 2 * kSecond;
  for (const std::string& text : {s.log, ga::store_text(s, now)}) {
    ga::Store r = small_store();
    std::string error;
    CHECK_MSG(ga::store_read(r, text, error), error.c_str());
    CHECK(ga::store_text(r, now) == ga::store_text(s, now));
    CHECK(ga::login_game(r, "niclas", mine.code, "y", now).refusal == ga::Refusal::none);
    CHECK(ga::find(r, b)->codes.size() == 1 && ga::find(r, b)->blocked);
  }
  ga::Store bad = small_store();
  std::string error;
  CHECK(!ga::store_read(bad, "account 1 Niclas 5\nseen 2 7\n", error));
  CHECK(error.find("line 2") != std::string::npos && error.find("next:") != std::string::npos);
}
