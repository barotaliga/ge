// The names' tables: the HID key names, the pad's buttons and axes, the mouse's buttons, the
// kinds, the families and their face-button labels, the event types; and the USB key.
//
// ge:summary The name tables (HID keys, pad controls, kinds, families, labels) and lookups
// ge:keywords input names hid key-table labels lookup usb_key
// ge:skill input
// ge:test tests/input_names_test.cpp
#include "input/names.hpp"

#include <array>
#include <cstdio>
#include <utility>

namespace ge::input {

namespace {

constexpr std::pair<uint16_t, std::string_view> kKeyNames[] = {
    {4, "a"},
    {5, "b"},
    {6, "c"},
    {7, "d"},
    {8, "e"},
    {9, "f"},
    {10, "g"},
    {11, "h"},
    {12, "i"},
    {13, "j"},
    {14, "k"},
    {15, "l"},
    {16, "m"},
    {17, "n"},
    {18, "o"},
    {19, "p"},
    {20, "q"},
    {21, "r"},
    {22, "s"},
    {23, "t"},
    {24, "u"},
    {25, "v"},
    {26, "w"},
    {27, "x"},
    {28, "y"},
    {29, "z"},
    {30, "1"},
    {31, "2"},
    {32, "3"},
    {33, "4"},
    {34, "5"},
    {35, "6"},
    {36, "7"},
    {37, "8"},
    {38, "9"},
    {39, "0"},
    {40, "enter"},
    {41, "escape"},
    {42, "backspace"},
    {43, "tab"},
    {44, "space"},
    {45, "minus"},
    {46, "equals"},
    {47, "left_bracket"},
    {48, "right_bracket"},
    {49, "backslash"},
    {50, "non_us_hash"},
    {51, "semicolon"},
    {52, "apostrophe"},
    {53, "grave"},
    {54, "comma"},
    {55, "period"},
    {56, "slash"},
    {57, "caps_lock"},
    {58, "f1"},
    {59, "f2"},
    {60, "f3"},
    {61, "f4"},
    {62, "f5"},
    {63, "f6"},
    {64, "f7"},
    {65, "f8"},
    {66, "f9"},
    {67, "f10"},
    {68, "f11"},
    {69, "f12"},
    {70, "print_screen"},
    {71, "scroll_lock"},
    {72, "pause"},
    {73, "insert"},
    {74, "home"},
    {75, "page_up"},
    {76, "delete"},
    {77, "end"},
    {78, "page_down"},
    {79, "right"},
    {80, "left"},
    {81, "down"},
    {82, "up"},
    {83, "num_lock"},
    {84, "kp_divide"},
    {85, "kp_multiply"},
    {86, "kp_minus"},
    {87, "kp_plus"},
    {88, "kp_enter"},
    {89, "kp_1"},
    {90, "kp_2"},
    {91, "kp_3"},
    {92, "kp_4"},
    {93, "kp_5"},
    {94, "kp_6"},
    {95, "kp_7"},
    {96, "kp_8"},
    {97, "kp_9"},
    {98, "kp_0"},
    {99, "kp_period"},
    {100, "non_us_backslash"},
    {101, "application"},
    {102, "power"},
    {103, "kp_equals"},
    {104, "f13"},
    {105, "f14"},
    {106, "f15"},
    {107, "f16"},
    {108, "f17"},
    {109, "f18"},
    {110, "f19"},
    {111, "f20"},
    {112, "f21"},
    {113, "f22"},
    {114, "f23"},
    {115, "f24"},
    {116, "execute"},
    {117, "help"},
    {118, "menu"},
    {119, "select"},
    {120, "stop"},
    {121, "again"},
    {122, "undo"},
    {123, "cut"},
    {124, "copy"},
    {125, "paste"},
    {126, "find"},
    {127, "mute"},
    {128, "volume_up"},
    {129, "volume_down"},
    {133, "kp_comma"},
    {224, "left_ctrl"},
    {225, "left_shift"},
    {226, "left_alt"},
    {227, "left_gui"},
    {228, "right_ctrl"},
    {229, "right_shift"},
    {230, "right_alt"},
    {231, "right_gui"},
};

constexpr std::array<std::string_view, kPadButtons> kPadButtonNames = {
    "south",          "east",          "west",         "north",         "back",
    "guide",          "start",         "left_stick",   "right_stick",   "left_shoulder",
    "right_shoulder", "dpad_up",       "dpad_down",    "dpad_left",     "dpad_right",
    "misc",           "right_paddle1", "left_paddle1", "right_paddle2", "left_paddle2",
    "touchpad"};
constexpr std::array<std::string_view, kPadAxes> kPadAxisNames = {
    "left_x", "left_y", "right_x", "right_y", "left_trigger", "right_trigger"};
constexpr std::array<std::string_view, kMouseButtons> kMouseButtonNames = {
    "left", "middle", "right", "back", "forward"};
constexpr std::array<std::string_view, kDeviceKinds> kKindNames = {
    "none",  "keyboard",     "mouse",    "gamepad", "joystick",
    "wheel", "flight_stick", "throttle", "pedals",  "touch"};
constexpr std::array<std::string_view, 4> kFamilyNames = {"generic", "xbox", "playstation",
                                                          "nintendo"};
constexpr std::array<std::string_view, kEventTypes> kEventNames = {
    "added",  "removed",    "key",       "text",       "motion",   "mouse_button",
    "wheel",  "pad_button", "pad_axis",  "joy_button", "joy_axis", "joy_hat",
    "sensor", "touch",      "pad_touch", "battery"};

// The four face buttons' labels, south east west north, by family.
constexpr std::array<std::array<std::string_view, 4>, 4> kFaceLabels = {{
    {"A", "B", "X", "Y"},
    {"A", "B", "X", "Y"},
    {"cross", "circle", "square", "triangle"},
    {"B", "A", "Y", "X"},
}};

template <size_t N>
std::optional<size_t> index_of(const std::array<std::string_view, N>& table,
                               std::string_view name) {
  for (size_t i = 0; i < N; i++) {
    if (table[i] == name) return i;
  }
  return std::nullopt;
}

}  // namespace

std::string key_name(uint16_t usage) {
  for (const auto& [u, n] : kKeyNames) {
    if (u == usage) return std::string(n);
  }
  return "key" + std::to_string(usage);
}

std::optional<uint16_t> key_from_name(std::string_view name) {
  for (const auto& [u, n] : kKeyNames) {
    if (n == name) return u;
  }
  if (name.size() > 3 && name.substr(0, 3) == "key") {
    unsigned v = 0;
    for (const char c : name.substr(3)) {
      if (c < '0' || c > '9') return std::nullopt;
      v = v * 10 + static_cast<unsigned>(c - '0');
      if (v >= 512) return std::nullopt;
    }
    return static_cast<uint16_t>(v);
  }
  return std::nullopt;
}

std::string_view pad_button_name(PadButton b) { return kPadButtonNames[static_cast<size_t>(b)]; }
std::optional<PadButton> pad_button_from_name(std::string_view name) {
  const auto i = index_of(kPadButtonNames, name);
  return i ? std::optional(static_cast<PadButton>(*i)) : std::nullopt;
}
std::string_view pad_axis_name(PadAxis a) { return kPadAxisNames[static_cast<size_t>(a)]; }
std::optional<PadAxis> pad_axis_from_name(std::string_view name) {
  const auto i = index_of(kPadAxisNames, name);
  return i ? std::optional(static_cast<PadAxis>(*i)) : std::nullopt;
}
std::string_view mouse_button_name(MouseButton b) {
  return kMouseButtonNames[static_cast<size_t>(b)];
}
std::optional<MouseButton> mouse_button_from_name(std::string_view name) {
  const auto i = index_of(kMouseButtonNames, name);
  return i ? std::optional(static_cast<MouseButton>(*i)) : std::nullopt;
}
std::string_view kind_name(DeviceKind k) { return kKindNames[static_cast<size_t>(k)]; }
std::optional<DeviceKind> kind_from_name(std::string_view name) {
  const auto i = index_of(kKindNames, name);
  return i ? std::optional(static_cast<DeviceKind>(*i)) : std::nullopt;
}
std::string_view family_name(Family f) { return kFamilyNames[static_cast<size_t>(f)]; }
std::optional<Family> family_from_name(std::string_view name) {
  const auto i = index_of(kFamilyNames, name);
  return i ? std::optional(static_cast<Family>(*i)) : std::nullopt;
}
std::string_view event_type_name(EventType t) { return kEventNames[static_cast<size_t>(t)]; }
std::optional<EventType> event_type_from_name(std::string_view name) {
  const auto i = index_of(kEventNames, name);
  return i ? std::optional(static_cast<EventType>(*i)) : std::nullopt;
}

std::string_view pad_label(Family f, PadButton b) {
  const auto i = static_cast<size_t>(b);
  if (i < 4) return kFaceLabels[static_cast<size_t>(f)][i];
  if (f == Family::playstation) {
    switch (b) {
      case PadButton::back:
        return "share";
      case PadButton::start:
        return "options";
      case PadButton::left_shoulder:
        return "L1";
      case PadButton::right_shoulder:
        return "R1";
      case PadButton::left_stick:
        return "L3";
      case PadButton::right_stick:
        return "R3";
      default:
        break;
    }
  }
  if (f == Family::nintendo) {
    switch (b) {
      case PadButton::back:
        return "-";
      case PadButton::start:
        return "+";
      case PadButton::left_shoulder:
        return "L";
      case PadButton::right_shoulder:
        return "R";
      default:
        break;
    }
  }
  if (f == Family::xbox) {
    switch (b) {
      case PadButton::back:
        return "view";
      case PadButton::start:
        return "menu";
      case PadButton::left_shoulder:
        return "LB";
      case PadButton::right_shoulder:
        return "RB";
      default:
        break;
    }
  }
  return pad_button_name(b);
}

std::string usb_key(uint16_t vendor, uint16_t product) {
  char buf[16];
  std::snprintf(buf, sizeof buf, "%04x:%04x", vendor, product);
  return buf;
}

}  // namespace ge::input
