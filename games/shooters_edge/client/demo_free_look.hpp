// FREE LOOK IN A DEMO: while a demo plays at its pace, holding SHIFT takes the mouse and turns
// the view away from the recorded one by the player's own mouse settings (`sensitivity` times
// `m_yaw` degrees a count sideways, times `m_pitch` up and down, `m_invert` flipping the second).
// The turn is kept as an offset on the recorded look, so the view still rides the player's eye
// and follows his own turns; the pitch it ends at is held within 89 degrees of level. Letting
// SHIFT go drops the offset: the view is the recorded one again.
//
// ge:summary Free look in a played demo: SHIFT held turns the view by the mouse
// ge:keywords demo free-look shift demo-look mouse-look offset playdemo watch
// ge:skill camera shooter
// ge:test tests/shooters_edge_demo_free_look_test.cpp
#pragma once

#include "render/view.hpp"
#include "shooter/controls.hpp"

namespace shooters_edge::client {

inline constexpr double kFreeLookMostPitch = 89;  // degrees above or below level

struct DemoFreeLook {
  bool held = false;  // SHIFT held while the demo plays
  double yaw = 0;     // degrees, the turn to the right
  double pitch = 0;   // degrees, the turn down (the classic sign)
};

// One frame: `held` whether SHIFT is held, `dx` and `dy` the mouse's counts this frame.
void free_look_step(DemoFreeLook& f, bool held, double dx, double dy,
                    const ge::shooter::Controls& c);

// The offset on `view` (a held look; nothing when not held), the pitch kept within
// kFreeLookMostPitch of level.
void free_look_apply(const DemoFreeLook& f, ge::render::FrameView& view);

}  // namespace shooters_edge::client
