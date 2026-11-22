// Quake II's session settings (client/q2_session.hpp): the map commands run on a change, the game's
// clock and the read-only state cvars.
//
// ge:summary Tests Quake II's map commands, timescale, the state cvars and cl_thirdperson
// ge:keywords q2 cl_beginmapcmd cl_changemapcmd cl_disconnectcmd timescale fixedtime cl_paused
// ge:skill console shooter
#include "shooters_edge/client/q2_session.hpp"

#include <cmath>
#include <string>
#include <vector>

#include "brush/map.hpp"
#include "check.hpp"
#include "console/console.hpp"
#include "shooter/brush_hull.hpp"
#include "shooter_loop_bench.hpp"
#include "shooters_edge/server/q2_wire.hpp"

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

TEST(a_map_that_is_not_the_levels_is_told_by_its_checksum) {
  const std::vector<uint8_t> file = {'I', 'B', 'S', 'P', 38, 0, 0, 0, 1, 2, 3};
  const std::string ours = std::to_string(shooters_edge::server::q2_map_checksum(file));
  CHECK(cl::q2_map_version(ours, file, true).empty());
  CHECK(cl::q2_map_version("", file, true).empty());  // a level that states none
  CHECK(cl::q2_map_version("12345", file, true) ==
        "Local map version differs from server: " + ours + " != 12345");
  CHECK(cl::q2_map_version("12345", file, false) ==
        "Local map version differs from demo: " + ours + " != 12345");
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

TEST(cl_thirdperson_is_a_cheat_and_moves_the_view_back) {
  Rig r;
  r.c.execute("cl_thirdperson 1");  // refused while sv_cheats is 0
  CHECK(!r.s.third_person && r.s.third_range == 60 && r.s.third_angle == 0);
  ge::render::FrameView view;
  view.eye[0] = 1;
  view.heading = 0;  // looking down +z
  CHECK(cl::q2_third_person(r.s, view, nullptr) == 0 && view.eye[2] == 0);
  r.s.third_person = true;
  CHECK(cl::q2_third_person(r.s, view, nullptr) == 60);  // no world: the whole range
  CHECK(std::abs(view.eye[2] + 60 * ge::brush::kUnitM) < 1e-9 && view.eye[0] == 1);
  ge::render::FrameView front;
  CHECK(cl::view_behind(front, 40, 180, nullptr) == 40);  // in front, looking back
  CHECK(std::abs(front.eye[2] - 40 * ge::brush::kUnitM) < 1e-9);
  CHECK(std::abs(std::abs(front.heading) - 3.14159265358979) < 1e-9);
}

TEST(cl_thirdperson_stops_short_of_a_wall) {
  // A wall 30 units behind the eye: the camera stops 4 units off it.
  const ge::shooter::ModelData data =
      ge::shooter::build_brush_model({ge::test::loop::box({-100, -100, -100}, {100, -30, 100})});
  const ge::shooter::BrushModel world = data.model();
  ge::render::FrameView view;  // map +y is the engine's +z: looking down map +y
  const double went = cl::view_behind(view, 60, 0, &world);
  CHECK(std::abs(went - 26) < 0.1);  // the trace stops a hair short
}

// cl_maxfps is whole milliseconds apart: 120 is 8 ms, so 125, and the player is told once per
// change (how to turn it off the first time only); exact values and 0 say nothing.
TEST(an_inexact_cl_maxfps_is_told_rounded_once_per_change) {
  CHECK(cl::q2_fps_rounded(62) == 62 && cl::q2_fps_rounded(120) == 125);
  CHECK(cl::q2_fps_rounded(144) == 125 && cl::q2_fps_rounded(0) == 0);
  CHECK(cl::q2_fps_rounded(60) == 62 && cl::q2_fps_rounded(5) == 10);
  Rig r;
  r.c.cvars().add({.name = "cl_maxfps", .fallback = "62", .kind = con::CvarKind::integer});
  r.step(true, "base1");
  CHECK(r.c.output().empty());
  r.c.execute("cl_maxfps 120");
  r.step(true, "base1");
  CHECK(r.c.output().size() == 2);
  CHECK(r.c.output()[0] == "cl_maxfps value 120 is inexact and will be rounded to 125.");
  CHECK(r.c.output()[1] == "(Set cl_warn_on_fps_rounding to 0 to disable this warning.)");
  r.step(true, "base1");  // no change: nothing more
  r.c.execute("cl_maxfps 60");
  r.step(true, "base1");
  CHECK(r.c.output().size() == 3);  // told again, without the hint
  r.c.execute("cl_maxfps 100; cl_warn_on_fps_rounding 0");
  r.step(true, "base1");
  r.c.execute("cl_maxfps 90");
  r.step(true, "base1");
  CHECK(r.c.output().size() == 3);
}

TEST(in_grab_0_takes_the_mouse_only_while_the_game_covers_the_screen) {
  Rig r;
  CHECK(cl::q2_takes_mouse(r.s, false) && cl::q2_takes_mouse(r.s, true));  // 1, the default
  r.c.execute("in_grab 0");
  CHECK(!cl::q2_takes_mouse(r.s, false) && cl::q2_takes_mouse(r.s, true));
  r.c.execute("in_grab 2");
  CHECK(cl::q2_takes_mouse(r.s, false));
  r.c.execute("in_enable 0");  // the mouse left alone, whatever in_grab says
  CHECK(!cl::q2_takes_mouse(r.s, false) && !cl::q2_takes_mouse(r.s, true));
}

TEST(the_download_switches_are_quake_iis_settings) {
  // allow_download and its kinds, read at each connect (net/qw_remote.cpp): every kind fetched
  // but `others`; `allow_download -1` refuses even a typed download.
  Rig r;
  const ge::console::Cvar* all = r.c.cvars().find("allow_download");
  CHECK(all != nullptr && all->value == "1");
  for (const char* kind : {"maps", "players", "models", "sounds", "textures", "pics"}) {
    const ge::console::Cvar* v = r.c.cvars().find(std::string("allow_download_") + kind);
    CHECK_MSG(v != nullptr && v->value == "1", kind);
  }
  // ogg_remap_tracks: on once Quake II registers it (off for every other game: q2_session.hpp).
  CHECK(r.s.remap_tracks && r.c.cvars().find("ogg_remap_tracks")->value == "1");
  const ge::console::Cvar* others = r.c.cvars().find("allow_download_others");
  CHECK(others != nullptr && others->value == "0");
  r.c.execute("allow_download -1");
  CHECK(all->value == "-1");
  r.c.execute("allow_download -2");  // out of range: held to -1
  CHECK(all->value == "-1");
}
