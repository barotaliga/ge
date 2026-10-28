// Fall Line's touch layout.
//
// ge:summary touch_layout: the edge thumb, the tuck lever, the flick, the tap-hold, two buttons
// ge:keywords fall-line touch thumbs touch_layout
// ge:skill input
// ge:test tests/fall_line_controls_test.cpp
#include "fall_line/controls/thumbs.hpp"

#include "skier/tuning.hpp"

namespace fall_line::controls {

namespace in = ge::input;

in::TouchLayout touch_layout(float aspect) {
  in::TouchLayout l;
  l.aspect = aspect;
  in::TouchControl edge{"edge", in::TouchKind::stick, 0, kTopBand, 0.5f, 1, kEdgeReach};
  edge.trail = true;
  in::TouchControl lever{"lever", in::TouchKind::lever, 0.5f, kTopBand, 1, 1, kLeverReach, 1};
  lever.floor = -1;
  lever.dead = kLeverDead;
  // The flick holds the jump as a key held from 0.15 to 0.75 of the full load.
  constexpr auto full = static_cast<float>(ge::skier::tuning::jump::kPopFull);
  in::TouchControl flick{"flick", in::TouchKind::flick};
  flick.of = "lever";
  flick.gesture.hold_min_s = 0.15f * full;
  flick.gesture.hold_max_s = 0.75f * full;
  in::TouchControl load{"load", in::TouchKind::tap_hold};
  load.of = "lever";
  const in::TouchControl reset{"reset", in::TouchKind::button, 0, 0, 0.12f, kTopBand};
  const in::TouchControl camera{"camera", in::TouchKind::button, 0.88f, 0, 1, kTopBand};
  l.controls = {edge, lever, flick, load, reset, camera};
  return l;
}

}  // namespace fall_line::controls
