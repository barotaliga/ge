// The words a player types that go to the server joined (qw_remote.hpp): the lobby's, with
// `team` kept as the player's user info on a classic QuakeWorld server, and the rules' words a
// classic server takes.
//
// ge:summary The typed words sent to the server joined: the lobby's, team as user info, the rules'
// ge:keywords remote_lobby remote_rules_word team setinfo lobby-words kill pause serverinfo
// ge:skill net shooter
// ge:test tests/shooters_edge_remote_test.cpp tests/shooters_edge_lobby_test.cpp
#include <string>

#include "console/console.hpp"
#include "shooters_edge/net/qw_remote.hpp"

namespace shooters_edge::net {

bool remote_lobby(Remote& r, const std::string& line) {
  if (!r.active || r.watch) return false;
  // On a classic QuakeWorld server `team` is the player's team in his user info, as Quake's
  // client has it: `team blue` sets it (the server tells everyone), `team` alone says it. Our own
  // server takes the lobby's word itself, and keeps the user info's meaning where no lobby runs.
  const bool classic = !r.q2 && !r.nq && !r.end.ext.has(ExtFamily::ge, kGeSettings);
  const size_t gap = line.find_first_of(" \t");
  if (classic && line.substr(0, gap) == "team") {
    ClientConfig& c = r.end.cfg;
    std::string name = gap == std::string::npos ? "" : line.substr(gap);
    name.erase(0, name.find_first_not_of(" \t\""));
    name.erase(std::min(name.size(), name.find_first_of('"')));
    while (!name.empty() && (name.back() == ' ' || name.back() == '\t')) name.pop_back();
    if (gap == std::string::npos || name.empty()) {
      if (r.ignore.con != nullptr) r.ignore.con->print("\"team\" is \"" + c.team + "\"");
      return true;
    }
    c.team = name;
    client_command(r.end, "setinfo team \"" + c.team + "\"");
    return true;
  }
  remote_command(r, line);
  return true;
}

bool remote_rules_word(Remote& r, const std::string& line) {
  if (!r.active || r.watch) return false;
  if (r.nq) {  // a server of protocol 15: the words it takes from a player
    if (!nq_server_word(line.substr(0, line.find_first_of(" \t")))) return false;
    nq_command(*r.nq, line);
    return true;
  }
  if (!r.q2) {  // Quake's client sends on only its server's own words, never any unknown one
    const std::string word = line.substr(0, line.find_first_of(" \t"));
    // Our own server's match words (it agreed the ge set's settings bit): its stats.
    const bool ours = r.end.ext.has(ExtFamily::ge, kGeSettings);
    const bool stats_word = word == "stats" || word == "teamstats" || word == "laststats";
    if (word != "kill" && word != "pause" && word != "serverinfo" && !(ours && stats_word))
      return false;
  }
  remote_command(r, line);  // Quake II's: every word the window does not know (`use Railgun`)
  return true;
}

}  // namespace shooters_edge::net
