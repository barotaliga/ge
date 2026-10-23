// The devices to the axes (engine/play/devices.hpp): each axis paired with the action of its
// name, read by its kind, and a press reaching exactly one step whatever the frame grants.
//
// ge:summary Tests: bind_axes by name, axes by kind, a press latched for exactly one step
// ge:keywords test play devices axes press latch toggle lever
// ge:skill agent-interface input testing
#include "check.hpp"
#include "input/bindings_text.hpp"
#include "input/names.hpp"
#include "input/sim.hpp"
#include "play/devices.hpp"

namespace in = ge::input;
namespace pl = ge::play;

TEST(each_axis_reads_its_action_by_kind_and_a_press_reaches_one_step) {
  const pl::Axis axes[] = {{"steer", pl::AxisKind::signed_unit, ""},
                           {"tuck", pl::AxisKind::lever, ""},
                           {"jump", pl::AxisKind::toggle, ""},
                           {"reset", pl::AxisKind::press, ""},
                           {"fly", pl::AxisKind::toggle, ""}};
  in::ActionMap m;
  m.defs = {{"steer", in::ActionKind::axis, false, ""},
            {"tuck", in::ActionKind::axis, true, ""},
            {"jump", in::ActionKind::button, false, ""},
            {"reset", in::ActionKind::button, false, ""}};
  std::string error;
  CHECK(
      in::read_bindings("ge-bindings 1\ncontext ride\nbind steer key:d\nbind tuck key:w\n"
                        "bind jump key:space\nbind reset key:r\n",
                        m, error));
  std::vector<std::string> missing;
  pl::DeviceAxes d = pl::bind_axes(m, axes, &missing);
  CHECK(missing.size() == 1 && missing[0] == "fly");

  in::SimInput sim;
  in::InputState s;
  in::ActionValues v = in::make_values(m);
  const auto kb = in::sim_plug(sim, in::sim_keyboard());
  in::sim_key(sim, kb, in::key::d, true);
  in::sim_key(sim, kb, in::key::w, true);
  in::sim_key(sim, kb, in::key::space, true);
  in::sim_key(sim, kb, in::key::r, true);
  in::sim_key(sim, kb, in::key::r, false);
  in::sim_frame(sim, s);
  in::evaluate(m, s, 1.0 / 60, v);
  pl::axes_frame(d, v);
  double out[5];
  pl::axes_step(d, v, out);
  CHECK(out[0] == 1 && out[1] == 1 && out[2] == 1 && out[3] == 1 && out[4] == 0);
  pl::axes_step(d, v, out);  // the frame's second step: the press is taken
  CHECK(out[3] == 0 && out[2] == 1);
  // A press in a frame that grants no step waits for the next frame's first.
  in::sim_key(sim, kb, in::key::r, true);
  in::sim_frame(sim, s);
  in::evaluate(m, s, 1.0 / 60, v);
  pl::axes_frame(d, v);
  in::sim_frame(sim, s);
  in::evaluate(m, s, 1.0 / 60, v);
  pl::axes_frame(d, v);
  pl::axes_step(d, v, out);
  CHECK(out[3] == 1);
  pl::axes_step(d, v, out);
  CHECK(out[3] == 0);
}
