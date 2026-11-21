// The window's view tools (view_from.hpp).
//
// ge:summary The window's view tools: viewfrom prints where you look from, r_viewfrom draws it
// ge:keywords viewfrom r_viewfrom view-from camera picture screenshot spot repro debug-view
// ge:skill shooter render
// ge:test tests/shooters_edge_view_from_test.cpp
#include "shooters_edge/client/view_from.hpp"

#include <cstdio>
#include <utility>

namespace shooters_edge::client {

namespace {

constexpr double kDegree = 3.14159265358979323846 / 180;

}  // namespace

void add_view_from(ge::console::Console& con, ViewFrom& v) {
  con.cvars().bind({.name = "r_viewfrom",
                    .help = "The world drawn from this place instead of the player's eye.",
                    .kind = ge::console::CvarKind::text,
                    .values = "\"\": the player's eye (the default); X Y Z HEADING PITCH: metres "
                              "(y up) and degrees, the place a picture is taken from"},
                   &v.asked);
  con.add_command("viewfrom",
                  "viewfrom: the place you look from, as an r_viewfrom line (the game and the map "
                  "after it)",
                  [&v](ge::console::Console& c, const ge::console::Args&) {
                    c.print(view_from_line(v.last, v.place));
                  });
}

void apply_view_from(ViewFrom& v, ge::render::FrameView& view, std::string place) {
  double x = 0, y = 0, z = 0, heading = 0, pitch = 0;
  if (!v.asked.empty() &&
      std::sscanf(v.asked.c_str(), "%lf %lf %lf %lf %lf", &x, &y, &z, &heading, &pitch) == 5) {
    view.eye[0] = x, view.eye[1] = y, view.eye[2] = z;
    view.heading = heading * kDegree;
    view.pitch = pitch * kDegree;
  }
  v.last = view;
  v.place = std::move(place);
}

std::string view_from_line(const ge::render::FrameView& view, const std::string& place) {
  char line[160];
  std::snprintf(line, sizeof line, "r_viewfrom \"%.2f %.2f %.2f %.1f %.1f\"", view.eye[0],
                view.eye[1], view.eye[2], view.heading / kDegree, view.pitch / kDegree);
  return std::string(line) + " // " + place;
}

}  // namespace shooters_edge::client
