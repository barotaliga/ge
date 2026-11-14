// The matchmaker's ops, defined (matchmaker_service.hpp).
//
// ge:summary The matchmaker's ops defined: the ticket checked, the queue asked, offers checked
// ge:keywords matchmaker-ops matchmaker_run queue_read offer_text
// ge:skill net
// ge:test tests/account_service_test.cpp
#include "account/matchmaker_service.hpp"

#include <algorithm>
#include <cstdlib>
#include <cstring>

#include "account/service.hpp"
#include "account/ticket.hpp"
#include "net/wire.hpp"

namespace ge::account {

namespace {

void put_text(std::vector<uint8_t>& out, std::string_view s) {
  const size_t n = std::min<size_t>(s.size(), 255);
  out.push_back(static_cast<uint8_t>(n));
  out.insert(out.end(), s.begin(), s.begin() + static_cast<std::ptrdiff_t>(n));
}

std::string get_text(net::Reader& r) {
  const uint8_t n = net::read_u8(r);
  const uint8_t* p = net::read_view(r, n);
  return p ? std::string(reinterpret_cast<const char*>(p), n) : std::string();
}

void put_u16(std::vector<uint8_t>& out, int64_t v) {
  const auto u = static_cast<uint16_t>(std::clamp<int64_t>(v, 0, 65535));
  out.push_back(static_cast<uint8_t>(u & 0xff));
  out.push_back(static_cast<uint8_t>(u >> 8));
}

Answer refuse(Refusal r, std::string words) {
  Answer a;
  a.refusal = r;
  a.words = std::move(words);
  return a;
}

std::string name_of(const Service& svc, uint64_t id) {
  const Account* a = find(svc.store, id);
  return a ? a->name : std::string();
}

void put_names(std::vector<uint8_t>& out, const Service& svc, const std::vector<uint64_t>& side) {
  out.push_back(static_cast<uint8_t>(side.size()));
  for (const uint64_t id : side) put_text(out, name_of(svc, id));
}

std::string joined(const std::vector<std::string>& names) {
  std::string s;
  for (size_t i = 0; i < names.size(); i++) s += (i ? " and " : "") + names[i];
  return s;
}

Answer queue_run(Service& svc, const std::vector<std::string>& f, int64_t now_s,
                 std::vector<uint8_t>& out) {
  if (f.size() != 5) return refuse(Refusal::unknown, refusal_words(Refusal::unknown, 0));
  Ticket t;
  const auto* tb = reinterpret_cast<const uint8_t*>(f[0].data());
  const Account* me = nullptr;
  if (ticket_read(tb, f[0].size(), svc.keys.pub, now_s, t) == TicketCheck::ok)
    me = find(svc.store, t.account);
  if (me == nullptr || me->removed_s != 0)
    return refuse(Refusal::wrong, "That sign-in has run out. Sign in again.");
  if (me->blocked) return refuse(Refusal::blocked, refusal_words(Refusal::blocked, 0));
  Matchmaker& mm = svc.matchmaker;
  Answer a;
  a.id = me->id;
  if (f[1] == "join") {
    const QueueAnswer q = matchmaker_join(mm, svc.matches.ladder, me->id, f[2], f[3], f[4], now_s);
    if (q != QueueAnswer::queued) return refuse(Refusal::unknown, queue_answer_words(q));
    matchmaker_pair(mm, now_s);
  } else if (f[1] == "leave") {
    matchmaker_leave(mm, me->id);
  } else if (f[1] != "look") {
    return refuse(Refusal::unknown, refusal_words(Refusal::unknown, 0));
  }
  const Look l = matchmaker_look(mm, me->id, now_s);
  out.push_back(static_cast<uint8_t>(l.state));
  put_u16(out, l.waited_s);
  put_u16(out, l.window / 10);
  if (l.state == LookState::matched && l.match != nullptr) {
    const MadeMatch& m = *l.match;
    const bool on_a = std::find(m.side_a.begin(), m.side_a.end(), me->id) != m.side_a.end();
    put_text(out, m.address);
    put_text(out, m.mode);
    put_names(out, svc, on_a ? m.side_a : m.side_b);
    put_names(out, svc, on_a ? m.side_b : m.side_a);
    std::vector<std::string> them;
    for (const uint64_t id : on_a ? m.side_b : m.side_a) them.push_back(name_of(svc, id));
    a.words = "Game found: " + m.mode + " against " + joined(them) + " on " + m.address + ".";
  } else {
    put_text(out, "");
    put_text(out, "");
    out.push_back(0);
    out.push_back(0);
    a.words = l.state == LookState::waiting
                  ? "Looking for a game (" + std::to_string(l.waited_s) + " s, within " +
                        std::to_string(l.window / 10) + " points)."
                  : "You are not looking for a game.";
  }
  return a;
}

Answer offer_run(Service& svc, const std::vector<std::string>& f, const net::Address& from,
                 int64_t now_s) {
  if (f.size() != 7 || f[0].size() != 32 || f[1].size() != 64)
    return refuse(Refusal::unknown, refusal_words(Refusal::unknown, 0));
  net::Key server{};
  std::memcpy(server.data(), f[0].data(), 32);
  net::Signature sig{};
  std::memcpy(sig.data(), f[1].data(), 64);
  const long port = std::strtol(f[4].c_str(), nullptr, 10);
  const long seats = std::strtol(f[5].c_str(), nullptr, 10);
  const long long time_s = std::strtoll(f[6].c_str(), nullptr, 10);
  const std::string text =
      offer_text(f[2], f[3], static_cast<uint16_t>(port), static_cast<int>(seats), time_s);
  if (port <= 0 || port > 65535 || seats < 0 || seats > 64 ||
      !net::sign_check(server, sig, reinterpret_cast<const uint8_t*>(text.data()), text.size()))
    return refuse(Refusal::wrong, "That offer is not signed by its server.");
  if (std::llabs(time_s - now_s) > kOfferSkew_s)
    return refuse(Refusal::wrong,
                  "That offer's time is not the master's. Check the server's clock.");
  if (!svc.matches.trusted.count(server))
    return refuse(Refusal::wrong, "This master does not count that server's games.");
  net::Address at = from;
  at.port = static_cast<uint16_t>(port);
  matchmaker_offer(svc.matchmaker, server, f[2], f[3], net::to_string(at), static_cast<int>(seats),
                   now_s);
  matchmaker_pair(svc.matchmaker, now_s);
  return Answer{};
}

}  // namespace

std::string offer_text(std::string_view game, std::string_view mode, uint16_t port, int seats,
                       int64_t time_s) {
  std::string s = "offer\n";
  s += game;
  s += "\n";
  s += mode;
  s += "\n" + std::to_string(port) + "\n" + std::to_string(seats) + "\n" + std::to_string(time_s);
  return s;
}

Answer matchmaker_run(Service& svc, Op op, const std::vector<std::string>& fields,
                      const net::Address& from, int64_t now_s, std::vector<uint8_t>& out) {
  out.clear();
  return op == Op::offer ? offer_run(svc, fields, from, now_s) : queue_run(svc, fields, now_s, out);
}

void service_offer(Service& svc, const net::Key& server, std::string_view game,
                   std::string_view mode, std::string_view address, int seats, int64_t now_s) {
  matchmaker_offer(svc.matchmaker, server, game, mode, address, seats, now_s);
  matchmaker_pair(svc.matchmaker, now_s);
}

bool queue_read(const std::vector<uint8_t>& bytes, QueueView& v) {
  v = QueueView{};
  net::Reader r = net::make_reader(bytes.data(), bytes.size());
  const uint8_t state = net::read_u8(r);
  v.waited_s = net::read_u16(r);
  v.window_points = net::read_u16(r);
  v.address = get_text(r);
  v.mode = get_text(r);
  for (std::vector<std::string>* side : {&v.mine, &v.theirs}) {
    const uint8_t n = net::read_u8(r);
    for (uint8_t i = 0; i < n && !r.bad; i++) side->push_back(get_text(r));
  }
  if (state > 2) return false;
  v.state = static_cast<LookState>(state);
  return !r.bad && r.at == bytes.size();
}

}  // namespace ge::account
