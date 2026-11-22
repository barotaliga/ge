// games/shooters_edge/net/qw_serverstatus.hpp: an answer of either game read into the block
// serverstatus prints (info keys padded, players by score), and the command's refusals.
//
// ge:summary Test: serverstatus's block for both games' answers and refusals; download's checks
// ge:keywords test serverstatus status-response players score ping
// ge:skill net testing
#include "shooters_edge/net/qw_serverstatus.hpp"

#include <string>
#include <vector>

#include "check.hpp"
#include "console/console.hpp"
#include "shooters_edge/net/qw_remote.hpp"

namespace cn = shooters_edge::net;

TEST(quake2_answer_prints_info_then_players_by_score) {
  std::vector<std::string> out;
  CHECK(cn::format_status("10.0.0.2:27910",
                          "\\hostname\\Base\\mapname\\q2dm1\n3 50 \"low\"\n12 40 \"high\"\n", out));
  CHECK(out.size() == 8);
  CHECK(out[0] == "Status response from 10.0.0.2:27910");
  CHECK(out[1].empty());
  CHECK(out[2] == "hostname             Base");
  CHECK(out[3] == "mapname              q2dm1");
  CHECK(out[4].empty());
  CHECK(out[5] == "Num Score Ping Name");
  CHECK(out[6] == "  1    12   40 high");
  CHECK(out[7] == "  2     3   50 low");
}

TEST(quakeworld_answer_reads_frags_and_ping) {
  std::vector<std::string> out;
  CHECK(cn::format_status("h:27500", "\\hostname\\qw\n5 21 10 33 \"bob\" \"base\" 4 4 \"red\"\n",
                          out));
  CHECK(out.back() == "  1    21   33 bob");
}

TEST(not_an_answer_and_bad_info) {
  std::vector<std::string> out;
  CHECK(!cn::format_status("h", "hello\n", out));
  CHECK(out.empty());
  CHECK(cn::format_status("h", "\\odd\n", out));  // a key with no value: not valid
  CHECK(out[2] == "hostname             badinfo");
}

TEST(command_needs_an_address_and_reads_it) {
  ge::console::Console con{false};
  cn::Remote r;
  cn::add_remote_commands(con, r);
  con.execute("serverstatus");
  CHECK(con.output().back() == "Usage: serverstatus [address]");
  con.execute("serverstatus not..an..address..");
  CHECK(con.output().back() == "Bad address: not..an..address..");
}

TEST(download_checks_the_path_and_queues_it) {
  ge::console::Console con{false};
  cn::Remote r;
  cn::add_remote_commands(con, r);
  con.execute("download maps/q2dm1.bsp");
  CHECK(con.output().back() == "Must be connected to a server.");
  con.execute("skins");
  CHECK(con.output().back() == "Must be in a level to load skins.");
  cn::Q2ClientEnd c;
  c.fetching = "maps/q2dm8.bsp";  // a fetch under way: the next waits behind it
  CHECK(cn::q2_download(c, "pics/colormap.pcx") == "");
  CHECK(c.fetch_queue.size() == 1 && c.fetch_queue[0] == "pics/colormap.pcx");
  CHECK(cn::q2_download(c, "PICS/ColorMap.PCX") == "File exists");  // lowered, already queued
  CHECK(cn::q2_download(c, "maps/q2dm8.bsp") == "File exists");     // the one coming
  CHECK(cn::q2_download(c, "config.cfg") == "Invalid quake path");
  CHECK(cn::q2_download(c, "maps/q2dm1.exe") == "Invalid quake path");
  CHECK(cn::q2_download(c, "maps/../q2dm1.bsp") == "Invalid quake path");
  CHECK(cn::q2_download(c, "save/game.txt") == "Operation not permitted");
  CHECK(cn::q2_download(c, "maps/" + std::string(60, 'a') + ".bsp") == "File name too long");
}
