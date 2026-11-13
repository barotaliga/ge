// LAB `se-login`: LOGGING IN ON SHOOTER'S EDGE'S SERVER FROM A STOCK CLASSIC CLIENT
// (docs/accounts.md § Logging in). Two dedicated servers on the simulated network: one that is its
// own account master (`sv_login 1`, its accounts in process) and one that asks a community's master
// elsewhere (`sv_login 2`, `sv_accountmaster` and `sv_accountkey` set). Classic clients join and
// type what a player types in the console: `login <name> <code>` sent as chat (the way a client
// sends a line it does not know), the two-step `login <name>` then the code alone, a wrong code, a
// rename while logged in, chat before logging in, a code read off the wire and typed again after
// its owner used it, and a guest under a registered name who never logs in. It answers whether each
// is logged in, renamed, held or dropped as the design says, whether a login line ever reaches
// another player, and whether every line a player reads is a game's words; each player's console is
// drawn as the player sees it.
//
// ge:summary Lab: classic clients logging in on the dedicated server, own master and remote
// ge:keywords lab se-login login sv_login sv_accountmaster login-code two-step-login auth-key
// ge:keywords reserved-names login-timeout console player-text
// ge:skill net labs
// ge:test tests/shooters_edge_classic_login_test.cpp
#include <cstdio>
#include <string>
#include <utility>
#include <vector>

#include "account/service.hpp"
#include "console/console.hpp"
#include "console/view.hpp"
#include "core/num.hpp"
#include "lab/lab.hpp"
#include "net/sim_network.hpp"
#include "server/server.hpp"
#include "shooters_edge/net/classic_client.hpp"
#include "shooters_edge/net/classic_game.hpp"
#include "shooters_edge/net/classic_info.hpp"
#include "ui/console.hpp"
#include "ui/raster.hpp"

namespace {

namespace gl = ge::lab;
namespace gn = ge::net;
namespace gs = ge::server;
namespace ga = ge::account;
namespace se = shooters_edge::net;
namespace gc = ge::core;
namespace con = ge::console;

constexpr uint16_t kPort = 27500, kClientPort = 27001;
constexpr int64_t kUnix0 = 1794500000ll * 1000000;
int64_t g_unix = kUnix0;  // the session's Unix clock: the servers' and the masters' codes
int64_t unix_now() { return g_unix; }

void counter_fresh(void* ctx, uint8_t* out, size_t size) {
  auto* n = static_cast<uint64_t*>(ctx);
  for (size_t i = 0; i < size; i++) out[i] = static_cast<uint8_t>((*n * 131 + i * 17) >> 1);
  (*n)++;
}

ga::Fresh fresh_of(uint32_t seed, uint8_t b) {
  ga::Fresh f{};
  for (size_t i = 0; i < f.size(); i++)
    f[i] = static_cast<uint8_t>(size_t{seed} * 29 + size_t{b} * 13 + i);
  return f;
}

gn::Key key_of(uint32_t seed, uint8_t b) {
  gn::Key k{};
  for (size_t i = 0; i < k.size(); i++)
    k[i] = static_cast<uint8_t>(size_t{seed} * 3 + size_t{b} * 7 + i);
  return k;
}

struct Player {
  std::string label, name;
  int server = 0;  // 0: the server that is its own master; 1: the one asking a master
  double join_s = 1;
  std::vector<std::pair<double, std::string>> typed;  // console lines and when
  // What happened.
  std::vector<std::string> console;  // "]typed" and the server's prints, in order
  std::string shown, auth;           // its name and login as every client sees them
  bool dropped = false;
};

Player player(std::string label, std::string name, int server, double join_s,
              std::vector<std::pair<double, std::string>> typed) {
  Player p;
  p.label = std::move(label);
  p.name = std::move(name);
  p.server = server;
  p.join_s = join_s;
  p.typed = std::move(typed);
  return p;
}

struct Session {
  std::string error;
  std::vector<Player> players;
  int leaks = 0;  // login lines (or a code) printed to anyone
  int agent = 0;  // lines in an agent's syntax
  uint64_t digest = 1469598103934665603ull;
};

void fold(uint64_t& d, const std::string& s) {
  for (unsigned char c : s) d = (d ^ c) * 1099511628211ull;
}

bool agent_syntax(const std::string& line) {
  return line.find("next:") != std::string::npos || line.find("skill:") != std::string::npos ||
         line.find('`') != std::string::npos;
}

ga::Store accounts(uint32_t seed) {
  ga::Store store;
  ga::store_init(store, ga::HashCost{8, 1});
  ga::create(store, "Niclas", "correct horse", g_unix, fresh_of(seed, 1));
  ga::create(store, "Ann", "battery staple", g_unix, fresh_of(seed, 2));
  return store;
}

Session run_session(uint32_t seed, double seconds, double login_time_s) {
  Session r;
  g_unix = kUnix0;
  gn::SimNetwork net = gn::make_sim_network(seed, gn::LinkConditions{});
  const gn::Address master_at = gn::sim_address(9, ga::kAccountPort);
  gn::Transport own_port, master_port, link, ports[2];
  gn::open_sim_transport(own_port, net, gn::sim_address(8, ga::kAccountPort));
  gn::open_sim_transport(master_port, net, master_at);
  gn::open_sim_transport(link, net, gn::sim_address(11, 40000));
  gn::open_sim_transport(ports[0], net, gn::sim_address(0, kPort));
  gn::open_sim_transport(ports[1], net, gn::sim_address(10, kPort));
  uint64_t fresh_n = seed;
  ga::Service own, master;
  ga::ServiceConfig sc;
  sc.seed = key_of(seed, 1);
  ga::service_init(own, sc, accounts(seed), own_port, counter_fresh, &fresh_n);
  sc.seed = key_of(seed, 2);
  ga::service_init(master, sc, accounts(seed), master_port, counter_fresh, &fresh_n);
  // The codes the players got from the hub's Account page (or the game CLI) before joining.
  const std::string niclas_own = ga::make_code(own.store, 1, g_unix, fresh_of(seed, 10)).code;
  const std::string ann_own = ga::make_code(own.store, 2, g_unix, fresh_of(seed, 11)).code;
  const std::string niclas_master = ga::make_code(master.store, 1, g_unix, fresh_of(seed, 12)).code;

  gs::ServerConfig c[2];
  for (int i = 0; i < 2; i++) {
    c[i].map = "arena";
    c[i].maxclients = 8;
    c[i].seed = seed;
    c[i].unix_us = unix_now;
    c[i].login.time_s = login_time_s;
  }
  c[0].hostname = "Own Accounts";
  c[0].login.login = 1;
  c[0].login.welcome = "Get a login code at this server's web page, under Account.";
  c[0].accounts = &own;
  c[1].hostname = "Community Night";
  c[1].login.login = 2;
  c[1].login.master = gn::to_string(master_at);
  c[1].login.key = gn::key_to_hex(ga::service_key(master));
  c[1].account_link = &link;
  gs::Server s[2];
  for (int i = 0; i < 2; i++)
    if (!gs::server_open(s[i], se::classic_game(), c[i], &ports[i], nullptr, r.error)) return r;

  r.players = {
      player("own-master", "niclas", 0, 1.0,
             {{3.0, "say login niclas " + niclas_own}, {6.0, "say gg"}}),
      player("guest", "N1clas", 0, 2.0, {{5.0, "say hi all"}}),
      player("wrong-code", "ann", 0, 3.0,
             {{6.0, "login ann 7Q2M-K4XD"},
              {8.0, "login ann " + ann_own},
              {10.0, "setinfo name annie"}}),
      player("two-step", "nic", 1, 1.0, {{4.0, "login niclas"}, {6.0, "say " + niclas_master}}),
      player("spent-code", "spy", 0, 2.0,
             {{8.0, "say login niclas " + niclas_own}, {9.0, "say hello"}}),
  };
  const size_t n = r.players.size();
  std::vector<gn::Transport> ct(n);
  std::vector<se::ClassicClientEnd> ends(n);
  std::vector<size_t> next_typed(n, 0), heard(n, 0);
  for (size_t i = 0; i < n; i++)
    gn::open_sim_transport(ct[i], net, gn::sim_address(static_cast<uint32_t>(20 + i), kClientPort));
  const auto frames = static_cast<int64_t>(gc::num::round(seconds / s[0].frame_s));
  for (int64_t f = 0; f <= frames; f++) {
    const int64_t now = s[0].now_us;
    const double t = static_cast<double>(now) / 1e6;
    g_unix = kUnix0 + now;
    gn::sim_advance(net, now);
    for (size_t i = 0; i < n; i++) {
      Player& p = r.players[i];
      if (t < p.join_s) continue;
      if (ends[i].transport == nullptr) {
        se::ClientConfig cc;
        cc.name = p.name;
        cc.files = {"skins/base.pcx"};
        cc.seed = static_cast<int>(i + 1);
        cc.qport = static_cast<uint16_t>(1000 + i);
        se::client_start(ends[i], ct[i], gn::sim_address(p.server == 0 ? 0 : 10, kPort), cc, t);
      }
      if (next_typed[i] < p.typed.size() && t >= p.typed[next_typed[i]].first &&
          ends[i].stage == se::ClientStage::active) {
        const std::string& line = p.typed[next_typed[i]].second;
        // What the player typed, as the console shows it (a chat line as the player wrote it).
        p.console.push_back("]" + (line.rfind("say ", 0) == 0 ? line.substr(4) : line));
        se::client_command(ends[i], line);
        next_typed[i]++;
      }
      se::client_tick(ends[i], now);
      for (; heard[i] < ends[i].prints.size(); heard[i]++) {
        std::string line = ends[i].prints[heard[i]];
        while (!line.empty() && line.back() == '\n') line.pop_back();
        p.console.push_back(line);
      }
    }
    for (gs::Server& one : s) gs::server_frame(one);
    ga::service_update(master, g_unix);
    gn::sim_advance(net, now);
  }
  const std::vector<std::string> codes = {niclas_own, ann_own, niclas_master};
  for (size_t i = 0; i < n; i++) {
    Player& p = r.players[i];
    const se::ClassicClientEnd& e = ends[i];
    p.dropped = e.stage != se::ClientStage::active;
    if (e.playernum >= 0 && e.playernum < 32) {
      // As another client on its server sees it: the first other player's view of this seat.
      const std::string& info = e.userinfos[e.playernum];
      p.shown = se::info_get(info, "name");
      p.auth = se::info_get(info, "*auth");
    }
    for (size_t k = 0; k < p.console.size(); k++) {
      const std::string& line = p.console[k];
      if (line.empty() || line[0] == ']') continue;
      if (agent_syntax(line)) r.agent++;
      for (const std::string& code : codes)
        if (line.find(code) != std::string::npos) r.leaks++;
      if (line.find("login niclas") != std::string::npos ||
          line.find("login ann") != std::string::npos)
        r.leaks++;
      fold(r.digest, line);
    }
    fold(r.digest, p.shown + "|" + p.auth + (p.dropped ? "|dropped" : "|on"));
  }
  return r;
}

void draw(gl::Picture& p, const con::ConsoleView& v) {
  ge::ui::Image img;
  img.width = p.width, img.height = p.height;
  img.rgba = std::move(p.rgba);
  ge::ui::DrawList dl;
  dl.begin(img.width, img.height);
  ge::ui::draw_console(dl, v);
  ge::ui::render(dl, img);
  p.rgba = std::move(img.rgba);
}

gl::Picture console_picture(const Player& p, int w, int h) {
  con::Console shown;
  con::ConsoleView v(shown);
  shown.execute("con_height 1");
  for (const std::string& line : p.console)
    v.add(line.rfind(']', 0) == 0 ? con::LineKind::command : con::LineKind::output, line);
  v.set_open(true);
  for (int f = 0; f < 600 && v.drop() < 1; f++) v.frame(1.0 / 60);
  gl::Picture pic = gl::create_picture(w, h, {24, 20, 16});
  draw(pic, v);
  return pic;
}

std::string hex(uint64_t v) {
  char out[20];
  std::snprintf(out, sizeof out, "%016llx", static_cast<unsigned long long>(v));
  return out;
}

void run(gl::Lab& lab) {
  const auto seed = static_cast<uint32_t>(lab.arg_num("seed"));
  const double seconds = lab.arg_num("seconds");
  const double login_time = lab.arg_num("logintime");
  const Session a = run_session(seed, seconds, login_time);
  const Session b = run_session(seed, seconds, login_time);
  if (!a.error.empty()) {
    lab.fail("the dedicated servers did not open: " + a.error,
             "Fix what the error names (games/shooters_edge/net/classic_login.cpp, "
             "engine/server/login.cpp), then run the lab again; skill: net.");
    return;
  }
  int in = 0, dropped = 0;
  gl::LabTable table{
      "each player: the server, what it typed, how it ends",
      {"player", "server", "joined as", "shown as", "login (*auth)", "on the server"},
      {}};
  for (const Player& p : a.players) {
    in += p.auth.empty() ? 0 : 1;
    dropped += p.dropped ? 1 : 0;
    table.rows.push_back({p.label, p.server == 0 ? "own master" : "master elsewhere", p.name,
                          p.shown, p.auth.empty() ? "-" : p.auth, p.dropped ? "dropped" : "yes"});
  }
  lab.metric("logged_in", in, "players");
  lab.metric("dropped", dropped, "players");
  lab.metric("login_lines_shown", a.leaks, "lines");
  lab.metric("lines_in_agent_syntax", a.agent, "lines");
  lab.metric("disagreeing_runs", a.digest == b.digest ? 0 : 1, "runs");
  lab.table(std::move(table));
  lab.note("digest " + hex(a.digest) + ", again " + hex(b.digest));
  const int w = static_cast<int>(lab.arg_num("width"));
  const int h = static_cast<int>(lab.arg_num("height"));
  struct Shot {
    const char* label;
    const char* title;
    const char* what;
    const char* expect;
  };
  const Shot shots[] = {
      {"own-master", "LOGGED IN ON A SERVER THAT IS ITS OWN MASTER",
       "A stock client joins a server with sv_login 1 and no sv_accountmaster: the server keeps "
       "its own accounts. The player types login with the one-time code from the server's web "
       "page; the client sends a line it does not know as chat, and the server takes it before "
       "anyone sees it.",
       "the welcome and the prompt, then 'Niclas logged in.' and the player's chat after it"},
      {"guest", "A GUEST UNDER A REGISTERED NAME",
       "A player joins as N1clas, which folds to the registered Niclas: the server seats him under "
       "its default name and says why. He chats before logging in, which a seat that must log in "
       "may not, is warned 10 seconds before his time is up, and is dropped.",
       "the name refused, the chat held, the warning, the drop"},
      {"wrong-code", "A WRONG CODE, THEN THE RIGHT ONE, THEN A RENAME",
       "Ann types a code that is not hers, then her own, then tries to change her name, which "
       "sv_loginname 1 keeps as her account's.",
       "the refusal in plain words, then logged in, then the rename refused"},
      {"two-step", "THE TWO-STEP LOGIN ON A COMMUNITY'S SERVER",
       "A server with sv_login 2 that asks a community's master over its own sealed channel. The "
       "player types login and his name, is asked for the code, and types it alone on the next "
       "line; the master's ticket is checked against sv_accountkey, and he is shown by his "
       "account's name.",
       "'Enter your login code for niclas:', then 'Niclas logged in.' and the name Niclas"},
      {"spent-code", "A CODE READ OFF THE WIRE",
       "The classic protocol carries the login line in plain text. Someone who read Niclas's code "
       "on the wire types it after him: a code works once, so it logs nobody in. He cannot chat "
       "either, and is dropped when his time is up.",
       "the refusal, the chat held, the drop"},
  };
  for (const Shot& one : shots)
    for (const Player& p : a.players)
      if (p.label == one.label)
        lab.shot({one.label, one.title, one.what, one.expect, {{"seed", lab.arg("seed")}}},
                 console_picture(p, w, h));
}

const gl::LabSpec kSpec{
    "se-login",
    gl::LabKind::sim,
    "game:shooters_edge",
    "games/shooters_edge/net/classic_login and engine/server/login (logging in from a stock "
    "client)",
    "net",
    "Does a stock classic client log in on our servers with a one-time code, by either form, on a "
    "server that is its own master and on one asking a master, with names, chat and time held as "
    "the design says and no login line shown to anyone?",
    {
        {"seed", "1", "the network's and the codes' stream"},
        {"seconds", "45", "server time to run, s"},
        {"logintime", "30", "sv_logintime, s"},
        {"width", "960", "each console picture's width, px"},
        {"height", "540", "each console picture's height, px"},
    },
    "--width=640 --height=360",
    run,
};
GE_LAB(kSpec);

}  // namespace
