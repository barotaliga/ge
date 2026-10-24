// The devices over SDL3: the subsystems' lifetime, the device table (SDL's ids to engine/input's),
// each SDL event translated to engine/input's, feedback played back, and SDL's virtual devices.
//
// ge:summary The OS's input devices over SDL3: translation, device table, feedback, virtual devices
// ge:keywords sdl gamepad joystick haptic sensor translate device-table virtual-joystick rumble
// ge:keywords force-feedback hot-plug finger touchpad battery
// ge:skill input platform
// ge:test tests/platform_devices_test.cpp
#include <algorithm>
#include <cmath>
#include <deque>
#include <string>
#include <vector>

#include "core/output.hpp"
#include "input/names.hpp"
#include "platform/devices_sdl.hpp"

namespace ge::platform {

namespace out = core::output;
namespace in = ge::input;

namespace {

// A virtual device's side of SDL's callbacks: what it was told, where SDL can write it.
struct VirtualSlot {
  uint32_t handle = 0;
  SDL_JoystickID sdl = 0;
  SDL_Joystick* joy = nullptr;  // opened to set its state
  in::DeviceKind kind = in::DeviceKind::gamepad;
  VirtualFeedback told;
};

struct Fingers {
  SDL_FingerID ids[16] = {};
  bool used[16] = {};
  // The small number a finger is reported as, kept from its touch down to its lift.
  int index(SDL_FingerID id, bool take) {
    for (int i = 0; i < 16; i++) {
      if (used[i] && ids[i] == id) return i;
    }
    if (!take) return -1;
    for (int i = 0; i < 16; i++) {
      if (!used[i]) {
        used[i] = true;
        ids[i] = id;
        return i;
      }
    }
    return -1;
  }
  void lift(int i) {
    if (i >= 0) used[i] = false;
  }
};

}  // namespace

struct Plugged {
  in::DeviceId id = 0;
  SDL_JoystickID sdl = 0;  // a joystick's or a gamepad's (0: a touch screen)
  SDL_TouchID touch = 0;
  SDL_Gamepad* pad = nullptr;
  SDL_Joystick* joy = nullptr;  // the pad's own, or the joystick
  SDL_Haptic* haptic = nullptr;
  int effect = -1;  // the one constant force, when the device has force feedback
  in::DeviceInfo info;
  Fingers fingers;
  // What was last sent, so a frame that changes nothing sends nothing.
  uint16_t low = 0, high = 0, left = 0, right = 0;
  double rumble_sent_s = -1, triggers_sent_s = -1;
  bool light = false;
  uint8_t r = 0, g = 0, b = 0;
  int player = -1;
  int16_t level = 0;
  bool running = false;
  double pos = 0, vel = 0, last_s = -1;
};

class Devices {
 public:
  DeviceOptions options;
  in::DeviceId next_id = 1;
  in::DeviceId keyboard = 0, mouse = 0;
  bool kbm_plugged = false;
  std::vector<Plugged> plugged;
  std::deque<VirtualSlot> virtuals;  // a deque: SDL holds pointers to its elements
  uint32_t next_virtual = 1;

  Plugged* by_sdl(SDL_JoystickID sdl) {
    for (Plugged& p : plugged) {
      if (p.sdl == sdl && sdl != 0) return &p;
    }
    return nullptr;
  }
  Plugged* by_touch(SDL_TouchID t) {
    for (Plugged& p : plugged) {
      if (p.sdl == 0 && p.touch == t) return &p;
    }
    return nullptr;
  }
  VirtualSlot* virt(uint32_t handle) {
    for (VirtualSlot& v : virtuals) {
      if (v.handle == handle) return &v;
    }
    return nullptr;
  }
};

namespace {

constexpr SDL_InitFlags kSubsystems =
    SDL_INIT_GAMEPAD | SDL_INIT_JOYSTICK | SDL_INIT_HAPTIC | SDL_INIT_SENSOR;

float unit(Sint16 v) { return std::max(-1.0f, static_cast<float>(v) / 32767.0f); }

std::string key_of(SDL_Joystick* j, uint16_t vendor, uint16_t product) {
  if (vendor != 0) return in::usb_key(vendor, product);
  char buf[33] = {};
  SDL_GUIDToString(SDL_GetJoystickGUID(j), buf, sizeof buf);
  return buf;
}

in::Family family_of(SDL_GamepadType t) {
  switch (t) {
    case SDL_GAMEPAD_TYPE_XBOX360:
    case SDL_GAMEPAD_TYPE_XBOXONE:
      return in::Family::xbox;
    case SDL_GAMEPAD_TYPE_PS3:
    case SDL_GAMEPAD_TYPE_PS4:
    case SDL_GAMEPAD_TYPE_PS5:
      return in::Family::playstation;
    case SDL_GAMEPAD_TYPE_NINTENDO_SWITCH_PRO:
    case SDL_GAMEPAD_TYPE_NINTENDO_SWITCH_JOYCON_LEFT:
    case SDL_GAMEPAD_TYPE_NINTENDO_SWITCH_JOYCON_RIGHT:
    case SDL_GAMEPAD_TYPE_NINTENDO_SWITCH_JOYCON_PAIR:
    case SDL_GAMEPAD_TYPE_GAMECUBE:
      return in::Family::nintendo;
    default:
      return in::Family::generic;
  }
}

in::DeviceKind kind_of(SDL_JoystickType t) {
  switch (t) {
    case SDL_JOYSTICK_TYPE_WHEEL:
      return in::DeviceKind::wheel;
    case SDL_JOYSTICK_TYPE_FLIGHT_STICK:
      return in::DeviceKind::flight_stick;
    case SDL_JOYSTICK_TYPE_THROTTLE:
      return in::DeviceKind::throttle;
    default:
      return in::DeviceKind::joystick;
  }
}

SDL_JoystickType sdl_type(in::DeviceKind k) {
  switch (k) {
    case in::DeviceKind::gamepad:
      return SDL_JOYSTICK_TYPE_GAMEPAD;
    case in::DeviceKind::wheel:
      return SDL_JOYSTICK_TYPE_WHEEL;
    case in::DeviceKind::flight_stick:
      return SDL_JOYSTICK_TYPE_FLIGHT_STICK;
    case in::DeviceKind::throttle:
      return SDL_JOYSTICK_TYPE_THROTTLE;
    default:
      return SDL_JOYSTICK_TYPE_UNKNOWN;
  }
}

uint32_t caps_of(SDL_PropertiesID props) {
  uint32_t f = 0;
  if (SDL_GetBooleanProperty(props, SDL_PROP_JOYSTICK_CAP_RUMBLE_BOOLEAN, false)) f |= in::kRumble;
  if (SDL_GetBooleanProperty(props, SDL_PROP_JOYSTICK_CAP_TRIGGER_RUMBLE_BOOLEAN, false)) {
    f |= in::kTriggerRumble;
  }
  if (SDL_GetBooleanProperty(props, SDL_PROP_JOYSTICK_CAP_RGB_LED_BOOLEAN, false)) f |= in::kLight;
  if (SDL_GetBooleanProperty(props, SDL_PROP_JOYSTICK_CAP_PLAYER_LED_BOOLEAN, false)) {
    f |= in::kPlayerLights;
  }
  return f;
}

bool on_battery(SDL_PowerState s) {
  return s == SDL_POWERSTATE_ON_BATTERY || s == SDL_POWERSTATE_CHARGING ||
         s == SDL_POWERSTATE_CHARGED;
}

// A joystick's force feedback: one constant force, the device's own autocenter off.
void open_haptic(const DeviceOptions& o, Plugged& p) {
  if (!o.haptics || !p.joy || !SDL_IsJoystickHaptic(p.joy)) return;
  p.haptic = SDL_OpenHapticFromJoystick(p.joy);
  if (!p.haptic) return;
  const uint32_t features = SDL_GetHapticFeatures(p.haptic);
  if (!(features & SDL_HAPTIC_CONSTANT)) {
    SDL_CloseHaptic(p.haptic);
    p.haptic = nullptr;
    return;
  }
  if (features & SDL_HAPTIC_AUTOCENTER) SDL_SetHapticAutocenter(p.haptic, 0);
  if (features & SDL_HAPTIC_GAIN) SDL_SetHapticGain(p.haptic, 100);
  SDL_HapticEffect eff{};
  eff.type = SDL_HAPTIC_CONSTANT;
  eff.constant.direction.type = SDL_HAPTIC_STEERING_AXIS;
  eff.constant.direction.dir[0] = 1;
  eff.constant.length = SDL_HAPTIC_INFINITY;
  p.effect = SDL_CreateHapticEffect(p.haptic, &eff);
  if (p.effect < 0) {
    out::warn(std::string("platform: '") + p.info.name +
              "' has force feedback but its constant force could not be made (" + SDL_GetError() +
              "); it plays rumble only; skill: input.");
    SDL_CloseHaptic(p.haptic);
    p.haptic = nullptr;
    return;
  }
  p.info.features |= in::kForce;
}

void plug_pad(Devices& d, SDL_JoystickID sdl, input::Queue& q, uint64_t t) {
  if (d.by_sdl(sdl)) return;
  SDL_Gamepad* pad = SDL_OpenGamepad(sdl);
  if (!pad) {
    out::warn(std::string("platform: a gamepad was plugged in but could not be opened (") +
              SDL_GetError() + "); unplug it and plug it in again; skill: input.");
    return;
  }
  Plugged p;
  p.id = d.next_id++;
  p.sdl = sdl;
  p.pad = pad;
  p.joy = SDL_GetGamepadJoystick(pad);
  in::DeviceInfo& i = p.info;
  i.id = p.id;
  i.kind = in::DeviceKind::gamepad;
  i.family = family_of(SDL_GetGamepadType(pad));
  i.name = SDL_GetGamepadName(pad) ? SDL_GetGamepadName(pad) : "gamepad";
  i.vendor = SDL_GetGamepadVendor(pad);
  i.product = SDL_GetGamepadProduct(pad);
  i.key = key_of(p.joy, i.vendor, i.product);
  i.features = caps_of(SDL_GetGamepadProperties(pad));
  if (SDL_GamepadHasSensor(pad, SDL_SENSOR_GYRO)) i.features |= in::kGyro;
  if (SDL_GamepadHasSensor(pad, SDL_SENSOR_ACCEL)) i.features |= in::kAccel;
  if (d.options.sensors) {
    if (i.features & in::kGyro) SDL_SetGamepadSensorEnabled(pad, SDL_SENSOR_GYRO, true);
    if (i.features & in::kAccel) SDL_SetGamepadSensorEnabled(pad, SDL_SENSOR_ACCEL, true);
  }
  i.touchpads = SDL_GetNumGamepadTouchpads(pad);
  if (i.touchpads > 0) i.features |= in::kTouchpad;
  int pct = -1;
  if (on_battery(SDL_GetGamepadPowerInfo(pad, &pct))) i.features |= in::kBattery;
  i.buttons = in::kPadButtons;
  i.axes = in::kPadAxes;
  i.player = SDL_GetGamepadPlayerIndex(pad);
  open_haptic(d.options, p);
  q.plug(t, p.info);
  d.plugged.push_back(p);
}

void plug_joystick(Devices& d, SDL_JoystickID sdl, input::Queue& q, uint64_t t) {
  if (d.by_sdl(sdl) || SDL_IsGamepad(sdl)) return;
  SDL_Joystick* j = SDL_OpenJoystick(sdl);
  if (!j) {
    out::warn(std::string("platform: a joystick was plugged in but could not be opened (") +
              SDL_GetError() + "); unplug it and plug it in again; skill: input.");
    return;
  }
  Plugged p;
  p.id = d.next_id++;
  p.sdl = sdl;
  p.joy = j;
  in::DeviceInfo& i = p.info;
  i.id = p.id;
  i.kind = kind_of(SDL_GetJoystickType(j));
  i.name = SDL_GetJoystickName(j) ? SDL_GetJoystickName(j) : "joystick";
  i.vendor = SDL_GetJoystickVendor(j);
  i.product = SDL_GetJoystickProduct(j);
  i.key = key_of(j, i.vendor, i.product);
  i.features = caps_of(SDL_GetJoystickProperties(j));
  int pct = -1;
  if (on_battery(SDL_GetJoystickPowerInfo(j, &pct))) i.features |= in::kBattery;
  i.buttons = std::min(SDL_GetNumJoystickButtons(j), in::kMaxButtons);
  i.axes = std::min(SDL_GetNumJoystickAxes(j), in::kMaxAxes);
  i.hats = std::min(SDL_GetNumJoystickHats(j), in::kMaxHats);
  if (SDL_GetNumJoystickButtons(j) > in::kMaxButtons || SDL_GetNumJoystickAxes(j) > in::kMaxAxes ||
      SDL_GetNumJoystickHats(j) > in::kMaxHats) {
    out::warn("platform: '" + i.name + "' reports more buttons, axes or hats than engine/input " +
              "keeps (kMaxButtons, kMaxAxes, kMaxHats in engine/input/device.hpp); the first " +
              "ones are read; skill: input.");
  }
  i.player = SDL_GetJoystickPlayerIndex(j);
  open_haptic(d.options, p);
  q.plug(t, p.info);
  d.plugged.push_back(p);
}

void unplug(Devices& d, SDL_JoystickID sdl, input::Queue& q, uint64_t t) {
  for (size_t k = 0; k < d.plugged.size(); k++) {
    Plugged& p = d.plugged[k];
    if (p.sdl != sdl || sdl == 0) continue;
    if (p.haptic) SDL_CloseHaptic(p.haptic);
    if (p.pad) {
      SDL_CloseGamepad(p.pad);
    } else if (p.joy) {
      SDL_CloseJoystick(p.joy);
    }
    q.unplug(t, p.id);
    for (VirtualSlot& v : d.virtuals) {
      if (v.sdl == sdl) v.told = {};
    }
    d.plugged.erase(d.plugged.begin() + static_cast<std::ptrdiff_t>(k));
    return;
  }
}

Plugged& touch_device(Devices& d, SDL_TouchID touch, input::Queue& q, uint64_t t) {
  if (Plugged* p = d.by_touch(touch)) return *p;
  Plugged p;
  p.id = d.next_id++;
  p.touch = touch;
  p.info.id = p.id;
  p.info.kind = in::DeviceKind::touch;
  const char* name = SDL_GetTouchDeviceName(touch);
  p.info.name = name ? name : "touch screen";
  p.info.key = "touch";
  q.plug(t, p.info);
  d.plugged.push_back(p);
  return d.plugged.back();
}

in::DeviceId id_of(Devices& d, SDL_JoystickID sdl) {
  const Plugged* p = d.by_sdl(sdl);
  return p ? p->id : 0;
}

void push_text(in::DeviceId kb, uint64_t t, const char* s, input::Queue& q) {
  const auto* u = reinterpret_cast<const unsigned char*>(s);
  while (u && *u) {
    uint32_t cp = 0;
    int more = 0;
    if (*u < 0x80) {
      cp = *u;
    } else if ((*u & 0xe0) == 0xc0) {
      cp = *u & 0x1fu;
      more = 1;
    } else if ((*u & 0xf0) == 0xe0) {
      cp = *u & 0x0fu;
      more = 2;
    } else {
      cp = *u & 0x07u;
      more = 3;
    }
    u++;
    for (int k = 0; k < more && *u; k++, u++) cp = (cp << 6) | (*u & 0x3fu);
    in::Event e{t, kb, in::EventType::text, 0, 0, {static_cast<float>(cp)}};
    q.push(e);
  }
}

in::TouchPhase phase_of(Uint32 type, Uint32 down, Uint32 up) {
  return type == down ? in::TouchPhase::down
         : type == up ? in::TouchPhase::up
                      : in::TouchPhase::move;
}

}  // namespace

void DevicesDeleter::operator()(Devices* d) const {
  if (!d) return;
  for (Plugged& p : d->plugged) {
    if (p.haptic) SDL_CloseHaptic(p.haptic);
    if (p.pad) {
      SDL_CloseGamepad(p.pad);
    } else if (p.joy) {
      SDL_CloseJoystick(p.joy);
    }
  }
  for (VirtualSlot& v : d->virtuals) {
    if (v.joy) SDL_CloseJoystick(v.joy);
    SDL_DetachVirtualJoystick(v.sdl);
  }
  delete d;
  SDL_QuitSubSystem(kSubsystems);
}

DevicesPtr open_devices(const DeviceOptions& options) {
  // Raw counts for a relative mouse (no OS acceleration on the deltas).
  SDL_SetHint(SDL_HINT_MOUSE_RELATIVE_SYSTEM_SCALE, "0");
  // The gamepads' events come whether or not a window has focus: a player alt-tabbed to a chat
  // still holds the pad, and a headless lab has no window at all.
  SDL_SetHint(SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS, "1");
  if (!SDL_InitSubSystem(kSubsystems)) {
    out::error(std::string("platform: the input devices' subsystems would not start (") +
               SDL_GetError() + "); the game runs with the keyboard and mouse only; skill: input.");
    return nullptr;
  }
  DevicesPtr d(new Devices);
  d->options = options;
  return d;
}

void devices_begin(Devices& d, input::Queue& q) {
  if (d.kbm_plugged || !d.options.keyboard) return;
  d.kbm_plugged = true;
  const uint64_t t = SDL_GetTicksNS();
  in::DeviceInfo kb;
  kb.id = d.keyboard = d.next_id++;
  kb.kind = in::DeviceKind::keyboard;
  kb.name = "keyboard";
  kb.key = "keyboard";
  q.plug(t, kb);
  in::DeviceInfo m;
  m.id = d.mouse = d.next_id++;
  m.kind = in::DeviceKind::mouse;
  m.name = "mouse";
  m.key = "mouse";
  m.features = in::kRawMotion;
  m.buttons = in::kMouseButtons;
  q.plug(t, m);
}

bool devices_event(Devices& d, const SDL_Event& e, input::Queue& q) {
  const uint64_t t = e.common.timestamp;
  switch (e.type) {
    case SDL_EVENT_KEY_DOWN:
    case SDL_EVENT_KEY_UP:
      if (!d.keyboard) return false;
      q.push(in::key_event(t, d.keyboard, static_cast<uint16_t>(e.key.scancode),
                           e.type == SDL_EVENT_KEY_DOWN, e.key.repeat));
      return true;
    case SDL_EVENT_TEXT_INPUT:
      if (!d.keyboard) return false;
      push_text(d.keyboard, t, e.text.text, q);
      return true;
    case SDL_EVENT_MOUSE_MOTION: {
      if (!d.mouse || e.motion.which == SDL_TOUCH_MOUSEID || e.motion.which == SDL_PEN_MOUSEID) {
        return false;
      }
      SDL_Window* w = SDL_GetWindowFromID(e.motion.windowID);
      const bool raw = w && SDL_GetWindowRelativeMouseMode(w);
      q.push(
          in::motion_event(t, d.mouse, e.motion.xrel, e.motion.yrel, e.motion.x, e.motion.y, raw));
      return true;
    }
    case SDL_EVENT_MOUSE_BUTTON_DOWN:
    case SDL_EVENT_MOUSE_BUTTON_UP:
      if (!d.mouse || e.button.which == SDL_TOUCH_MOUSEID || e.button.button < 1 ||
          e.button.button > in::kMouseButtons) {
        return false;
      }
      q.push(in::button_event(t, d.mouse, in::EventType::mouse_button,
                              static_cast<uint16_t>(e.button.button - 1),
                              e.type == SDL_EVENT_MOUSE_BUTTON_DOWN));
      return true;
    case SDL_EVENT_MOUSE_WHEEL: {
      if (!d.mouse || e.wheel.which == SDL_TOUCH_MOUSEID) return false;
      const float flip = e.wheel.direction == SDL_MOUSEWHEEL_FLIPPED ? -1.0f : 1.0f;
      in::Event w{t, d.mouse, in::EventType::mouse_wheel,
                  0, 0,       {flip * e.wheel.x, flip * e.wheel.y}};
      q.push(w);
      return true;
    }
    case SDL_EVENT_GAMEPAD_ADDED:
      plug_pad(d, e.gdevice.which, q, t);
      return true;
    case SDL_EVENT_JOYSTICK_ADDED:
      plug_joystick(d, e.jdevice.which, q, t);
      return true;
    case SDL_EVENT_GAMEPAD_REMOVED:
      unplug(d, e.gdevice.which, q, t);
      return true;
    case SDL_EVENT_JOYSTICK_REMOVED:
      unplug(d, e.jdevice.which, q, t);
      return true;
    case SDL_EVENT_GAMEPAD_BUTTON_DOWN:
    case SDL_EVENT_GAMEPAD_BUTTON_UP: {
      const in::DeviceId id = id_of(d, e.gbutton.which);
      if (!id || e.gbutton.button >= in::kPadButtons) return id != 0;
      q.push(in::button_event(t, id, in::EventType::pad_button, e.gbutton.button,
                              e.type == SDL_EVENT_GAMEPAD_BUTTON_DOWN));
      return true;
    }
    case SDL_EVENT_GAMEPAD_AXIS_MOTION: {
      const in::DeviceId id = id_of(d, e.gaxis.which);
      if (!id || e.gaxis.axis >= in::kPadAxes) return id != 0;
      q.push(in::axis_event(t, id, in::EventType::pad_axis, e.gaxis.axis, unit(e.gaxis.value)));
      return true;
    }
    case SDL_EVENT_GAMEPAD_SENSOR_UPDATE: {
      const in::DeviceId id = id_of(d, e.gsensor.which);
      if (!id) return false;
      const bool gyro = e.gsensor.sensor == SDL_SENSOR_GYRO;
      if (!gyro && e.gsensor.sensor != SDL_SENSOR_ACCEL) return true;
      in::Event s{t,
                  id,
                  in::EventType::sensor,
                  0,
                  static_cast<uint16_t>(gyro ? 0 : 1),
                  {e.gsensor.data[0], e.gsensor.data[1], e.gsensor.data[2]}};
      q.push(s);
      return true;
    }
    case SDL_EVENT_GAMEPAD_TOUCHPAD_DOWN:
    case SDL_EVENT_GAMEPAD_TOUCHPAD_MOTION:
    case SDL_EVENT_GAMEPAD_TOUCHPAD_UP: {
      const in::DeviceId id = id_of(d, e.gtouchpad.which);
      if (!id) return false;
      q.push(in::touch_event(
          t, id, in::EventType::pad_touch, static_cast<uint16_t>(e.gtouchpad.finger),
          phase_of(e.type, SDL_EVENT_GAMEPAD_TOUCHPAD_DOWN, SDL_EVENT_GAMEPAD_TOUCHPAD_UP),
          e.gtouchpad.x, e.gtouchpad.y, e.gtouchpad.pressure,
          static_cast<float>(e.gtouchpad.touchpad)));
      return true;
    }
    case SDL_EVENT_JOYSTICK_AXIS_MOTION: {
      const Plugged* p = d.by_sdl(e.jaxis.which);
      if (!p || p->pad || e.jaxis.axis >= in::kMaxAxes) return p != nullptr;
      q.push(in::axis_event(t, p->id, in::EventType::joy_axis, e.jaxis.axis, unit(e.jaxis.value)));
      return true;
    }
    case SDL_EVENT_JOYSTICK_BUTTON_DOWN:
    case SDL_EVENT_JOYSTICK_BUTTON_UP: {
      const Plugged* p = d.by_sdl(e.jbutton.which);
      if (!p || p->pad || e.jbutton.button >= in::kMaxButtons) return p != nullptr;
      q.push(in::button_event(t, p->id, in::EventType::joy_button, e.jbutton.button,
                              e.type == SDL_EVENT_JOYSTICK_BUTTON_DOWN));
      return true;
    }
    case SDL_EVENT_JOYSTICK_HAT_MOTION: {
      const Plugged* p = d.by_sdl(e.jhat.which);
      if (!p || p->pad || e.jhat.hat >= in::kMaxHats) return p != nullptr;
      // SDL's hat bits are engine/input's (up 1, right 2, down 4, left 8).
      q.push(in::axis_event(t, p->id, in::EventType::joy_hat, e.jhat.hat, e.jhat.value));
      return true;
    }
    case SDL_EVENT_JOYSTICK_BATTERY_UPDATED: {
      const Plugged* p = d.by_sdl(e.jbattery.which);
      if (!p) return false;
      const float charge =
          e.jbattery.percent < 0 ? -1.0f : static_cast<float>(e.jbattery.percent) / 100.0f;
      in::Event b{t, p->id, in::EventType::battery, 0, 0, {charge}};
      q.push(b);
      return true;
    }
    case SDL_EVENT_FINGER_DOWN:
    case SDL_EVENT_FINGER_MOTION:
    case SDL_EVENT_FINGER_UP: {
      Plugged& p = touch_device(d, e.tfinger.touchID, q, t);
      const bool down = e.type == SDL_EVENT_FINGER_DOWN;
      const int f = p.fingers.index(e.tfinger.fingerID, down);
      if (f < 0) return true;
      q.push(in::touch_event(t, p.id, in::EventType::touch, static_cast<uint16_t>(f),
                             phase_of(e.type, SDL_EVENT_FINGER_DOWN, SDL_EVENT_FINGER_UP),
                             e.tfinger.x, e.tfinger.y, e.tfinger.pressure));
      if (e.type == SDL_EVENT_FINGER_UP) p.fingers.lift(f);
      return true;
    }
    default:
      return false;
  }
}

void pump_devices(Devices& d, input::Queue& q) {
  devices_begin(d, q);
  SDL_UpdateJoysticks();
  SDL_Event e;
  while (SDL_PollEvent(&e)) devices_event(d, e, q);
}

int16_t haptic_level(double force) {
  const double f = std::max(-1.0, std::min(1.0, force));
  const auto level = static_cast<int>(std::lround(f * 32767.0));
  return static_cast<int16_t>(std::abs(level) <= 1 ? 0 : level);
}

void play_feedback(Devices& d, const input::Feedback& fb, const input::InputState& state,
                   double now_s, const input::DeviceDb* db) {
  for (Plugged& p : d.plugged) {
    if (!p.joy) continue;
    const in::DeviceFeedback* f = fb.find(p.id);
    float low = 0, high = 0;
    if (f) in::rumble_at(*f, now_s, low, high);
    const auto lo = static_cast<uint16_t>(std::lround(std::clamp(low, 0.0f, 1.0f) * 65535));
    const auto hi = static_cast<uint16_t>(std::lround(std::clamp(high, 0.0f, 1.0f) * 65535));
    // SDL's rumble lasts the duration it is given: a held rumble is resent before it runs out.
    const bool stale = (lo || hi) && now_s - p.rumble_sent_s > 0.5;
    if ((p.info.features & in::kRumble) && (lo != p.low || hi != p.high || stale)) {
      SDL_RumbleJoystick(p.joy, lo, hi, 1000);
      p.low = lo;
      p.high = hi;
      p.rumble_sent_s = now_s;
    }
    const bool trig_live = f && (f->triggers_until_s == 0 || now_s < f->triggers_until_s);
    const auto tl =
        static_cast<uint16_t>(trig_live ? std::lround(std::clamp(f->left, 0.0f, 1.0f) * 65535) : 0);
    const auto tr = static_cast<uint16_t>(
        trig_live ? std::lround(std::clamp(f->right, 0.0f, 1.0f) * 65535) : 0);
    const bool trig_stale = (tl || tr) && now_s - p.triggers_sent_s > 0.5;
    if ((p.info.features & in::kTriggerRumble) && (tl != p.left || tr != p.right || trig_stale)) {
      SDL_RumbleJoystickTriggers(p.joy, tl, tr, 1000);
      p.left = tl;
      p.right = tr;
      p.triggers_sent_s = now_s;
    }
    if (f && f->light_set && (p.info.features & in::kLight) &&
        (!p.light || f->r != p.r || f->g != p.g || f->b != p.b)) {
      SDL_SetJoystickLED(p.joy, f->r, f->g, f->b);
      p.light = true;
      p.r = f->r;
      p.g = f->g;
      p.b = f->b;
    }
    if (f && f->player >= 0 && f->player != p.player) {
      SDL_SetJoystickPlayerIndex(p.joy, f->player);
      p.player = f->player;
    }
    if (!p.haptic || p.effect < 0) continue;
    // The wheel's position, speed and acceleration from its steering axis, frame to frame.
    const in::Slot* slot = in::find_slot(state, p.id);
    double pos = 0;
    if (slot) {
      const in::DeviceRow row = in::row_for(db, p.info);
      bool found = false;
      pos = in::role_value(row, *slot, in::Role::steer, &found);
      if (!found) pos = slot->axes[0];
    }
    double vel = 0, acc = 0;
    if (p.last_s >= 0 && now_s > p.last_s) {
      vel = (pos - p.pos) / (now_s - p.last_s);
      acc = (vel - p.vel) / (now_s - p.last_s);
    }
    p.pos = pos;
    p.vel = vel;
    p.last_s = now_s;
    const int16_t level = haptic_level(f ? in::force_at(*f, now_s, pos, vel, acc) : 0.0);
    if (level != p.level || !p.running) {
      SDL_HapticEffect eff{};
      eff.type = SDL_HAPTIC_CONSTANT;
      eff.constant.direction.type = SDL_HAPTIC_STEERING_AXIS;
      eff.constant.direction.dir[0] = 1;
      eff.constant.length = SDL_HAPTIC_INFINITY;
      eff.constant.level = level;
      SDL_UpdateHapticEffect(p.haptic, p.effect, &eff);
      if (!p.running) p.running = SDL_RunHapticEffect(p.haptic, p.effect, 1);
      p.level = level;
    }
  }
}

// --- Virtual devices ---

namespace {

bool SDLCALL v_rumble(void* u, Uint16 low, Uint16 high) {
  auto* v = static_cast<VirtualSlot*>(u);
  v->told.low = low;
  v->told.high = high;
  v->told.rumbles++;
  return true;
}
bool SDLCALL v_triggers(void* u, Uint16 left, Uint16 right) {
  auto* v = static_cast<VirtualSlot*>(u);
  v->told.left = left;
  v->told.right = right;
  return true;
}
bool SDLCALL v_led(void* u, Uint8 r, Uint8 g, Uint8 b) {
  auto* v = static_cast<VirtualSlot*>(u);
  v->told.r = r;
  v->told.g = g;
  v->told.b = b;
  v->told.light = true;
  return true;
}
void SDLCALL v_player(void* u, int player) { static_cast<VirtualSlot*>(u)->told.player = player; }
bool SDLCALL v_sensors(void*, bool) { return true; }

}  // namespace

uint32_t attach_virtual(Devices& d, const VirtualDesc& desc) {
  VirtualSlot& slot = d.virtuals.emplace_back();
  slot.handle = d.next_virtual++;
  slot.kind = desc.kind;
  const bool pad = desc.kind == in::DeviceKind::gamepad;
  SDL_VirtualJoystickDesc v;
  SDL_INIT_INTERFACE(&v);
  v.type = static_cast<Uint16>(sdl_type(desc.kind));
  v.vendor_id = desc.vendor;
  v.product_id = desc.product;
  v.naxes = static_cast<Uint16>(pad ? SDL_GAMEPAD_AXIS_COUNT : desc.axes);
  v.nbuttons = static_cast<Uint16>(pad ? in::kPadButtons : desc.buttons);
  v.nhats = static_cast<Uint16>(pad ? 0 : desc.hats);
  SDL_VirtualJoystickSensorDesc sensors[2] = {{SDL_SENSOR_GYRO, 250}, {SDL_SENSOR_ACCEL, 250}};
  SDL_VirtualJoystickTouchpadDesc pads[2] = {{1, {}}, {1, {}}};
  if (pad && desc.gyro) {
    v.nsensors = 2;
    v.sensors = sensors;
  }
  if (pad && desc.touchpads > 0) {
    v.ntouchpads = static_cast<Uint16>(std::min(desc.touchpads, 2));
    v.touchpads = pads;
  }
  if (pad) {
    v.button_mask = (1u << in::kPadButtons) - 1u;
    v.axis_mask = (1u << SDL_GAMEPAD_AXIS_COUNT) - 1u;
  }
  v.name = desc.name.c_str();
  v.userdata = &slot;
  v.Rumble = v_rumble;
  v.RumbleTriggers = v_triggers;
  v.SetLED = v_led;
  v.SetPlayerIndex = v_player;
  v.SetSensorsEnabled = v_sensors;
  slot.sdl = SDL_AttachVirtualJoystick(&v);
  if (!slot.sdl) {
    out::error(std::string("platform: the virtual device '") + desc.name +
               "' could not be attached (" + SDL_GetError() + "); skill: input.");
    d.virtuals.pop_back();
    return 0;
  }
  slot.joy = SDL_OpenJoystick(slot.sdl);
  return slot.handle;
}

void detach_virtual(Devices& d, uint32_t handle) {
  VirtualSlot* v = d.virt(handle);
  if (!v || !v->joy) return;
  SDL_CloseJoystick(v->joy);
  v->joy = nullptr;
  SDL_DetachVirtualJoystick(v->sdl);
}

void virtual_axis(Devices& d, uint32_t handle, int axis, float value) {
  VirtualSlot* v = d.virt(handle);
  if (!v || !v->joy) return;
  // A gamepad's trigger reads 0 .. 1 off the axis's whole range.
  const bool trigger = v->kind == in::DeviceKind::gamepad && axis >= SDL_GAMEPAD_AXIS_LEFT_TRIGGER;
  const double x = trigger ? value * 65535.0 - 32768.0 : value * 32767.0;
  SDL_SetJoystickVirtualAxis(v->joy, axis,
                             static_cast<Sint16>(std::lround(std::clamp(x, -32768.0, 32767.0))));
}

void virtual_button(Devices& d, uint32_t handle, int button, bool down) {
  if (VirtualSlot* v = d.virt(handle); v && v->joy)
    SDL_SetJoystickVirtualButton(v->joy, button, down);
}

void virtual_hat(Devices& d, uint32_t handle, int hat, uint8_t bits) {
  if (VirtualSlot* v = d.virt(handle); v && v->joy) SDL_SetJoystickVirtualHat(v->joy, hat, bits);
}

void virtual_sensor(Devices& d, uint32_t handle, bool gyro, float x, float y, float z) {
  VirtualSlot* v = d.virt(handle);
  if (!v || !v->joy) return;
  const float data[3] = {x, y, z};
  SDL_SendJoystickVirtualSensorData(v->joy, gyro ? SDL_SENSOR_GYRO : SDL_SENSOR_ACCEL,
                                    SDL_GetTicksNS(), data, 3);
}

void virtual_touch(Devices& d, uint32_t handle, int touchpad, bool down, float x, float y) {
  if (VirtualSlot* v = d.virt(handle); v && v->joy) {
    SDL_SetJoystickVirtualTouchpad(v->joy, touchpad, 0, down, x, y, down ? 1.0f : 0.0f);
  }
}

namespace {

SDL_WindowID first_window() {
  int n = 0;
  SDL_Window** ws = SDL_GetWindows(&n);
  const SDL_WindowID id = ws && n > 0 ? SDL_GetWindowID(ws[0]) : 0;
  SDL_free(reinterpret_cast<void*>(ws));
  return id;
}

}  // namespace

void virtual_key(Devices&, uint16_t usage, bool down) {
  SDL_Event e{};
  e.type = down ? SDL_EVENT_KEY_DOWN : SDL_EVENT_KEY_UP;
  e.key.timestamp = SDL_GetTicksNS();
  e.key.windowID = first_window();
  e.key.scancode = static_cast<SDL_Scancode>(usage);
  e.key.down = down;
  SDL_PushEvent(&e);
}

void virtual_mouse_motion(Devices&, float dx, float dy) {
  SDL_Event e{};
  e.type = SDL_EVENT_MOUSE_MOTION;
  e.motion.timestamp = SDL_GetTicksNS();
  e.motion.windowID = first_window();
  e.motion.which = 1;
  e.motion.xrel = dx;
  e.motion.yrel = dy;
  SDL_PushEvent(&e);
}

VirtualFeedback virtual_feedback(const Devices& d, uint32_t handle) {
  for (const VirtualSlot& v : d.virtuals) {
    if (v.handle == handle) return v.told;
  }
  return {};
}

input::DeviceId virtual_id(const Devices& d, uint32_t handle) {
  for (const VirtualSlot& v : d.virtuals) {
    if (v.handle != handle) continue;
    for (const Plugged& p : d.plugged) {
      if (p.sdl == v.sdl) return p.id;
    }
  }
  return 0;
}

}  // namespace ge::platform
