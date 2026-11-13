// Tests: logins on a game server (engine/server/login.hpp): a server that is its own master logs a
// player in by `login <name> <code>` and by the two-step form, as a command and as chat; a code
// works once; a wrong one is refused in a player's words; a seat that must log in may not chat and
// is warned, then dropped; a guest may not use a registered name, nor a logged-in player another;
// a server asking a master elsewhere learns its key, checks its ticket, and believes no ticket
// signed by another key.
//
// ge:summary Tests: server logins: own and remote master, the two-step line, timeouts, names
// ge:keywords test login sv_login two-step-login login-code reserved-names login-timeout
// ge:skill net testing
#include "server/login.hpp"

#include <string>
#include <vector>

#include "check.hpp"
#include "core/num.hpp"
#include "net/sim_network.hpp"

namespace ga = ge::account;
namespace gn = ge::net;
namespace gs = ge::server;

namespace {

constexpr int64_t kT0 = 1794500000ll * 1000000;
const gn::Address kMasterAt = gn::sim_address(1, ga::kAccountPort);
const gn::Address kServerAt = gn::sim_address(2, 40000);

void counter_fresh(void* ctx, uint8_t* out, size_t size) {
  auto* n = static_cast<uint64_t*>(ctx);
  for (size_t i = 0; i < size; i++) out[i] = static_cast<uint8_t>((*n * 131 + i * 17) >> 1);
  (*n)++;
}

gn::Key key_of(uint8_t b) {
  gn::Key k{};
  for (size_t i = 0; i < k.size(); i++) k[i] = static_cast<uint8_t>(size_t{b} * 7 + i);
  return k;
}

ga::Fresh fresh_of(uint8_t b) {
  ga::Fresh f{};
  for (size_t i = 0; i < f.size(); i++) f[i] = static_cast<uint8_t>(size_t{b} * 13 + i);
  return f;
}

struct World {
  gn::SimNetwork net = gn::make_sim_network(9, gn::LinkConditions{});
  gn::Transport master_port, link;
  ga::Service svc;
  gs::Logins logins;
  uint64_t fresh_n = 1;
  int64_t now = kT0;
  double game_s = 0;
  uint8_t next_fresh = 20;

  explicit World(int login = 1) {
    gn::open_sim_transport(master_port, net, kMasterAt);
    gn::open_sim_transport(link, net, kServerAt);
    ga::Store store;
    ga::store_init(store, ga::HashCost{8, 1});
    ga::create(store, "Niclas", "correct horse", now, fresh_of(1));
    ga::create(store, "Ann", "battery staple", now, fresh_of(2));
    ga::ServiceConfig c;
    c.seed = key_of(1);
    ga::service_init(svc, c, std::move(store), master_port, counter_fresh, &fresh_n);
    settings.login = login;
    settings.time_s = 60;
  }

  gs::LoginSettings settings;

  std::string code(uint64_t account) {
    return ga::make_code(svc.store, account, now, fresh_of(next_fresh++)).code;
  }

  // Run the frame a while (the master answering), the game's clock with it.
  void run(double seconds) {
    const int steps = static_cast<int>(ge::core::num::round(seconds * 20));
    for (int k = 0; k < steps; k++) {
      now += 50000;
      game_s += 0.05;
      gn::sim_advance(net, now);
      ga::service_update(svc, now);
      gn::sim_advance(net, now);
      gs::logins_frame(logins, game_s, now);
    }
  }

  // The lines told to `slot` since the last call, and everyone's.
  std::vector<std::string> told(int slot) {
    std::vector<std::string> out;
    for (const gs::LoginOut& o : logins.out)
      if ((o.kind == gs::LoginOut::Kind::tell && o.slot == slot) ||
          o.kind == gs::LoginOut::Kind::tell_all)
        out.push_back(o.text);
    return out;
  }
  bool has(gs::LoginOut::Kind kind, int slot, const std::string& text) const {
    for (const gs::LoginOut& o : logins.out)
      if (o.kind == kind && o.slot == slot && o.text == text) return true;
    return false;
  }
};

bool contains(const std::vector<std::string>& lines, const std::string& part) {
  for (const std::string& l : lines)
    if (l.find(part) != std::string::npos) return true;
  return false;
}

bool open_own(World& w) {
  std::string error;
  const bool ok = gs::logins_open(w.logins, w.settings, ga::NameRules{15, true, false}, &w.svc,
                                  nullptr, counter_fresh, &w.fresh_n, w.now, error);
  CHECK_MSG(ok, "logins_open (own master): " + error);
  return ok;
}

}  // namespace

TEST(login_own_master_by_one_line_and_by_two_steps) {
  World w;
  if (!open_own(w)) return;
  gs::login_seat(w.logins, 0, 11, false, "10.0.0.7:27001", 0);
  CHECK(contains(w.told(0), "login <name> <code>"));
  CHECK(contains(w.told(0), "within 60 seconds"));
  w.logins.out.clear();

  // As chat, quoted, the way a client sends a line it does not know.
  const std::string code = w.code(1);
  CHECK(gs::login_line(w.logins, 0, "say \"login niclas " + code + "\"", 1, w.now));
  CHECK(gs::login_account(w.logins, 0) == 1);
  CHECK(w.has(gs::LoginOut::Kind::auth, 0, "Niclas"));
  CHECK(w.has(gs::LoginOut::Kind::name, 0, "Niclas"));
  CHECK(contains(w.told(0), "Niclas logged in."));
  w.logins.out.clear();
  CHECK(gs::login_line(w.logins, 0, "login niclas " + code, 2, w.now));
  CHECK(contains(w.told(0), "already logged in"));

  // Two steps, as a command, on another seat: the code typed alone next.
  w.logins.out.clear();
  gs::login_seat(w.logins, 1, 12, false, "10.0.0.8:27001", 0);
  CHECK(gs::login_line(w.logins, 1, "login ann", 1, w.now));
  CHECK(contains(w.told(1), "Enter your login code for ann:"));
  CHECK(gs::login_line(w.logins, 1, "say " + w.code(2), 2, w.now));
  CHECK(gs::login_account(w.logins, 1) == 2);
}

TEST(login_code_works_once_and_wrong_ones_read_like_a_game) {
  World w;
  if (!open_own(w)) return;
  gs::login_seat(w.logins, 0, 11, false, "10.0.0.7:27001", 0);
  gs::login_seat(w.logins, 1, 12, false, "10.0.0.8:27001", 0);
  const std::string code = w.code(1);
  CHECK(gs::login_line(w.logins, 0, "login Niclas " + code, 1, w.now));
  w.logins.out.clear();
  CHECK(gs::login_line(w.logins, 1, "login Niclas " + code, 1, w.now));
  CHECK(gs::login_account(w.logins, 1) == 0);
  CHECK(contains(w.told(1), "don't match"));
  for (const std::string& line : w.told(1)) {
    CHECK(line.find("next:") == std::string::npos && line.find('`') == std::string::npos);
  }
}

TEST(login_a_seat_that_must_may_not_chat_and_is_dropped_in_time) {
  World w;
  if (!open_own(w)) return;
  gs::login_seat(w.logins, 0, 11, false, "10.0.0.7:27001", 0);
  gs::login_seat(w.logins, 1, 12, true, "10.0.0.8:27001", 0);  // a spectator watches as a guest
  w.logins.out.clear();
  CHECK(gs::login_line(w.logins, 0, "say hello", 1, w.now));
  CHECK(contains(w.told(0), "Log in before you chat."));
  CHECK(!gs::login_line(w.logins, 1, "say hello", 1, w.now));
  CHECK(!gs::login_line(w.logins, 0, "ready", 1, w.now));  // a lobby's word goes on
  w.logins.out.clear();
  w.game_s = 49.9;
  w.run(0.2);
  CHECK(contains(w.told(0), "10 seconds left"));
  w.logins.out.clear();
  w.game_s = 59.9;
  w.run(0.2);
  CHECK(w.has(gs::LoginOut::Kind::drop, 0, "didn't log in in time"));
  bool spectator_dropped = false;
  for (const gs::LoginOut& o : w.logins.out)
    spectator_dropped = spectator_dropped || (o.kind == gs::LoginOut::Kind::drop && o.slot == 1);
  CHECK(!spectator_dropped);
}

TEST(login_names_registered_and_logged_in_are_kept) {
  World w;
  if (!open_own(w)) return;
  std::string name = "N1clas";
  CHECK(gs::login_name(w.logins, 2, name, true) == gs::NameVerdict::reserved);
  gs::login_seat(w.logins, 2, 13, false, "10.0.0.9:27001", 0);
  CHECK(contains(w.told(2), "belongs to a registered player"));
  name = "guest";
  CHECK(gs::login_name(w.logins, 2, name, false) == gs::NameVerdict::free);

  gs::login_seat(w.logins, 0, 11, false, "10.0.0.7:27001", 0);
  CHECK(gs::login_line(w.logins, 0, "login niclas " + w.code(1), 1, w.now));
  name = "someone";
  CHECK(gs::login_name(w.logins, 0, name, false) == gs::NameVerdict::account);
  CHECK(name == "Niclas");

  World off;
  off.settings.reserved = 0;
  if (!open_own(off)) return;
  name = "niclas";
  CHECK(gs::login_name(off.logins, 2, name, true) == gs::NameVerdict::free);
}

TEST(login_remote_master_key_learned_ticket_checked) {
  World w;
  w.settings.master = gn::to_string(kMasterAt);
  std::string error;
  CHECK_MSG(gs::logins_open(w.logins, w.settings, ga::NameRules{}, nullptr, &w.link, counter_fresh,
                            &w.fresh_n, w.now, error),
            "logins_open (remote): " + error);
  w.run(0.5);
  CHECK(w.logins.key_known && w.logins.master_key == ga::service_key(w.svc));
  CHECK(!w.logins.notes.empty() && w.logins.notes[0].find("sv_accountkey") != std::string::npos);

  gs::login_seat(w.logins, 0, 11, false, "10.0.0.7:27001", w.game_s);
  w.logins.out.clear();
  CHECK(gs::login_line(w.logins, 0, "login niclas " + w.code(1), w.game_s, w.now));
  CHECK(w.logins.seats[0].state == gs::LoginState::asking);
  w.run(0.5);
  CHECK(gs::login_account(w.logins, 0) == 1);
  CHECK(w.has(gs::LoginOut::Kind::auth, 0, "Niclas"));

  // A server told another key believes no ticket the master signs.
  World other;
  other.settings.master = gn::to_string(kMasterAt);
  other.settings.key = gn::key_to_hex(key_of(5));
  CHECK(gs::logins_open(other.logins, other.settings, ga::NameRules{}, nullptr, &other.link,
                        counter_fresh, &other.fresh_n, other.now, error));
  gs::login_seat(other.logins, 0, 11, false, "10.0.0.7:27001", 0);
  CHECK(gs::login_line(other.logins, 0, "login niclas " + other.code(1), 0, other.now));
  other.run(3.5);  // sealed to the wrong key: the master never answers it
  CHECK(gs::login_account(other.logins, 0) == 0);
  CHECK(other.logins.stats.unanswered + other.logins.stats.bad_tickets == 1);
}

TEST(login_needs_a_service_when_logins_are_asked) {
  gs::Logins l;
  gs::LoginSettings set;
  set.login = 1;
  std::string error;
  CHECK(!gs::logins_open(l, set, ga::NameRules{}, nullptr, nullptr, counter_fresh, nullptr, 0,
                         error));
  CHECK(error.find("sv_accountmaster") != std::string::npos);
  set.master = "not an address at all:";
  error.clear();
  CHECK(!gs::logins_open(l, set, ga::NameRules{}, nullptr, nullptr, counter_fresh, nullptr, 0,
                         error));
}
