// The window's view tools (client/view_from.hpp): `viewfrom` prints the view as the r_viewfrom line
// that draws it again, and r_viewfrom puts that place in the view.
//
// ge:summary Tests: viewfrom's line, and r_viewfrom drawing the world from it
// ge:keywords test viewfrom r_viewfrom view-from camera
// ge:skill shooter testing
#include <cmath>
#include <string>

#include "check.hpp"
#include "shooters_edge/client/view_from.hpp"

namespace cl = shooters_edge::client;

TEST(viewfrom_prints_the_line_r_viewfrom_draws_again) {
  ge::console::Console con;
  cl::ViewFrom v;
  cl::add_view_from(con, v);
  ge::render::FrameView view;
  view.eye[0] = 5.12, view.eye[1] = -0.45, view.eye[2] = 23.6;
  view.heading = 0.5, view.pitch = -0.25;
  cl::apply_view_from(v, view, "q1 start");  // r_viewfrom unset: the eye as it was
  CHECK(view.eye[0] == 5.12 && view.heading == 0.5);
  con.execute("viewfrom");
  con.run();
  const std::string line = con.output().back();
  CHECK(line.starts_with("r_viewfrom \"5.12 -0.45 23.60 28.6 -14.3\"") &&
        line.ends_with("// q1 start"));
  // The line pasted: the world drawn from there, whatever the player's eye.
  con.execute(line);
  con.run();
  ge::render::FrameView other;
  cl::apply_view_from(v, other, "q1 start");
  CHECK(std::fabs(other.eye[2] - 23.6) < 1e-9 && std::fabs(other.heading - 0.5) < 1e-3 &&
        std::fabs(other.pitch + 0.25) < 1e-3);
  con.execute("r_viewfrom \"\"");
  con.run();
  ge::render::FrameView mine;
  mine.eye[0] = 1;
  cl::apply_view_from(v, mine, "q1 start");
  CHECK(mine.eye[0] == 1);
}
