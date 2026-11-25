// The window's gpustats and gpudebug (gpu_debug.hpp).
//
// ge:summary Shooter's Edge's gpudebug notes: where the player stands and looks, the look
// ge:keywords gpudebug notes add_gpudebug gpu_debug_notes
// ge:skill shooter render
// ge:test tests/shooters_edge_gpu_debug_test.cpp
#include "shooters_edge/client/gpu_debug.hpp"

#include <cmath>
#include <cstdio>

namespace shooters_edge::client {

std::vector<std::string> gpu_debug_notes(const GpuDebugSources& s) {
  std::vector<std::string> out;
  char line[256];
  // WHERE: the game and the map, the player (or the view watched), the line that draws it again.
  out.push_back("# where");
  // No world drawn yet (the title, a menu, a lobby): nothing to place.
  const bool drawn = s.view_from != nullptr && !s.view_from->place.empty();
  if (!drawn) out.push_back("no level drawn yet (the title, a menu or a match's lobby)");
  if (drawn) out.push_back("game and map: " + s.view_from->place);
  const PlaceSeen* seen = s.seen;
  const Listen* l = s.listen;
  if (!drawn) {
    // nothing more to say of the place
  } else if (seen != nullptr && seen->on) {
    std::snprintf(line, sizeof line,
                  "watching a recording: the view at (%.0f %.0f %.0f) units, angles pitch %.1f "
                  "yaw %.1f roll %.1f",
                  seen->at.x, seen->at.y, seen->at.z, seen->angles.x, seen->angles.y,
                  seen->angles.z);
    out.push_back(line);
  } else if (l != nullptr && l->server != nullptr && l->server->world.entities.size() > 1) {
    const auto& me = l->server->world.entities[1];
    std::snprintf(line, sizeof line,
                  "player at (%.1f %.1f %.1f) units (z up, the feet's box origin), view angles "
                  "pitch %.1f yaw %.1f roll %.1f (pitch down positive), moving %.0f units/s",
                  me.origin.x, me.origin.y, me.origin.z, me.view_angles.x, me.view_angles.y,
                  me.view_angles.z,
                  std::sqrt(me.velocity.x * me.velocity.x + me.velocity.y * me.velocity.y +
                            me.velocity.z * me.velocity.z));
    out.push_back(line);
  } else {
    out.push_back("no level played or watched");
  }
  if (drawn) {
    const auto& v = s.view_from->last;
    std::snprintf(line, sizeof line,
                  "eye at (%.2f %.2f %.2f) m (y up), heading %.1f, pitch %.1f degrees", v.eye[0],
                  v.eye[1], v.eye[2], v.heading * 180 / 3.14159265358979323846,
                  v.pitch * 180 / 3.14159265358979323846);
    out.push_back(line);
    out.push_back("draw it again: " + view_from_line(v, s.view_from->place));
  }
  // LOOK: the look and the window.
  out.push_back("# look");
  std::string look = "r_look " + (s.look != nullptr ? *s.look : std::string("?"));
  if (s.width != nullptr && s.height != nullptr)
    look += ", window " + std::to_string(*s.width) + "x" + std::to_string(*s.height) + " px";
  out.push_back(look);
  return out;
}

}  // namespace shooters_edge::client
