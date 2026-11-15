// The administrator's trial (admin_trial.hpp).
//
// ge:summary The admin trial defined: the script, the players' clients, the fold
// ge:keywords server admin trial script fold
// ge:skill net
// ge:test tests/server_feature_test.cpp
#include "server/admin_trial.hpp"

#include <map>
#include <memory>

#include "net/sim_network.hpp"
#include "server/admin.hpp"
#include "server/feature.hpp"
#include "server/sample.hpp"

namespace ge::server {

namespace gn = ge::net;

namespace {

constexpr uint32_t kServerNode = 1, kFirstClient = 10;
constexpr uint16_t kClientPort = 40000;

uint64_t fold(uint64_t h, uint64_t v) {
  for (int i = 0; i < 8; i++) h = (h ^ ((v >> (8 * i)) & 0xff)) * 1099511628211ull;
  return h;
}
uint64_t fold_text(uint64_t h, const std::string& s) {
  for (char c : s) h = (h ^ static_cast<uint8_t>(c)) * 1099511628211ull;
  return fold(h, s.size());
}

struct Client {
  std::string name;
  gn::Transport t;
  gn::Node node;
  gn::SessionClient client;
};

// THE TRIAL'S OWN FEATURE: `hello` answered with the player's name.
bool hello_command(void*, Features& f, int player, const std::vector<std::string>& w,
                   std::string_view) {
  if (w[0] != "hello") return false;
  feature_answer(f, player, "Hello, " + std::string(f.server->session.players[player].name) + ".");
  return true;
}

}  // namespace

AdminTrial run_admin_trial(const AdminTrialConfig& tc) {
  AdminTrial out;
  const ServerGame& game = sample_game();
  console::Console con(true);
  std::vector<std::string> console_said;
  con.set_sink([&](std::string_view l) { console_said.emplace_back(l); });
  std::map<std::string, std::string> files;
  con.set_files(
      [&](std::string_view name, std::string& text) {
        const auto it = files.find(std::string(name));
        if (it == files.end()) return false;
        text = it->second;
        return true;
      },
      [&](std::string_view name, std::string_view text) {
        files[std::string(name)] = std::string(text);
        return true;
      });
  ServerConfig c;
  add_server_cvars(con.cvars(), c, game);
  AdminFeatures admin;
  Features features;
  features.list = admin_features(admin);
  Feature hello;
  hello.name = "hello";
  hello.command = hello_command;
  features.list.push_back(hello);
  features_attach(features, con);
  con.execute("set rcon_password \"" + tc.rcon_password + "\"; set sv_maxclients 8",
              console::Source::local);
  c.offline = false;
  c.seconds = 16;
  c.hostname = "Admin Night";

  gn::SimNetwork net = gn::make_sim_network(tc.seed, tc.link);
  gn::Transport sock;
  gn::open_sim_transport(sock, net, gn::sim_address(kServerNode, static_cast<uint16_t>(c.port)));
  Server s;
  s.features = &features;
  if (!server_open(s, game, c, &sock, nullptr, out.error)) return out;

  const char* names[] = {"Ana", "Bo", "Cy"};
  std::vector<std::unique_ptr<Client>> clients;
  const auto open_client = [&](int i, uint32_t node_id, uint16_t port) {
    auto p = std::make_unique<Client>();
    p->name = names[i];
    gn::open_sim_transport(p->t, net, gn::sim_address(node_id, port));
    gn::NodeConfig nc;
    nc.protocol = game.protocol;
    nc.channels.count = 1;
    nc.channels.delivery[0] = gn::Delivery::reliable;
    nc.seed = uint64_t{tc.seed} * 1000 + node_id * 7 + port;
    gn::node_init(p->node, nc, p->t);
    gn::session_client_init(p->client, p->node, 0);
    return p;
  };
  for (int i = 0; i < 3; i++)
    clients.push_back(open_client(i, kFirstClient + static_cast<uint32_t>(i), kClientPort));

  // THE SCRIPT: (time, who, what). `join` (re)joins; anything else is a command line.
  struct Step {
    double at_s;
    int who;
    const char* what;
  };
  const std::string ok = "rcon " + tc.rcon_password + " ";
  const std::vector<std::pair<Step, std::string>> script = {
      {{1.0, 0, "join"}, ""},
      {{1.2, 1, "join"}, ""},
      {{1.4, 2, "join"}, ""},
      {{3.0, 0, ""}, "status"},
      {{4.0, 0, ""}, "rcon wrong kick Bo"},
      {{5.0, 0, ""}, ok + "kick Bo"},
      {{6.5, 1, "join"}, ""},
      {{8.0, 0, ""}, ok + "ban Cy"},
      {{9.0, 0, ""}, ok + "status"},
      {{9.5, 2, "join"}, ""},
      {{10.0, 0, ""}, ok + "listip"},
      {{10.5, 0, ""}, "dance"},
      {{11.0, 0, ""}, "hello"},
      {{12.0, 0, ""}, "rcon guess1 status"},
      {{12.5, 0, ""}, "rcon guess2 status"},
      {{13.0, 0, ""}, "rcon guess3 status"},
      {{13.5, 0, ""}, ok + "status"},
  };
  size_t next = 0;
  const auto frames = static_cast<int64_t>(c.seconds / s.frame_s + 0.5);
  bool bo_was_kicked = false;
  for (int64_t n = 0; n < frames; n++) {
    const int64_t now = server_next_us(s);
    gn::sim_advance(net, now);
    server_frame(s);
    const double t = static_cast<double>(now) / 1e6;
    while (next < script.size() && script[next].first.at_s <= t) {
      const Step& st = script[next].first;
      Client& p = *clients[st.who];
      if (std::string(st.what) == "join") {
        if (p.client.state != gn::ClientState::idle) {
          gn::session_client_leave(p.client, now);
          // A fresh client from the same machine: a new port, the old link's word forgotten.
          gn::close_transport(p.t);
          clients[st.who] = open_client(st.who, kFirstClient + static_cast<uint32_t>(st.who),
                                        static_cast<uint16_t>(kClientPort + 1));
        }
        Client& q = *clients[st.who];
        gn::session_client_join(q.client, sock.local, q.name.c_str(), 0, now);
      } else {
        const std::string& line = script[next].second;
        gn::session_client_command(p.client, line.c_str(), line.size());
        out.lines.push_back({t, p.name, line, true});
      }
      next++;
    }
    for (auto& cp : clients) {
      Client& p = *cp;
      gn::node_update(p.node, now);
      gn::session_client_update(p.client, now);
      for (const gn::SessionEvent& e : p.client.events) {
        if (e.type == gn::SessionEventType::print) {
          const std::string text(gn::session_text(p.client.event_bytes, e), e.size);
          out.lines.push_back({t, p.name, text, false});
          if (text == "Bad rcon password.") out.wrong_refused = true;
          if (text.rfind("Too many wrong passwords", 0) == 0) out.locked = true;
          if (text == "Hello, Ana.") out.hello_answered = true;
          if (text == "Unknown command \"dance\".") out.unknown_told = true;
        } else if (e.type == gn::SessionEventType::kicked) {
          out.lines.push_back(
              {t, p.name, std::string("(removed: ") + gn::why_name(e.why) + ")", false});
          if (p.name == "Bo") bo_was_kicked = out.bo_kicked = true;
          if (p.name == "Cy") out.cy_banned = true;
        } else if (e.type == gn::SessionEventType::welcome && p.name == "Bo" && bo_was_kicked) {
          out.bo_back = true;
        } else if (e.type == gn::SessionEventType::refused && e.ask == gn::SessionAsk::join &&
                   p.name == "Cy") {
          out.cy_refused = true;
          out.lines.push_back(
              {t, p.name, std::string("(refused: ") + gn::why_name(e.why) + ")", false});
        }
      }
      gn::node_flush(p.node, now);
    }
  }
  out.log.assign(admin.log.begin(), admin.log.end());
  out.ban_file = files.count("listip.cfg") ? files["listip.cfg"] : "";
  out.rcon_runs = admin.rcon_runs;
  out.rcon_refused = admin.rcon_refused;
  out.commands = features.commands;
  out.refused_joins = s.session.stats.refused_joins;
  out.seated_end = gn::session_player_count(s.session, false);
  uint64_t h = 1469598103934665603ull;
  for (const AdminLine& l : out.lines)
    h = fold_text(fold_text(fold(h, static_cast<uint64_t>(l.at_s * 1e3)), l.player), l.text);
  for (const std::string& l : out.log) h = fold_text(h, l);
  h = fold_text(h, out.ban_file);
  out.digest = fold(h, static_cast<uint64_t>(out.seated_end));
  out.ok = out.bo_kicked && out.bo_back && out.cy_banned && out.cy_refused && out.wrong_refused &&
           out.locked && out.hello_answered && out.unknown_told && out.rcon_runs >= 3;
  server_close(s);
  gn::close_transport(sock);
  for (auto& p : clients) gn::close_transport(p->t);
  return out;
}

}  // namespace ge::server
