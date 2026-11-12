// LAB `accounts`: a master's account store through a busy month (docs/accounts.md, M59a).
// Players sign up from a seeded list of names, and a share of them try a name another already
// holds, spelled alike (a digit for a letter, a dash, capitals, coloured text); every refusal is
// counted and a sample shown. Players sign in, some after a typing slip, and log in to game servers
// with one-time codes; a code typed a second time and a classic login with the account's password
// must both be refused. An attacker guesses one account's password from many addresses for a day:
// the throttle's holds say how many of its guesses were ever checked. The store's log and its
// snapshot are read back into a new store, which must be the same. A bench half times one
// Argon2id hash at the store's cost and at the master's default (measured, never in the digest).
//
// ge:summary Lab: accounts signed up, look-alike names refused, logins, codes, a brute force held
// ge:keywords lab accounts account sign-up sign-in login-code one-time-code throttle brute-force
// ge:keywords look-alike reserved-name argon2id cost log snapshot
// ge:skill net labs
// ge:test tests/account_store_test.cpp
#include <chrono>
#include <cstdio>
#include <string>
#include <vector>

#include "account/names.hpp"
#include "account/store.hpp"
#include "account/store_text.hpp"
#include "core/prng.hpp"
#include "lab/chart.hpp"
#include "lab/lab.hpp"
#include "net/crypto.hpp"

namespace {

namespace ga = ge::account;
namespace gc = ge::core;
namespace gl = ge::lab;

constexpr int64_t kSecond = 1000000;
constexpr int64_t kStart = 1794500000ll * kSecond;  // a fixed evening: the lab reads no clock

ga::Fresh draw(gc::Rng& rng) {
  ga::Fresh f{};
  for (uint8_t& b : f) b = static_cast<uint8_t>(gc::int_in(rng, 0, 255));
  return f;
}

std::string made_name(gc::Rng& rng) {
  static const char* kParts[] = {"ra",  "zo",  "kel", "mir", "tor", "va",  "quin", "dex", "lo",
                                 "sha", "gru", "fen", "bo",  "nyx", "tal", "or",   "ix",  "ul"};
  std::string n;
  const int parts = gc::int_in(rng, 2, 3);
  for (int i = 0; i < parts; i++) n += kParts[gc::pick_index(rng, 18)];
  if (gc::chance(rng, 0.3)) n += std::to_string(gc::int_in(rng, 1, 99));
  if (gc::chance(rng, 0.5)) n[0] = static_cast<char>(n[0] - 'a' + 'A');
  return n.size() > ga::kNameMax ? n.substr(0, ga::kNameMax) : n;
}

// Another spelling a player could pass off as `name`, and how it was made.
std::string look_alike(gc::Rng& rng, const std::string& name, std::string& how) {
  std::string n = name;
  switch (gc::int_in(rng, 0, 3)) {
    case 0:
      for (char& c : n) c = static_cast<char>(c >= 'a' && c <= 'z' ? c - 'a' + 'A' : c);
      how = "capitals";
      break;
    case 1:
      for (char& c : n) {
        if (c == 'o')
          c = '0';
        else if (c == 'l' || c == 'i')
          c = '1';
        else if (c == 'e')
          c = '3';
        else if (c == 'a')
          c = '4';
      }
      how = "digits for letters";
      break;
    case 2:
      n.insert(n.size() / 2, n.size() < ga::kNameMax ? "_" : "");
      if (n == name && !n.empty()) n[0] = static_cast<char>(n[0] ^ 0x20);
      how = "a separator";
      break;
    default:
      for (char& c : n) c = static_cast<char>(static_cast<uint8_t>(c) | 0x80);
      how = "coloured text";
      break;
  }
  return n;
}

// A name as a player sees it: coloured text shown as its plain letters in brackets.
std::string shown(const std::string& n) {
  std::string out;
  bool coloured = false;
  for (const char c : n) coloured |= (static_cast<uint8_t>(c) & 0x80) != 0;
  for (const char c : n) out.push_back(static_cast<char>(static_cast<uint8_t>(c) & 0x7F));
  return coloured ? "[red] " + out : out;
}

void run(gl::Lab& lab) {
  const int players = static_cast<int>(lab.arg_num("players"));
  const double copycats = lab.arg_num("copycats");
  const int hours = static_cast<int>(lab.arg_num("attack_hours"));
  const ga::HashCost cost{static_cast<uint32_t>(lab.arg_num("cost_kib")),
                          static_cast<uint32_t>(lab.arg_num("cost_passes"))};
  gc::Rng rng = gc::make_rng(static_cast<uint32_t>(lab.arg_num("seed")));

  ga::Store store;
  ga::store_init(store, cost);
  int64_t now = kStart;

  // 1. Sign-ups, and the copycats.
  struct Player {
    std::string name, password;
    uint64_t id = 0;
  };
  std::vector<Player> made;
  int taken_plain = 0, refused_alike = 0, let_alike = 0, tries_alike = 0;
  gl::LabTable alike{
      "names tried against one already held", {"held", "tried", "made by", "answer"}, {}};
  for (int i = 0; i < players; i++) {
    now += 7 * kSecond;
    Player p;
    p.name = made_name(rng);
    p.password = "pw-" + std::to_string(i) + "-horse";
    const ga::Answer a = ga::create(store, p.name, p.password, now, draw(rng));
    if (a.refusal == ga::Refusal::name_taken) {
      taken_plain++;
      continue;
    }
    if (a.refusal != ga::Refusal::none) {
      lab.fail("sign-up of " + p.name + " refused: " + a.words,
               "made_name keeps to account/names.hpp's characters; skill: net.");
      continue;
    }
    p.id = a.id;
    made.push_back(p);
    if (!made.empty() && gc::chance(rng, copycats)) {
      const Player& held = made[gc::pick_index(rng, made.size())];
      std::string how;
      const std::string copy = look_alike(rng, held.name, how);
      tries_alike++;
      // coloured text cannot be signed up with; it is a guest's name on a game's server
      const bool guest = how == "coloured text";
      bool refused;
      if (guest) {
        refused = ga::name_key(ga::NameRules{31, true, false}, copy) ==
                  ga::name_key(ga::kEveryGame, held.name);
      } else {
        refused = ga::create(store, copy, "copycat-pass", now, draw(rng)).refusal ==
                  ga::Refusal::name_taken;
      }
      (refused ? refused_alike : let_alike)++;
      if (alike.rows.size() < 12) {
        alike.rows.push_back(
            {held.name, shown(copy), how,
             refused ? (guest ? "renamed on the server" : "name taken") : "LET THROUGH"});
      }
    }
  }
  lab.metric("accounts", static_cast<double>(made.size()), "count");
  lab.metric("names_taken_already", taken_plain, "count");
  lab.metric("look_alikes_tried", tries_alike, "count");
  lab.metric("look_alikes_refused", refused_alike, "count");
  lab.table(std::move(alike));
  if (let_alike) {
    lab.fail(std::to_string(let_alike) + " look-alike names were let through",
             "name_key folds case, look-alikes, separators and coloured text "
             "(account/names.hpp); skill: net.");
  }

  // 2. An evening of logins: passwords, codes, game passwords, slips.
  int signed_in = 0, coded = 0, reused = 0, slips = 0, slip_refused = 0, password_classic = 0;
  for (size_t i = 0; i < made.size(); i++) {
    Player& p = made[i];
    now += 3 * kSecond;
    const std::string from = "10.1." + std::to_string(i / 250) + "." + std::to_string(i % 250);
    if (gc::chance(rng, 0.1)) {  // a slip first
      slips++;
      if (ga::sign_in(store, p.name, p.password + "x", from, now).refusal == ga::Refusal::wrong) {
        slip_refused++;
      }
      now += 2 * kSecond;  // past the one-second hold
    }
    if (ga::sign_in(store, p.name, p.password, from, now).refusal == ga::Refusal::none) {
      signed_in++;
    }
    if (i % 3 == 0) {
      const ga::Answer code = ga::make_code(store, p.id, now, draw(rng));
      now += 30 * kSecond;  // typed into the game's console half a minute later
      if (ga::login_game(store, p.name, code.code, from, now).refusal == ga::Refusal::none) coded++;
    } else if (i % 3 == 1) {  // a code typed a second time: spent
      const ga::Answer code = ga::make_code(store, p.id, now, draw(rng));
      ga::login_game(store, p.name, code.code, from, now + 20 * kSecond);
      if (ga::login_game(store, p.name, code.code, from, now + 40 * kSecond).refusal ==
          ga::Refusal::none) {
        reused++;  // must never happen
      }
      now += 41 * kSecond;
    } else if (ga::login_game(store, p.name, p.password, from, now + 5 * kSecond).refusal ==
               ga::Refusal::none) {
      password_classic++;  // must never happen
    }
  }
  lab.metric("signed_in", signed_in, "count");
  lab.metric("slips_refused", slip_refused, "count");
  lab.metric("logged_in_by_code", coded, "count");
  lab.metric("a_code_used_twice", reused, "count");
  lab.metric("password_taken_by_a_classic_login", password_classic, "count");
  if (signed_in != static_cast<int>(made.size()) || slip_refused != slips) {
    lab.fail("of " + std::to_string(made.size()) + " players " + std::to_string(signed_in) +
                 " signed in, and " + std::to_string(slip_refused) + " of " +
                 std::to_string(slips) + " slips were refused",
             "sign_in in account/store.cpp; skill: net.");
  }
  if (reused) {
    lab.fail("a login code logged a player in twice",
             "login_game spends a code as it takes it (account/store.cpp); skill: net.");
  }
  if (password_classic) {
    lab.fail("a classic login took an account's password",
             "login_game takes only a login code (account/store.hpp); skill: net.");
  }

  // 3. A brute force on one account from fresh addresses, one guess a second for a day.
  const Player& victim = made.front();
  gl::Trace checked{"guesses checked", {}, {}}, sent{"guesses sent", {}, {}};
  int64_t t = now, checked_n = 0, sent_n = 0;
  const int64_t end = now + hours * 3600 * kSecond;
  int next_mark = 0;
  while (t < end) {
    const ga::Answer a = ga::sign_in(
        store, victim.name, "guess" + std::to_string(sent_n),
        "203.0.113." + std::to_string(sent_n % 250) + ":" + std::to_string(sent_n / 250), t);
    sent_n++;
    if (a.refusal != ga::Refusal::held) checked_n++;
    if (t >= now + next_mark * 600 * kSecond) {
      const double hrs = static_cast<double>(t - now) / (3600.0 * kSecond);
      checked.x.push_back(hrs);
      checked.y.push_back(static_cast<double>(checked_n));
      sent.x.push_back(hrs);
      sent.y.push_back(static_cast<double>(sent_n));
      next_mark++;
    }
    t += kSecond;
  }
  lab.metric("attack_guesses_sent", static_cast<double>(sent_n), "count");
  lab.metric("attack_guesses_checked", static_cast<double>(checked_n), "count");
  const double cap = static_cast<double>(hours) * 4 + 12;  // a hold of 15 min: 4 an hour, + ramp
  if (static_cast<double>(checked_n) > cap) {
    lab.fail(std::to_string(checked_n) + " of an attacker's guesses were checked in " +
                 std::to_string(hours) + " h, more than about " + std::to_string(int(cap)),
             "the account's hold doubles to 15 minutes (account/store.cpp, fail); skill: net.");
  }
  now = end + ga::kHoldMax_us;
  if (ga::sign_in(store, victim.name, victim.password, "10.9.9.9", now).refusal !=
      ga::Refusal::none) {
    lab.fail("the attacked account's owner could not sign in once the hold ran out",
             "a right password clears the account's hold (account/store.cpp); skill: net.");
  }

  // 4. The log and the snapshot, read back.
  const std::string snapshot = ga::store_text(store, now);
  int same = 0;
  for (const std::string* text : {static_cast<const std::string*>(&store.log), &snapshot}) {
    ga::Store back;
    ga::store_init(back, cost);
    std::string error;
    if (!ga::store_read(back, *text, error)) {
      lab.fail("the store did not read back: " + error, "account/store_text.cpp; skill: net.");
    } else if (ga::store_text(back, now) == snapshot) {
      same++;
    }
  }
  lab.metric("log_bytes", static_cast<double>(store.log.size()), "bytes");
  lab.metric("snapshot_bytes", static_cast<double>(snapshot.size()), "bytes");
  lab.metric("read_back_same", same, "of 2");
  if (same != 2) {
    lab.fail("the log or the snapshot read back into a different store",
             "store_read must apply every line store_text writes; skill: net.");
  }

  // 5. What a hash costs (measured).
  std::vector<uint8_t> work;
  const uint8_t salt[ge::net::kSaltBytes] = {};
  uint8_t out[32];
  gl::LabTable costs{"one Argon2id hash", {"memory", "passes", "ms"}, {}};
  const std::vector<double> kibs = lab.arg_nums("bench_kib");
  for (const double kib_d : kibs) {
    const auto kib = static_cast<uint32_t>(kib_d);
    work.resize(size_t{kib} * 1024);
    const auto t0 = std::chrono::steady_clock::now();
    ge::net::password_hash(out, "correct horse battery", salt, kib, 3, work.data());
    const double ms =
        std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
    char ms_text[32];
    std::snprintf(ms_text, sizeof ms_text, "%.1f", ms);
    costs.rows.push_back({std::to_string(kib / 1024) + " MiB", "3", ms_text});
    lab.metric("hash_ms_" + std::to_string(kib / 1024) + "mib", ms, "ms", gl::Det::measured);
  }
  lab.table(std::move(costs), gl::Det::measured);

  gl::LabTable totals{"the month in numbers", {"what", "count", "expected"}, {}};
  totals.rows.push_back({"accounts", std::to_string(made.size()), "every new name"});
  totals.rows.push_back({"look-alike names refused", std::to_string(refused_alike),
                         std::to_string(tries_alike) + " (all)"});
  totals.rows.push_back({"signed in", std::to_string(signed_in), "every account"});
  totals.rows.push_back({"logged in to a server by code", std::to_string(coded), "every third"});
  totals.rows.push_back({"a code typed again that let a player in", std::to_string(reused), "0"});
  totals.rows.push_back(
      {"classic logins that took the account's password", std::to_string(password_classic), "0"});
  totals.rows.push_back({"attacker's guesses sent / checked",
                         std::to_string(sent_n) + " / " + std::to_string(checked_n),
                         "about 4 an hour checked"});
  lab.table(std::move(totals));

  gl::LineChart line;
  line.frame = {"AN ATTACKER GUESSING ONE PASSWORD, ONE GUESS A SECOND FROM NEW ADDRESSES",
                "seed " + lab.arg("seed") + ", " + std::to_string(hours) + " h", "t (h)",
                "guesses (log)"};
  line.log_y = true;
  line.spikes = false;
  line.traces = {sent, checked};
  const gl::Chart chart = gl::line_chart(line);
  lab.shot({"brute-force",
            chart.title,
            "One guess a second at one account's password, each from an address it has not used "
            "before, so only the account's own hold slows it. Sent: every guess. Checked: those "
            "the store hashed and compared; the rest were refused unread while the account was "
            "held.",
            "the sent line rising 3600 an hour; the checked line flat after its first minutes, "
            "about 4 an hour (a 15-minute hold), so a day gives about a hundred guesses",
            {{"seed", lab.arg("seed")}, {"attack_hours", lab.arg("attack_hours")}}},
           gl::to_picture(chart.plot));

  gl::Trace hold{"hold after the nth wrong password", {}, {}};
  for (int n = 1; n <= 14; n++) {
    ga::Store one;
    ga::store_init(one, ga::HashCost{8, 1});
    ga::create(one, "someone", "the password", kStart, ga::Fresh{});
    int64_t tt = kStart;
    int64_t wait = 0;
    for (int k = 0; k < n; k++) {
      ga::sign_in(one, "someone", "wrong guess", "a" + std::to_string(k), tt);
      wait = ga::sign_in(one, "someone", "x", "b" + std::to_string(k), tt).wait_us;
      tt += wait;
    }
    hold.x.push_back(n);
    hold.y.push_back(static_cast<double>(wait) / kSecond);
  }
  gl::LineChart hl;
  hl.frame = {"THE HOLD AFTER EACH WRONG PASSWORD IN A ROW", "account/store.cpp", "wrong in a row",
              "hold (s, log)"};
  hl.log_y = true;
  hl.points = true;
  hl.spikes = false;
  hl.rules = {900};
  hl.traces = {hold};
  const gl::Chart hc = gl::line_chart(hl);
  lab.shot({"hold",
            hc.title,
            "How long an account is held after n wrong passwords in a row, each waited out. A "
            "right password clears it. The dashed line is the cap, 15 minutes.",
            "1 s, 2 s, 4 s ... doubling to 512 s at the tenth, then flat on the cap; never a "
            "block for good, so nobody can lock another player out",
            {}},
           gl::to_picture(hc.plot));
}

const gl::LabSpec kSpec{
    "accounts",
    gl::LabKind::sim,
    "engine",
    "engine/account (store, names, store_text), engine/net/crypto.hpp (password_hash)",
    "net",
    "Does a master's account store keep names to one player however they are spelled, let players "
    "in with the right secret and the right kind of secret only (a code once), hold back a brute "
    "force, and read its own log back?",
    {
        {"players", "400", "sign-ups"},
        {"copycats", "0.25", "the share of sign-ups followed by a look-alike of a held name"},
        {"attack_hours", "24", "how long the attacker guesses"},
        {"cost_kib", "64", "the store's hash memory, KiB (the lab's: a real master's is 65536)"},
        {"cost_passes", "1", "the store's hash passes"},
        {"bench_kib", "19456,65536", "hash memories timed, KiB (measured)"},
        {"seed", "1", "the names', passwords' and fresh bytes' stream"},
    },
    "--players=60 --attack_hours=4 --bench_kib=1024",
    run,
};
GE_LAB(kSpec);

}  // namespace
