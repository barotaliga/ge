// The rules' own settings for the reference manual, made with no map (rules_manual.hpp).
//
// ge:summary The rules' own settings made with no map for the reference manual: Quake's, Quake II's
// ge:keywords rules-settings manual rules_manual add_mod_settings make_q2_server
// ge:skill console shooter
// ge:test tests/shooters_edge_manual_test.cpp
#include "shooters_edge/server/rules_manual.hpp"

#include <memory>
#include <string>

#include "shooters_edge/server/q2_server.hpp"
#include "shooters_edge/server/rules_runner.hpp"

namespace shooters_edge::server {

void rules_manual(std::string_view games, std::vector<ge::console::ManualEntry>& out) {
  const std::string first(games.substr(0, games.find(',')));
  const std::string layer = "game:" + first;
  std::vector<ge::console::ManualEntry> made;
  if (first == "q1" || first == "qw") {
    auto rules = std::make_unique<RulesCommon>();
    add_mod_settings(*rules, ge::shooter::ServerSettings{});
    made = ge::console::describe_settings(rules->cvars, layer);
  } else if (first == "q2") {
    const std::unique_ptr<Q2Server> rules = make_q2_server({});
    made = ge::console::describe_settings(rules->cvars, layer);
  }
  out.insert(out.end(), made.begin(), made.end());
}

}  // namespace shooters_edge::server
