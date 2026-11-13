// A FINISHED MATCH'S REPORT (docs/accounts.md § The match report): what a match leaves, the same
// record for every game. A server fills it when a match ends (engine/server's `ServerGame::
// match_report`); the master it reports to adds each logged-in player's part to the account's
// totals, the hub shows it, and a recorded match's replay file closes with it.
//
// The bytes (little endian; a string is a length u8 and its bytes, cut at kReportName; a number is
// a varint, signed ones zigzagged):
//   "geMR" version u8 (1), id u64, game, version, server, address, mode, map, demo_sha256,
//   start_s, duration_ms, ended u8, columns (count u8, each a name), teams (count u8, each name,
//   score, colour u32), players (count u8, each name, team, account u64, bot u8, score, frags,
//   deaths, suicides, team_kills, seconds, ping_min, ping_avg, ping_max, then one number per
//   column)
// A report past its limits (kReportPlayers, kReportTeams, kReportColumns) is refused by the
// writer, never cut: a report that says less than the match is worse than none. The reader refuses
// damage (a short record, a count past its limit, an unknown version) with what and where.
//
//   report_write(report, out, error) -> false when past a limit
//   report_read(bytes, size, out, error) -> false when damaged
//
// ge:summary A finished match's report: the match, its teams, its players and the game's columns
// ge:keywords match-report report_write report_read scoreboard frags deaths suicides team-kills
// ge:keywords account-totals game-columns closing-summary
// ge:skill net
// ge:test tests/net_report_test.cpp
#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace ge::net {

constexpr size_t kReportName = 64;  // a string's bytes kept (a SHA-256 in hex fits)
constexpr size_t kReportPlayers = 64, kReportTeams = 16, kReportColumns = 64;

enum class MatchEnd : uint8_t {
  limit,    // a frag or time limit, as the rules end a match
  forfeit,  // a side left
  broken    // broken off (the map changed by hand, the server stopped)
};
const char* match_end_name(MatchEnd e);

struct ReportTeam {
  std::string name;
  int64_t score = 0;
  uint32_t colour = 0;  // the game's own colour number
};

struct ReportPlayer {
  std::string name, team;  // team "" without teams
  uint64_t account = 0;    // 0: a guest
  bool bot = false;
  int64_t score = 0, frags = 0, deaths = 0, suicides = 0, team_kills = 0;
  int64_t seconds = 0;                               // played
  int64_t ping_min = 0, ping_avg = 0, ping_max = 0;  // ms over the time played (0: not known)
  std::vector<int64_t> columns;                      // one per MatchReport::columns, in order
};

struct MatchReport {
  uint64_t id = 0;  // the server's: its count of matches reported, mixed with its start
  std::string game, version, server, address, mode, map;
  std::string demo_sha256;  // hex, "" when not recorded
  int64_t start_s = 0;      // Unix seconds
  int64_t duration_ms = 0;
  MatchEnd ended = MatchEnd::limit;
  std::vector<std::string> columns;  // the game's own, named (its hub labels them)
  std::vector<ReportTeam> teams;
  std::vector<ReportPlayer> players;
};

bool report_write(const MatchReport& r, std::vector<uint8_t>& out, std::string& error);
bool report_read(const uint8_t* data, size_t size, MatchReport& out, std::string& error);

}  // namespace ge::net
