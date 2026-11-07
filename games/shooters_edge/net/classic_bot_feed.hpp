// A BOT TEAM'S MATCH AS A FEED, AND A CONTROL FILE BACK: what a person (or an agent) directing a
// team of bots on a live server (classic_bot_team.hpp) reads as the match goes, and how its lines
// reach the bots while they play.
//
// THE FEED is JSON, one object a line, appended as the match goes, so `tail -f` reads it live:
// - every line the team's log takes (`log`): a server's print folded to plain text, a bot's own
//   line (a death, a setting taken), the console's answers. A print that is a player's words is
//   `chat` (`name: words`) or `team_chat` (`(name): words`) with `from` and `text`, so a person's
//   words to the bots stand apart from the server's;
// - every `every_s` a `status`: the map, the score (this team's against the rest), every player
//   on the server (slot, name, team, frags, a bot of this team's or not) and each bot's state
//   (its console name, its name, skill, health, armour, weapon in hand, rockets and cells, mode,
//   role, goal, place, origin, frags, deaths, round trip).
//
// THE CONTROL FILE is any text file the bots' process watches: each whole line appended to it is
// a console line (`bot_team_command`: a line to every bot, `bot<N> <line>` to one), so
// `echo 'bot2 say_team "i take ra"' >> control.txt` speaks through the second bot.
//
// ge:summary A bot team's live match as a JSON feed, and a control file whose lines reach the bots
// ge:keywords bot-feed live-feed json-lines tail chat team-chat status control-file direct-bots
// ge:keywords bot_feed_line bot_feed_status control_poll stream-match steer-bots turing pickup
// ge:skill bots net
// ge:test tests/shooters_edge_bot_team_test.cpp
#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "shooters_edge/net/classic_bot_team.hpp"

namespace shooters_edge::net {

// The feed's line for a log line `line` at `t_s` seconds: `log`, `chat` or `team_chat`.
std::string bot_feed_line(const BotTeam& t, double t_s, const std::string& line);
// The feed's `status` line at `t_s` seconds.
std::string bot_feed_status(const BotTeam& t, double t_s);

// A text file watched for lines appended to it.
struct ControlFile {
  std::string path;
  uint64_t offset = 0;  // the bytes read so far
  std::string pending;  // read, not yet a whole line
};
// The whole lines appended since the last call (none when the file is missing or unchanged).
std::vector<std::string> control_poll(ControlFile& c);

}  // namespace shooters_edge::net
