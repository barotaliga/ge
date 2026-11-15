// The top-right corner's frame rate and readouts (corners.hpp).
//
// ge:summary The corner's readings from the player's game and the drawing of both
// ge:keywords corner_readings draw_corners show_speed scr_clock r_netgraph
// ge:skill ui
// ge:test tests/hud_corner_readouts_test.cpp
#include "shooters_edge/client/corners.hpp"

#include <cmath>
#include <ctime>

#include "hud/frame_graph.hpp"
#include "shooters_edge/hud/layout.hpp"

namespace shooters_edge::client {

ge::hud::Readings corner_readings(const Listen* l, double fallback_s) {
  ge::hud::Readings r;
  r.game_s = fallback_s;
  if (l != nullptr && l->server != nullptr) {
    const PlayerState me = player_state(*l);
    const double vx = me.velocity.x, vy = me.velocity.y, vz = me.velocity.z;
    r.ground_speed = std::sqrt(vx * vx + vy * vy);  // a readout, not the simulation
    r.speed = std::sqrt(vx * vx + vy * vy + vz * vz);
    r.game_s = l->server->sv.time - l->entered_at;
  }
  const std::time_t now = std::time(nullptr);
  std::tm local{};
#if defined(_WIN32)
  localtime_s(&local, &now);
#else
  localtime_r(&now, &local);
#endif
  r.hour = local.tm_hour;
  r.minute = local.tm_min;
  return r;
}

void draw_corners(ge::ui::DrawList& dl, int w, int h, const ge::platform::Pacing& pacing,
                  const ge::hud::ReadoutSettings& s, const ge::hud::Readings& r,
                  const ge::hud::NetHistory* net) {
  const ge::hud::Style style = hud::hud_style();
  const ge::hud::Frame frame = ge::hud::frame_for(w, h, style);
  const ge::ui::Rect fps =
      ge::hud::draw_frame_rate(dl, frame, style, pacing.times, pacing.refresh, pacing.show);
  const float top = fps.h > 0 ? fps.bottom() - frame.safe.y : 0;
  const float taken = ge::hud::draw_corner_readouts(dl, frame, style, s, r, top);
  if (net == nullptr) return;
  // The readouts' plates end a gap above where `taken` stops; the graph starts a gap below them.
  const float gap = frame.px(4);
  const float below = taken > 0 ? (top > 0 ? top + gap : 0) + taken - gap : top;
  ge::hud::draw_net_graph(dl, frame, style, *net, s.net, below);
}

}  // namespace shooters_edge::client
