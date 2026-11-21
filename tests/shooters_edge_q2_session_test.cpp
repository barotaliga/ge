// Quake II's session settings (client/q2_session.hpp): the map commands run on a change, the game's
// clock and the read-only state cvars.
//
// ge:summary Tests Quake II's cl_beginmapcmd, cl_changemapcmd, cl_disconnectcmd, timescale, state
// ge:keywords q2 cl_beginmapcmd cl_changemapcmd cl_disconnectcmd timescale fixedtime cl_paused
// ge:skill console shooter
#include "shooters_edge/client/q2_session.hpp"

#include <string>
#include <vector>

#include "check.hpp"
#include "console/console.hpp"

namespace cl = shooters_edge::client;
namespace con = ge::console;

namespace {

// A console whose `said` command keeps what each map command ran.
struct Rig {
  con::Console c;
  cl::Q2Session s;
  std::vector<std::string> said;
  Rig() {
    cl::add_q2_session_cvars(c.cvars(), s);
    c.add_command("said", "keeps its word",
                  [this](con::Console&, const con::Args& a) { said.push_back(a[1]); });
    c.execute("cl_beginmapcmd \"said begin\"; cl_changemapcmd \"said change\"");
    c.execute("cl_disconnectcmd \"said gone\"");
  }
  std::string step(bool joined, const char* map, bool hosting = false, bool paused = false) {
    said.clear();
    cl::q2_session_step(c, s, joined, map, hosting, paused);
    c.run();
    std::string out;
    for (const std::string& w : said) out += (out.empty() ? "" : " ") + w;
    return out;
  }
  std::string value(const char* name) { return c.cvars().find(name)->value; }
};

}  // namespace

TEST(the_map_commands_run_as_the_game_moves_on) {
  Rig r;
  CHECK(r.step(false, "") == "");                  // the menu: nothing
  CHECK(r.step(true, "") == "");                   // joining, no map yet
  CHECK(r.step(true, "base1") == "begin");         // in the first map
  CHECK(r.step(true, "base1") == "");              // still there
  CHECK(r.step(true, "") == "change");             // leaving it, the next loading
  CHECK(r.step(true, "base2") == "begin");         // in the next
  CHECK(r.step(true, "base3") == "change begin");  // straight to another
  CHECK(r.step(false, "") == "gone");              // left the game
  CHECK(r.step(false, "") == "");
}

TEST(empty_map_commands_run_nothing) {
  Rig r;
  r.c.execute("cl_beginmapcmd \"\"");
  CHECK(r.step(true, "q2dm1") == "");
}

TEST(timescale_and_fixedtime_are_cheats_over_the_frame) {
  Rig r;
  CHECK(cl::q2_game_dt(r.s, 0, 0.02) == 0.02);
  r.c.execute("timescale 2; fixedtime 10");  // refused while sv_cheats is 0
  CHECK(r.s.timescale == 1 && r.s.fixed_ms == 0);
  r.s.timescale = 2;
  CHECK(cl::q2_game_dt(r.s, 0, 0.02) == 0.04);
  r.s.fixed_ms = 10;
  CHECK(cl::q2_game_dt(r.s, 0, 0.02) == 0.02);    // 10 ms, twice as fast
  CHECK(cl::q2_game_dt(r.s, 0.05, 0.02) == 0.1);  // host_framerate first
}

TEST(the_state_cvars_follow_the_game_and_refuse_the_player) {
  Rig r;
  CHECK(r.value("cl_running") == "1" && r.value("sv_running") == "0");
  r.step(true, "base1", true, false);
  CHECK(r.value("sv_running") == "1" && r.value("cl_paused") == "0");
  r.step(true, "base1", true, true);
  CHECK(r.value("cl_paused") == "1" && r.value("sv_paused") == "1");
  r.c.execute("cl_paused 0");
  CHECK(r.value("cl_paused") == "1");
  r.step(false, "");
  CHECK(r.value("sv_running") == "0" && r.value("sv_paused") == "0");
}

TEST(cl_autopause_is_on_by_default) {
  Rig r;
  CHECK(r.s.autopause);
  r.c.execute("cl_autopause 0");
  CHECK(!r.s.autopause);
}
