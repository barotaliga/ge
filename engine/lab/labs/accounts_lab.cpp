// LAB `accounts`: a master's account store through a busy month, and its service on the network
// (docs/accounts.md, M59a, M59b).
// Players sign up from a seeded list of names, and a share of them try a name another already
// holds, spelled alike (a digit for a letter, a dash, capitals, coloured text); every refusal is
// counted and a sample shown. Players sign in, some after a typing slip, and log in to game servers
// with one-time codes; a code typed a second time and a classic login with the account's password
// must both be refused. An attacker guesses one account's password from many addresses for a day:
// the throttle's holds say how many of its guesses were ever checked. The store's log and its
// snapshot are read back into a new store, which must be the same. A bench half times one
// Argon2id hash at the store's cost and at the master's default (measured, never in the digest).
// Then players ask the account service on the simulated network at three levels of loss: sign up,
// sign in, a code, a game server's login by it; every ticket is checked as a server would, and
// tickets made wrong on purpose (old, changed, another master's) must each be refused.
// Last, friends (M17j) over a lossy network with the tickets the players hold: each asks a few
// others, who answer by asking back when they see the request; each says where they play; one
// player blocks another (who must be told nothing different and see them offline); each invites a
// friend, and the invites must arrive once. The lists are read back in pages, never an answer
// bigger than its ask.
//
// ge:summary Lab: accounts signed up, look-alike names refused, logins, codes, a brute force held
// ge:keywords lab accounts account sign-up sign-in login-code one-time-code throttle brute-force
// ge:keywords look-alike reserved-name argon2id cost log snapshot service ticket sealed-ask loss
// ge:skill net labs
// ge:test tests/account_store_test.cpp
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <memory>
#include <string>
#include <vector>

#include "account/friends_service.hpp"
#include "account/names.hpp"
#include "account/service.hpp"
#include "account/store.hpp"
#include "account/store_text.hpp"
#include "account/ticket.hpp"
#include "core/prng.hpp"
#include "lab/chart.hpp"
#include "lab/lab.hpp"
#include "net/crypto.hpp"
#include "net/sim_network.hpp"

namespace {

namespace ga = ge::account;
namespace gc = ge::core;
namespace gl = ge::lab;
namespace gn = ge::net;

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

// THE SERVICE ON THE SIMULATED NETWORK (M59b). Each player, in turn: signs up, signs in (a
// ticket), makes a code, and a game server asks the service to log the player in by that code (a
// second ticket). Every ask is sealed, and lost asks and answers are sent again; the service must
// make each account once and spend each code once whatever the loss.
struct Seat {
  gn::Transport port;
  std::string name, password, code;
  int step = 0;  // 0 sign up, 1 sign in, 2 code, 3 the server's login, 4 done
  ga::Ask ask;
  int64_t asked_us = 0;
  bool failed = false;
};

void fresh_from(void* ctx, uint8_t* out, size_t size) {
  auto* rng = static_cast<gc::Rng*>(ctx);
  for (size_t i = 0; i < size; i++) out[i] = static_cast<uint8_t>(gc::int_in(*rng, 0, 255));
}

gn::Key draw_key(gc::Rng& rng) {
  gn::Key k{};
  for (uint8_t& b : k) b = static_cast<uint8_t>(gc::int_in(rng, 0, 255));
  return k;
}

void service_section(gl::Lab& lab, ga::HashCost cost) {
  const int players = static_cast<int>(lab.arg_num("service_players"));
  const std::vector<double> losses = lab.arg_nums("service_loss");
  const gn::Address service_at = gn::sim_address(1, ga::kAccountPort);
  const gn::Address server_at = gn::sim_address(2, 27500);
  std::vector<gl::Sample> times;
  gl::LabTable rows{"the account service on the simulated network (40 ms each way)",
                    {"loss", "asks", "sent", "answers resent", "accounts", "tickets good",
                     "codes spent twice", "slowest ask (ms)"},
                    {}};
  int bad_total = 0;
  for (const double loss : losses) {
    gc::Rng rng = gc::make_rng(static_cast<uint32_t>(lab.arg_num("seed")) * 1000 +
                               static_cast<uint32_t>(loss * 100));
    gn::LinkConditions c;
    c.latency_ms = 40;
    c.loss = loss;
    gn::SimNetwork net =
        gn::make_sim_network(static_cast<uint32_t>(gc::int_in(rng, 1, 1 << 30)), c);
    gn::Transport service_port, server_port;
    gn::open_sim_transport(service_port, net, service_at);
    gn::open_sim_transport(server_port, net, server_at);
    ga::Store store;
    ga::store_init(store, cost);
    ga::ServiceConfig config;
    config.seed = draw_key(rng);
    auto svc = std::make_unique<ga::Service>();
    ga::service_init(*svc, config, std::move(store), service_port, fresh_from, &rng);
    const gn::Key key = ga::service_key(*svc);
    std::vector<std::unique_ptr<Seat>> seats;
    for (int i = 0; i < players; i++) {
      auto seat = std::make_unique<Seat>();
      gn::open_sim_transport(seat->port, net, gn::sim_address(10 + static_cast<uint32_t>(i), 5000));
      seat->name = "player" + std::to_string(i);
      seat->password = "secret number " + std::to_string(gc::int_in(rng, 1000, 9999));
      seats.push_back(std::move(seat));
    }
    std::vector<double> ms;
    int asks = 0, tickets_good = 0, refused_wrongly = 0;
    uint64_t sent = 0;
    int64_t now = kStart;
    const auto begin = [&](Seat& s) {
      s.asked_us = now;
      asks++;
      switch (s.step) {
        case 0:
          ga::ask_start(s.ask, ga::Op::create, service_at, key, {s.name, s.password}, draw_key(rng),
                        now);
          break;
        case 1:
          ga::ask_start(s.ask, ga::Op::sign_in, service_at, key, {s.name, s.password},
                        draw_key(rng), now);
          break;
        case 2:
          ga::ask_start(s.ask, ga::Op::code, service_at, key, {s.name, s.password}, draw_key(rng),
                        now);
          break;
        default:
          ga::ask_start(s.ask, ga::Op::login, service_at, key,
                        {s.name, s.code, "10.0.0." + std::to_string(s.name.size()) + ":27001"},
                        draw_key(rng), now);
          break;
      }
      s.ask.max_tries = 12;
    };
    for (auto& s : seats) begin(*s);
    for (int tick = 0; tick < 4000; tick++) {
      bool all_done = true;
      for (auto& sp : seats) {
        Seat& s = *sp;
        if (s.step >= 4 || s.failed) continue;
        all_done = false;
        const uint64_t tries_before = static_cast<uint64_t>(s.ask.tries);
        ga::ask_update(s.ask, s.step == 3 ? server_port : s.port, now);
        sent += static_cast<uint64_t>(s.ask.tries) - tries_before;
      }
      if (all_done) break;
      now += 10000;
      gn::sim_advance(net, now);
      ga::service_update(*svc, now);
      gn::sim_advance(net, now);
      uint8_t buf[gn::kMaxDatagram];
      gn::Address from;
      int n = 0;
      for (auto& sp : seats) {
        while ((n = gn::transport_receive(sp->port, buf, sizeof buf, from, now)) >= 0) {
          ga::ask_read(sp->ask, buf, static_cast<size_t>(n), from);
        }
      }
      while ((n = gn::transport_receive(server_port, buf, sizeof buf, from, now)) >= 0) {
        for (auto& sp : seats) {
          if (sp->step == 3 && ga::ask_read(sp->ask, buf, static_cast<size_t>(n), from)) break;
        }
      }
      for (auto& sp : seats) {
        Seat& s = *sp;
        if (s.step >= 4 || s.failed || !s.ask.done) continue;
        if (!s.ask.answered || s.ask.reply.refusal != ga::Refusal::none) {
          s.failed = true;
          refused_wrongly++;
          continue;
        }
        ms.push_back(static_cast<double>(now - s.asked_us) / 1000.0);
        if (s.step == 1 || s.step == 3) {
          ga::Ticket t;
          if (ga::ticket_read(s.ask.reply.ticket.data(), s.ask.reply.ticket.size(), key,
                              now / kSecond, t) == ga::TicketCheck::ok &&
              t.name == s.name) {
            tickets_good++;
          }
        }
        if (s.step == 2) s.code = s.ask.reply.code;
        s.step++;
        if (s.step < 4) begin(s);
      }
    }
    // Every code spent once: the same login line again must be refused.
    int spent_twice = 0;
    for (int i = 0; i < players && i < 4; i++) {
      Seat& s = *seats[static_cast<size_t>(i)];
      if (s.code.empty()) continue;
      const ga::Answer again = ga::login_game(svc->store, s.name, s.code, "again", now);
      if (again.refusal == ga::Refusal::none) spent_twice++;
    }
    const size_t accounts = svc->store.accounts.size();
    double slowest = 0;
    for (const double v : ms) slowest = v > slowest ? v : slowest;
    char loss_text[16];
    std::snprintf(loss_text, sizeof loss_text, "%.0f%%", loss * 100);
    char slow_text[16];
    std::snprintf(slow_text, sizeof slow_text, "%.0f", slowest);
    rows.rows.push_back({loss_text, std::to_string(asks), std::to_string(sent),
                         std::to_string(svc->stats.resent), std::to_string(accounts),
                         std::to_string(tickets_good) + " of " + std::to_string(2 * players),
                         std::to_string(spent_twice), slow_text});
    const std::string tag = "loss" + std::to_string(static_cast<int>(loss * 100));
    lab.metric("service_" + tag + "_resent", static_cast<double>(svc->stats.resent), "count");
    lab.metric("service_" + tag + "_tickets_good", tickets_good, "count");
    lab.metric("service_" + tag + "_slowest_ms", slowest, "ms");
    times.push_back({loss_text + std::string(" loss"), ms});
    if (refused_wrongly || tickets_good != 2 * players ||
        accounts != static_cast<size_t>(players) || spent_twice) {
      bad_total++;
      lab.fail("at " + std::string(loss_text) + " loss: " + std::to_string(refused_wrongly) +
                   " asks unanswered or refused, " + std::to_string(tickets_good) + " of " +
                   std::to_string(2 * players) + " tickets good, " + std::to_string(accounts) +
                   " accounts for " + std::to_string(players) + " players, " +
                   std::to_string(spent_twice) + " codes spent twice",
               "account/service.cpp (the recent answers, the retries); skill: net.");
    }
  }
  lab.table(std::move(rows));
  lab.metric("service_levels_bad", bad_total, "count");

  // A ticket's refusals, each made on purpose.
  gc::Rng rng = gc::make_rng(static_cast<uint32_t>(lab.arg_num("seed")) + 77);
  const gn::SignKeys ours = gn::sign_keys(draw_key(rng));
  const gn::SignKeys theirs = gn::sign_keys(draw_key(rng));
  const int64_t now_s = kStart / kSecond;
  gl::LabTable refusals{"tickets a server is handed", {"ticket", "the server's check"}, {}};
  const auto check = [&](const std::string& what, const ga::Ticket& t, int64_t at_s, int flip) {
    uint8_t bytes[ga::kTicketMax];
    const size_t n = ga::ticket_write(t, bytes, sizeof bytes);
    if (flip >= 0) bytes[static_cast<size_t>(flip)] ^= 1;
    ga::Ticket out;
    const ga::TicketCheck r = ga::ticket_read(bytes, n, ours.pub, at_s, out);
    refusals.rows.push_back({what, ga::ticket_check_name(r)});
    return r;
  };
  const ga::Ticket good = ga::ticket_issue(ours, 7, "Ranger", now_s);
  int ticket_wrong = 0;
  ticket_wrong += check("signed by this master", good, now_s + 60, -1) != ga::TicketCheck::ok;
  ticket_wrong +=
      check("a day and a minute old", good, now_s + 86460, -1) != ga::TicketCheck::expired;
  ticket_wrong += check("its name changed by one bit", good, now_s,
                        static_cast<int>(ga::kTicketHead)) != ga::TicketCheck::bad_signature;
  ga::Ticket forged = ga::ticket_issue(theirs, 1, "Admin", now_s);
  ticket_wrong +=
      check("signed by another master", forged, now_s, -1) != ga::TicketCheck::other_master;
  forged.master = ours.pub;
  ticket_wrong += check("another key's, naming this master", forged, now_s, -1) !=
                  ga::TicketCheck::bad_signature;
  lab.table(std::move(refusals));
  lab.metric("tickets_judged_wrong", ticket_wrong, "count");
  if (ticket_wrong) {
    lab.fail(std::to_string(ticket_wrong) + " tickets were judged wrongly",
             "account/ticket.cpp; skill: net.");
  }

  gl::ChartFrame frame{"HOW LONG EACH ASK TO THE ACCOUNT SERVICE TOOK, BY LOSS",
                       "seed " + lab.arg("seed") + ", 40 ms each way, an ask sent again each 0.5 s",
                       "time to the answer (ms)", "loss"};
  const gl::Chart chart = gl::box_chart(frame, times);
  lab.shot(
      {"service",
       chart.title,
       "Each player signs up, signs in, makes a login code, and a game server logs the player "
       "in by it: four sealed asks, each answered by the master's account service on the "
       "simulated network at 40 ms each way. A lost ask or answer is sent again every half "
       "second; the service answers an ask it has seen with the answer it gave.",
       "with no loss every ask about 80 ms (one round trip); with loss a tail in steps of half a "
       "second (one resend each), every ask answered, never a second account or a code "
       "spent twice",
       {{"seed", lab.arg("seed")}, {"service_loss", lab.arg("service_loss")}}},
      gl::to_picture(chart.plot));
}

// FRIENDS ON THE NETWORK (M17j): the friends' ops asked with the players' tickets, at a loss.
void friends_section(gl::Lab& lab, ga::HashCost cost) {
  const int players = static_cast<int>(lab.arg_num("friends_players"));
  const int asks_each = static_cast<int>(lab.arg_num("friends_asks"));
  gc::Rng rng = gc::make_rng(static_cast<uint32_t>(lab.arg_num("seed")) * 7919 + 17);
  gn::LinkConditions c;
  c.latency_ms = 40;
  c.loss = 0.1;
  gn::SimNetwork net = gn::make_sim_network(static_cast<uint32_t>(gc::int_in(rng, 1, 1 << 30)), c);
  const gn::Address service_at = gn::sim_address(1, ga::kAccountPort);
  gn::Transport service_port, client;
  gn::open_sim_transport(service_port, net, service_at);
  gn::open_sim_transport(client, net, gn::sim_address(9, 5000));
  ga::Store store;
  ga::store_init(store, cost);
  ga::ServiceConfig config;
  config.seed = draw_key(rng);
  auto svc = std::make_unique<ga::Service>();
  ga::service_init(*svc, config, std::move(store), service_port, fresh_from, &rng);
  int64_t now = kStart;
  size_t biggest = 0;
  int unanswered = 0;
  const auto ask = [&](ga::Op op, const std::vector<std::string>& fields) {
    ga::Ask a;
    ga::ask_start(a, op, service_at, ga::service_key(*svc), fields, draw_key(rng), now);
    a.max_tries = 12;
    for (int i = 0; i < 400 && !a.done; i++) {
      ga::ask_update(a, client, now);
      now += 20000;
      gn::sim_advance(net, now);
      ga::service_update(*svc, now);
      gn::sim_advance(net, now);
      uint8_t buf[gn::kMaxDatagram];
      gn::Address from;
      int n = 0;
      while ((n = gn::transport_receive(client, buf, sizeof buf, from, now)) >= 0) {
        biggest = std::max(biggest, static_cast<size_t>(n));
        ga::ask_read(a, buf, static_cast<size_t>(n), from);
      }
    }
    if (!a.answered) unanswered++;
    return a.reply;
  };
  // The players and their tickets (signed by the service's key, as a sign-in makes them).
  std::vector<std::string> names, tickets;
  for (int i = 0; i < players; i++) {
    names.push_back("friend" + std::to_string(i));
    const ga::Answer made = ga::create(svc->store, names.back(), "a long password", now, draw(rng));
    const ga::Ticket t = ga::ticket_issue(svc->keys, made.id, names.back(), now / kSecond);
    uint8_t b[ga::kTicketMax];
    const size_t n = ga::ticket_write(t, b, sizeof b);
    tickets.emplace_back(reinterpret_cast<const char*>(b), n);
  }
  const auto list = [&](int who) {
    std::vector<ga::FriendRow> all;
    bool more = true;
    for (int page = 0; more && page < 16; page++) {
      std::vector<ga::FriendRow> rows;
      if (!ga::friends_page_read(ask(ga::Op::friends, {tickets[who], std::to_string(page)}).ticket,
                                 rows, more))
        break;
      all.insert(all.end(), rows.begin(), rows.end());
    }
    return all;
  };
  // Asks out; then each answers what waits on them.
  int asked = 0;
  for (int i = 0; i < players; i++)
    for (int k = 0; k < asks_each; k++) {
      const int j = (i + 1 + gc::int_in(rng, 0, players - 2)) % players;
      if (ask(ga::Op::friend_, {tickets[i], "ask", names[j]}).code == "asked") asked++;
    }
  int answered_back = 0;
  for (int i = 0; i < players; i++)
    for (const ga::FriendRow& r : list(i))
      if (r.kind == ga::FriendRowKind::asks_you &&
          ask(ga::Op::friend_, {tickets[i], "ask", r.name}).code == "made")
        answered_back++;
  // Presence: each says where they are.
  const char* states[] = {"online", "lobby", "playing"};
  std::vector<std::string> said(players);
  for (int i = 0; i < players; i++) {
    said[i] = states[gc::int_in(rng, 0, 2)];
    ask(ga::Op::presence,
        {tickets[i], said[i], "shooters_edge", "203.0.113." + std::to_string(i + 1) + ":27500",
         i % 2 ? "dm4 duel" : "e1m2 4on4"});
  }
  int seen_right = 0, seen_rows = 0;
  std::vector<std::vector<ga::FriendRow>> lists(players);
  for (int i = 0; i < players; i++) {
    lists[i] = list(i);
    for (const ga::FriendRow& r : lists[i]) {
      if (r.kind != ga::FriendRowKind::friend_) continue;
      seen_rows++;
      const int j = std::stoi(r.name.substr(6));
      seen_right += r.presence.state != ga::PresenceState::offline &&
                            said[j] == ga::presence_state_name(r.presence.state)
                        ? 1
                        : 0;
    }
  }
  // A block: player 0 blocks its first friend, who asks again and looks.
  int block_silent = 0;
  const std::vector<uint64_t> f0 = ga::friends_of(svc->friends, 1);
  if (!f0.empty()) {
    const int b = static_cast<int>(f0[0]) - 1;
    ask(ga::Op::friend_, {tickets[0], "block", names[b]});
    const ga::Reply again = ask(ga::Op::friend_, {tickets[b], "ask", names[0]});
    block_silent += again.code == "asked" ? 1 : 0;
    bool sees = false;
    for (const ga::FriendRow& r : list(b)) sees |= r.name == names[0];
    block_silent += sees ? 0 : 1;
  }
  // Invites: each invites one friend; each reads theirs.
  int sent = 0, got = 0;
  for (int i = 0; i < players; i++) {
    const std::vector<uint64_t> fr = ga::friends_of(svc->friends, static_cast<uint64_t>(i) + 1);
    if (fr.empty()) continue;
    const ga::Reply r =
        ask(ga::Op::invite, {tickets[i], names[fr[0] - 1], "shooters_edge",
                             "203.0.113." + std::to_string(i + 1) + ":27500", "join me?"});
    sent += r.refusal == ga::Refusal::none ? 1 : 0;
  }
  for (int i = 0; i < players; i++) {
    std::vector<ga::InviteRow> inv;
    if (ga::invites_read(ask(ga::Op::invites, {tickets[i]}).ticket, inv))
      got += static_cast<int>(inv.size());
  }
  // One player's list, as the game CLI prints it.
  gl::LabTable shown{"friend0's friends list read back over the network",
                     {"name", "kind", "state", "where", "words"},
                     {}};
  for (const ga::FriendRow& r : list(0))
    shown.rows.push_back({r.name,
                          r.kind == ga::FriendRowKind::friend_    ? "friend"
                          : r.kind == ga::FriendRowKind::asks_you ? "asks you"
                                                                  : "you asked",
                          ga::presence_state_name(r.presence.state), r.presence.where,
                          r.presence.words});
  lab.table(std::move(shown));
  lab.metric("friends_asked", asked, "count");
  lab.metric("friends_made", static_cast<double>(svc->friends.pairs.size()), "pairs");
  lab.metric("friends_answered_back", answered_back, "count");
  lab.metric("presence_rows", seen_rows, "count");
  lab.metric("presence_seen_right", seen_right, "count");
  lab.metric("block_silent", block_silent, "of 2");
  lab.metric("invites_sent", sent, "count");
  lab.metric("invites_got", got, "count");
  lab.metric("friends_biggest_answer", static_cast<double>(biggest), "bytes");
  lab.metric("friends_unanswered", unanswered, "asks");
  if (unanswered != 0 || seen_right != seen_rows || block_silent != 2 || got != sent ||
      biggest > ga::kAccountAskSize)
    lab.fail("friends over the network went wrong: " + std::to_string(unanswered) +
                 " asks unanswered, " + std::to_string(seen_right) + " of " +
                 std::to_string(seen_rows) + " presences right, block silent " +
                 std::to_string(block_silent) + " of 2, invites " + std::to_string(got) + " of " +
                 std::to_string(sent),
             "Run tests/account_friends_test and tests/account_service_test; skill: net.");
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
  const int64_t end = now + int64_t{hours} * 3600 * kSecond;
  int next_mark = 0;
  while (t < end) {
    const ga::Answer a = ga::sign_in(
        store, victim.name, "guess" + std::to_string(sent_n),
        "203.0.113." + std::to_string(sent_n % 250) + ":" + std::to_string(sent_n / 250), t);
    sent_n++;
    if (a.refusal != ga::Refusal::held) checked_n++;
    if (t >= now + int64_t{next_mark} * 600 * kSecond) {
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

  // 6. The service over the network: players ask a master's account service on the simulated
  // network, at three levels of loss, and a game server logs each in by the code it made.
  service_section(lab, cost);
  friends_section(lab, cost);

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
    "engine/account (store, names, store_text, ticket, service, friends), engine/net/crypto.hpp",
    "net",
    "Does a master's account store keep names to one player however they are spelled, let players "
    "in with the right secret and the right kind of secret only (a code once), hold back a brute "
    "force, read its own log back, and answer sealed asks over a lossy network, each once?",
    {
        {"players", "400", "sign-ups"},
        {"copycats", "0.25", "the share of sign-ups followed by a look-alike of a held name"},
        {"attack_hours", "24", "how long the attacker guesses"},
        {"cost_kib", "64", "the store's hash memory, KiB (the lab's: a real master's is 65536)"},
        {"cost_passes", "1", "the store's hash passes"},
        {"bench_kib", "19456,65536", "hash memories timed, KiB (measured)"},
        {"service_players", "40", "players asking the account service on the network"},
        {"service_loss", "0,0.1,0.3", "the network's loss at each level of the service's run"},
        {"friends_players", "24", "players making friends on the network (at 10% loss)"},
        {"friends_asks", "3", "friend requests each sends"},
        {"seed", "1", "the names', passwords' and fresh bytes' stream"},
    },
    "--players=60 --attack_hours=4 --bench_kib=1024 --service_players=12 --friends_players=10",
    run,
};
GE_LAB(kSpec);

}  // namespace
