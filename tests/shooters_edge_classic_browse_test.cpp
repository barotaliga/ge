// games/shooters_edge/net/classic_browse.hpp: the status query's bytes; a reply with players,
// a spectator and teams read into the server list's words; the mode from the server's keys
// (its own `mode`, coop, team, ffa, single); a password and the skill; a reply that is not one
// refused; the master's reply read into addresses; an install's list of masters.
//
// ge:summary Test: the classic status reply and master reply read for the server browser
// ge:keywords test classic status-query status-reply master-reply mode coop team spectator
// ge:skill net testing
#include "shooters_edge/net/classic_browse.hpp"

#include <cstring>
#include <string>
#include <vector>

#include "check.hpp"

namespace gn = ge::net;
namespace se = shooters_edge::net;

namespace {

std::vector<uint8_t> bytes(const std::string& s) { return {s.begin(), s.end()}; }

bool read(const std::string& text, gn::ServerInfo& info) {
  const std::vector<uint8_t> b = bytes(std::string("\xff\xff\xff\xffn", 5) + text);
  return se::classic_read_status(b.data(), b.size(), info);
}

}  // namespace

TEST(classic_status_query_bytes) {
  uint8_t out[64];
  const size_t n = se::classic_status_query(out, sizeof out);
  CHECK(n == 14);
  CHECK(std::memcmp(out, "\xff\xff\xff\xffstatus 23\n", 14) == 0);
  CHECK(se::classic_master_query(out, sizeof out) == 3);
  CHECK(out[0] == 'c' && out[1] == '\n' && out[2] == 0);
}

TEST(classic_status_reply_read) {
  gn::ServerInfo info;
  CHECK(
      read("\\maxfps\\77\\hostname\\Friday night 4on4\\map\\dm3\\maxclients\\8"
           "\\maxspectators\\4\\deathmatch\\1\\teamplay\\2\\*gamedir\\ktx\\status\\Countdown\n"
           "12 31 5 25 \"alpha\" \"base\" 4 4 \"red\"\n"
           "13 -2 5 40 \"bravo two\" \"\" 13 13 \"blue\"\n"
           "14 -9999 1 -30 \"\\s\\watcher\" \"\" 0 0 \"\"\n",
           info));
  CHECK(info.name == "Friday night 4on4");
  CHECK(info.map == "dm3");
  CHECK(info.mode == "team");
  CHECK(info.game == "ktx");
  CHECK(info.state == "Countdown");
  CHECK(info.players == 2 && info.max_players == 8);
  CHECK(info.spectators == 1 && info.max_spectators == 4);
  CHECK(info.skill == -1);
  CHECK(!info.locked);
  CHECK(info.roster.size() == 3);
  CHECK(info.roster[1].name == "bravo two");
  CHECK(info.roster[1].frags == -2);
  CHECK(info.roster[1].team == "blue");
  CHECK(info.roster[2].spectator);
  CHECK(info.roster[2].name == "watcher");
  CHECK(info.roster[2].ping_ms == 30);
  CHECK(info.keys.size() == 9);
}

TEST(classic_status_reply_modes) {
  gn::ServerInfo info;
  CHECK(read("\\hostname\\x\\coop\\1\\deathmatch\\0\\skill\\2\\maxclients\\8\n", info));
  CHECK(info.mode == "coop");
  CHECK(info.skill == 2);
  CHECK(read("\\hostname\\x\\deathmatch\\3\n", info));
  CHECK(info.mode == "ffa");
  CHECK(read("\\hostname\\x\\deathmatch\\1\\mode\\duel\n", info));
  CHECK(info.mode == "duel");
  CHECK(read("\\hostname\\x\\needpass\\1\n", info));
  CHECK(info.mode == "single");
  CHECK(info.locked);
  CHECK(info.players == 0);
}

TEST(classic_status_reply_refuses_other_datagrams) {
  gn::ServerInfo info;
  const std::vector<uint8_t> ping = bytes("\xff\xff\xff\xffl");
  CHECK(!se::classic_read_status(ping.data(), ping.size(), info));
  const std::vector<uint8_t> print = bytes("\xff\xff\xff\xffnno info here\n");
  CHECK(!se::classic_read_status(print.data(), print.size(), info));
  const std::vector<uint8_t> game = bytes("\x01\x00\x00\x00\x02\x00");
  CHECK(!se::classic_read_status(game.data(), game.size(), info));
}

TEST(classic_master_reply_read) {
  const uint8_t reply[] = {0xff, 0xff, 0xff, 0xff, 'd',  '\n', 10, 0, 0, 7, 0x6b, 0x6c,
                           192,  168,  1,    20,   0x6b, 0x6c, 1,  2, 3, 4, 0,    0};
  std::vector<gn::Address> got;
  CHECK(se::classic_read_master(reply, sizeof reply, got));
  CHECK(got.size() == 2);  // the port-0 row skipped
  CHECK(gn::to_string(got[0]) == "10.0.0.7:27500");
  CHECK(gn::to_string(got[1]) == "192.168.1.20:27500");
  gn::ServerInfo info;
  CHECK(!se::classic_read_status(reply, sizeof reply, info));
  const uint8_t status[] = {0xff, 0xff, 0xff, 0xff, 'n', '\\'};
  CHECK(!se::classic_read_master(status, sizeof status, got));
}

TEST(classic_masters_in_an_install_list) {
  const std::vector<std::string> m = se::classic_masters_in(
      "// sources\nmaster \"One\" one.example:27000\nfile \"Local\" servers.txt\n"
      "master \"Two\" 192.0.2.4:27000\r\n");
  CHECK(m.size() == 2);
  CHECK(m[0] == "one.example:27000");
  CHECK(m[1] == "192.0.2.4:27000");
}
