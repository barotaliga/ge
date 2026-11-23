// games/shooters_edge/client/gpu_debug.hpp: with no level the notes say so; the look and the
// window's size are named; the eye and the line that draws the same picture again come from the
// view last drawn.
//
// ge:summary Tests for Shooter's Edge's gpudebug notes
// ge:keywords test gpudebug notes viewfrom
// ge:skill testing shooter
#include <string>
#include <vector>

#include "check.hpp"
#include "shooters_edge/client/gpu_debug.hpp"

namespace cl = shooters_edge::client;

TEST(the_notes_name_the_place_the_look_and_how_to_draw_it_again) {
  cl::ViewFrom vf;
  vf.place = "q1 start";
  vf.last.eye[0] = 5.12, vf.last.eye[1] = -0.45, vf.last.eye[2] = 23.6;
  const std::string look = "modern";
  const int w = 1920, h = 1080;
  cl::GpuDebugSources s;
  s.view_from = &vf;
  s.look = &look;
  s.width = &w;
  s.height = &h;
  const std::vector<std::string> n = cl::gpu_debug_notes(s);
  const auto has = [&n](const std::string& part) {
    for (const std::string& l : n)
      if (l.find(part) != std::string::npos) return true;
    return false;
  };
  CHECK(!n.empty() && n[0] == "# where");
  CHECK(has("game and map: q1 start"));
  CHECK(has("no level played or watched"));  // a world drawn, no server or recording
  CHECK(has("eye at (5.12 -0.45 23.60) m"));
  CHECK(has("draw it again: r_viewfrom \"5.12 -0.45 23.60"));
  CHECK(has("# look"));
  CHECK(has("r_look modern, window 1920x1080 px"));
}

TEST(before_any_level_is_drawn_the_notes_say_so_and_place_nothing) {
  cl::ViewFrom vf;  // nothing drawn: no place
  cl::GpuDebugSources s;
  s.view_from = &vf;
  const std::vector<std::string> n = cl::gpu_debug_notes(s);
  bool said = false, placed = false;
  for (const std::string& l : n) {
    said |= l.find("no level drawn yet") != std::string::npos;
    placed |= l.find("eye at") != std::string::npos || l.find("draw it again") != std::string::npos;
  }
  CHECK(said);
  CHECK(!placed);
}
