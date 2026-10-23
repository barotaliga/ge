// THE ACTIONS: what a game asks for ("steer", "jump", "look") rather than which key or stick
// gives it, so a player can rebind every control and every device works at once. A game states
// its actions once (`ActionDef`: a name, a kind); an `ActionMap` binds sources to them in named
// CONTEXTS; `evaluate` reads the input state through the map once a frame into `ActionValues`.
//
// THE KINDS:
//   button  on or off, with the frame's edges (a key, a pad button, a trigger past its threshold)
//   axis    one value, -1 .. 1 (or 0 .. 1 when `lever`): a stick's axis through its shape, a
//           pedal, a key pushing it toward `value` through a ramp
//   stick   two values in the unit disc: a pad's stick through its stick shape, four keys
//   delta   two unbounded values summed over the frame: the mouse's motion, the gyro's turn, a
//           stick or a key turning at a rate (units a second), for a camera or a first-person look
//
// SEVERAL SOURCES ON ONE ACTION. Every binding of an action is read: the buttons are OR'd; on an
// axis or a stick the keys' pushes are SUMMED (left and right held together cancel) and ramped,
// the analog sources' strongest wins, and between the two the larger push wins; deltas add.
// So a keyboard and a pad both steer without a switch, and the last one touched is never the
// only one heard.
//
// CONTEXTS are bundles of bindings a game switches on and off ("ride", "menu", "console").
// They are read in the map's order, and a context marked `blocking` takes the sources it binds:
// a lower context does not see a key the console has bound while the console is open.
//
// A CHORD: a binding with `with` set reads only while that other source is held
// (`key:left_shift` with `key:w`).
//
// PER PLAYER: `evaluate` reads every device unless it is handed the player's devices
// (players.hpp), so two pads on one screen are two players. A binding's `device` names one
// model of device (a DeviceInfo key: a wheel's own buttons), or is empty for any of its kind.
//
// The text form of a map (the bindings file a player's rebinding is saved in) is
// bindings_text.hpp; rebinding a control by pressing it is rebind.hpp.
//
// ge:summary Actions and bindings: ActionDef, Source, Binding, contexts, evaluate, ActionValues
// ge:keywords input action actions binding bindings action-map context contexts blocking chord
// ge:keywords modifier source key mouse pad joystick hat gyro touch role wheel axis stick delta
// ge:keywords button rebind remap combine evaluate action-values mouse-look sensitivity
// ge:skill input
// ge:test tests/input_actions_test.cpp
#pragma once

#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "input/device_db.hpp"
#include "input/shape.hpp"
#include "input/state.hpp"
#include "input/touch.hpp"

namespace ge::input {

enum class ActionKind : uint8_t { button, axis, stick, delta };

struct ActionDef {
  std::string name;  // snake_case, as the bindings file spells it
  ActionKind kind = ActionKind::button;
  bool lever = false;  // an axis read 0 .. 1 (a throttle, a brake) rather than -1 .. 1
  std::string help;    // what it does, a phrase (a rebinding screen prints it)
};

enum class SourceKind : uint8_t {
  none,
  key,           // code: HID usage
  mouse_button,  // code: MouseButton
  mouse_motion,  // code: 0 x, 1 y (counts or pixels a frame)
  mouse_wheel,   // code: 0 x, 1 y (notches a frame)
  pad_button,    // code: PadButton
  pad_axis,      // code: PadAxis
  pad_stick,     // code: 0 left, 1 right (both axes)
  joy_button,    // code: index
  joy_axis,      // code: index
  joy_hat,       // code: index, dir: the Hat bit
  gyro,          // code: 0 x (pitch), 1 y (yaw), 2 z (roll), rad/s
  touch,         // name: the touch layout's control (touch.hpp)
  role,          // code: Role (device_db.hpp): a wheel's steer, a pedal, a throttle
};

struct Source {
  SourceKind kind = SourceKind::none;
  uint16_t code = 0;
  int8_t half = 0;   // an axis read as one half: +1 its positive half, -1 its negative (0: all)
  uint8_t dir = 0;   // a hat's direction bit
  std::string name;  // a touch control's name
  bool operator==(const Source& o) const {
    return kind == o.kind && code == o.code && half == o.half && dir == o.dir && name == o.name;
  }
};

struct Binding {
  uint16_t action = 0;  // index into the map's defs
  Source source;
  // A digital source on an axis: where it pushes (-1 left, 1 right); on a stick or a delta, it
  // pushes along `component`. An analog source: a gain (-1 inverts).
  float value = 1;
  uint8_t component = 0;  // a stick's or a delta's: 0 x, 1 y
  AxisShape shape;        // an analog axis source
  StickShape stick;       // a pad_stick source
  Threshold threshold;    // an analog source read as a button
  Ramp ramp;              // keys onto an axis or a stick (the action's ramp is its first key's)
  float rate = 0;         // a stick, key or axis onto a delta: units a second at full push
  Source with;            // a chord: read only while this is held (kind none: always)
  std::string device;     // a DeviceInfo key: only that model of device ("" any)
};

struct Context {
  std::string name;
  bool active = true;
  bool blocking = false;  // takes the sources it binds from the contexts after it
  std::vector<Binding> bindings;
};

struct ActionMap {
  std::vector<ActionDef> defs;
  std::vector<Context> contexts;  // read in this order

  int find_action(std::string_view name) const;  // -1 when there is none
  Context* find_context(std::string_view name);
  const Context* find_context(std::string_view name) const;
  // Turn a context on or off; false when there is none of that name.
  bool set_active(std::string_view context, bool on);
};

struct ActionValue {
  float x = 0, y = 0;  // an axis's value is x; a stick's and a delta's are both
  bool held = false, pressed = false, released = false;  // a button's (an axis's: |x| > 0.5)
  float key_x = 0, key_y = 0;  // the ramped push of the keys (the ramp's state)
  // The device that gave the value this frame (0 when none did, or the pooled keyboard/mouse).
  DeviceId device = 0;
};

struct ActionValues {
  std::vector<ActionValue> v;  // one per def
  const ActionValue& operator[](size_t i) const { return v[i]; }
};

// What `evaluate` reads besides the state: every part optional.
struct Sources {
  const DeviceDb* db = nullptr;         // for `role` sources
  const TouchLayout* layout = nullptr;  // for `touch` sources, with the frame read off it
  const TouchFrame* touch = nullptr;
  std::span<const DeviceId> only;      // read only these devices (empty: every device)
  bool keyboard = true, mouse = true;  // whether the pooled keyboard and mouse are read
};

// A binding of `source` to action `action` with the defaults its source wants: a wheel's or a
// pedal's role is calibrated by its device row, so it has no dead zone (0 .. 1); every other
// analog source has AxisShape's.
Binding make_binding(uint16_t action, const Source& source);

ActionValues make_values(const ActionMap& map);
// Read the state through the map over a frame of `dt_s` (the ramps and the rate deltas).
void evaluate(const ActionMap& map, const InputState& s, double dt_s, ActionValues& out,
              const Sources& from = {});

// One source's reading now, raw (before a binding's shape): a button 0 or 1, an axis its value
// (one half of it, positive, when `half` is set), the mouse's motion this frame. The strongest
// over every device the source may come from; `by` is that device.
float source_value(const Source& src, const InputState& s, const Sources& from,
                   const std::string& device, DeviceId* by = nullptr);
// Whether a button-like source went down this frame (a tap that is already up again counts).
bool source_tapped(const Source& src, const InputState& s, const Sources& from,
                   const std::string& device);
// Whether the source is a button (or a key, a hat, a touch button, a button role): on or off.
bool is_digital(const Source& src, const Sources& from);

}  // namespace ge::input
