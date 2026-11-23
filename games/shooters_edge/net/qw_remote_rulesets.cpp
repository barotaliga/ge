// The ruleset on a server joined (qw_remote_rulesets.hpp).
//
// ge:summary The ruleset on a server joined: the book, the lock, f_ruleset answers said in chat
// ge:keywords ruleset f_ruleset fair-play playing standby status
// ge:skill classic net console
// ge:test tests/shooters_edge_remote_rulesets_test.cpp
#include "shooters_edge/net/qw_remote_rulesets.hpp"

#include <cstdlib>
#include <memory>
#include <string>

#include "classic/rulesets.hpp"
#include "console/ruleset.hpp"
#include "shooters_edge/net/protocol.hpp"
#include "shooters_edge/net/qw_remote.hpp"

namespace shooters_edge::net {

namespace {

namespace gc = ge::console;

bool on(gc::Console& con, const char* name) {
  const gc::Cvar* v = con.cvars().find(name);
  return v != nullptr && v->on();
}

bool watching(const Remote& r) { return r.q2 ? r.q2->cfg.spectator : r.end.cfg.spectator; }

}  // namespace

bool remote_playing(const Remote& r) {
  if (!r.active || watching(r)) return false;
  for (const auto& [key, value] : r.demo.state.serverinfo)
    if (gc::lower(key) == "status") return gc::lower(value) != "standby";
  return true;
}

void add_remote_rulesets(gc::Console& con, Remote& r) {
  r.rulesets = std::make_unique<gc::Rulesets>(con, [&r] { return remote_playing(r); });
  r.rulesets->set_book(ge::classic::qw_rulesets());
  con.add_command("f_ruleset", "f_ruleset [check]: what the answers say, or ask everyone now",
                  [&r](gc::Console& c, const gc::Args& a) {
                    if (a.size() >= 2 && gc::lower(a[1]) == "check") {
                      if (!r.active) {
                        c.print("You are not connected to a server.");
                        return;
                      }
                      remote_command(r, "say f_ruleset");
                      return;
                    }
                    c.print(ge::classic::fair_help());
                  });
}

void remote_rulesets_frame(Remote& r, gc::Console& con) {
  if (!r.rulesets) return;
  const bool q2 = r.q2_game && r.q2_game();
  const auto want = q2 ? ge::classic::q2_rulesets() : ge::classic::qw_rulesets();
  if (r.rulesets->book().data() != want.data()) r.rulesets->set_book(want);
  r.rulesets->set_connected(r.active);
  r.rulesets->update();
  ge::classic::FairFacts& f = r.fair_facts;
  f.server = r.server_name;
  f.version = kProgramVersion;
  f.ruleset = std::string(r.rulesets->current() ? r.rulesets->current()->name : "default");
  f.modified = on(con, "ce_enabled") || on(con, "cr_enabled");
  f.scripts = on(con, "allow_scripts");
  f.forced_skins = on(con, "enemyforceskins");
  f.side_step = on(con, "cl_idrive");
  // The beam's smoothing is said to everyone when a player changes it in a match, as the leagues
  // expect: the others know how the player sees the lightning.
  if (const gc::Cvar* shaft = con.cvars().find("cl_fakeshaft"); shaft != nullptr) {
    if (shaft->value != r.fakeshaft_seen && !r.fakeshaft_seen.empty() && remote_playing(r))
      con.append("say \"" + ge::classic::fakeshaft_words(std::atof(shaft->value.c_str())) + "\"");
    r.fakeshaft_seen = shaft->value;
  }
}

void remote_fair_step(Remote& r, const std::vector<client::DemoPrint>& prints, double now_s) {
  if (!r.active || watching(r)) return;
  for (const client::DemoPrint& p : prints) {
    if (p.level != 3 || !ge::classic::fair_query(p.text, "f_ruleset")) continue;
    if (!r.fair.may_answer(now_s)) return;
    remote_command(r, "say \"" + ge::classic::ruleset_reply(r.fair_facts) + "\"");
    return;
  }
}

}  // namespace shooters_edge::net
