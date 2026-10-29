// The store's controller layer's shared pieces: origins and glyphs, the merge into the game's
// own action values, and the action manifest and default configuration written from the map.
//
// ge:summary Store controller helpers: origins and glyphs, merging values, the action manifest
// ge:keywords services steam-input origin glyph merge action-manifest in-game-actions vdf
// ge:keywords default-config
// ge:skill services input
// ge:test tests/services_controllers_test.cpp
#include "services/controllers.hpp"

#include <string>

#include "input/bindings_text.hpp"
#include "input/names.hpp"

namespace ge::services {

namespace {

std::string_view family_prefix(input::Family f) {
  switch (f) {
    case input::Family::xbox:
      return "xbox";
    case input::Family::playstation:
      return "ps";
    case input::Family::nintendo:
      return "switch";
    case input::Family::generic:
      break;
  }
  return "pad";
}

std::string lower(std::string_view s) {
  std::string out(s);
  for (char& c : out)
    if (c >= 'A' && c <= 'Z') c = static_cast<char>(c - 'A' + 'a');
  return out;
}

// The words a label is made of, one per axis.
std::string_view axis_label(input::PadAxis a) {
  switch (a) {
    case input::PadAxis::left_x:
      return "left stick left and right";
    case input::PadAxis::left_y:
      return "left stick up and down";
    case input::PadAxis::right_x:
      return "right stick left and right";
    case input::PadAxis::right_y:
      return "right stick up and down";
    case input::PadAxis::left_trigger:
      return "left trigger";
    case input::PadAxis::right_trigger:
      return "right trigger";
  }
  return "axis";
}

std::string_view axis_glyph(input::PadAxis a) {
  switch (a) {
    case input::PadAxis::left_x:
    case input::PadAxis::left_y:
      return "stick_left";
    case input::PadAxis::right_x:
    case input::PadAxis::right_y:
      return "stick_right";
    case input::PadAxis::left_trigger:
      return "trigger_left";
    case input::PadAxis::right_trigger:
      return "trigger_right";
  }
  return "axis";
}

float mag2(const input::ActionValue& v) { return v.x * v.x + v.y * v.y; }

// The section of Steam's in-game actions file an action goes in, and its input mode.
std::string_view manifest_group(const input::ActionDef& d) {
  switch (d.kind) {
    case input::ActionKind::button:
      return "Button";
    case input::ActionKind::axis:
      return d.lever ? "AnalogTrigger" : "StickPadGyro";
    case input::ActionKind::stick:
    case input::ActionKind::delta:
      return "StickPadGyro";
  }
  return "Button";
}

}  // namespace

bool store_readable(const input::Source& s) {
  return s.kind == input::SourceKind::pad_button || s.kind == input::SourceKind::pad_axis ||
         s.kind == input::SourceKind::pad_stick;
}

Origin origin_of(input::Family family, const input::Source& s) {
  Origin o;
  o.source = s;
  const std::string prefix(family_prefix(family));
  switch (s.kind) {
    case input::SourceKind::pad_button: {
      const auto b = static_cast<input::PadButton>(s.code);
      o.label = std::string(input::pad_label(family, b));
      o.glyph = prefix + "_" + lower(o.label);
      // A label that is a symbol ("-", "+") is no file name: the position names the glyph.
      if (o.label.empty() || !(o.label[0] >= 'A' && o.label[0] <= 'z'))
        o.glyph = prefix + "_" + std::string(input::pad_button_name(b));
      return o;
    }
    case input::SourceKind::pad_axis: {
      const auto a = static_cast<input::PadAxis>(s.code);
      o.label = std::string(axis_label(a));
      o.glyph = prefix + "_" + std::string(axis_glyph(a));
      return o;
    }
    case input::SourceKind::pad_stick:
      o.label = s.code == 0 ? "left stick" : "right stick";
      o.glyph = prefix + (s.code == 0 ? "_stick_left" : "_stick_right");
      return o;
    default:
      o.label = input::source_text(s);
      o.glyph = "unknown";
      return o;
  }
}

void merge_store_values(const input::ActionMap& map, const input::ActionValues& store,
                        input::ActionValues& into) {
  const size_t n = map.defs.size();
  if (store.v.size() < n || into.v.size() < n) return;
  for (size_t i = 0; i < n; i++) {
    const input::ActionValue& s = store.v[i];
    input::ActionValue& v = into.v[i];
    switch (map.defs[i].kind) {
      case input::ActionKind::button:
        v.held = v.held || s.held;
        v.pressed = v.pressed || s.pressed;
        v.released = (v.released || s.released) && !v.held;
        if (s.held && !v.device) v.device = s.device;
        break;
      case input::ActionKind::axis:
      case input::ActionKind::stick:
        if (mag2(s) > mag2(v)) {
          v.x = s.x;
          v.y = s.y;
          v.device = s.device;
        }
        v.pressed = v.pressed || s.pressed;
        v.held = v.held || s.held;
        v.released = (v.released || s.released) && !v.held;
        break;
      case input::ActionKind::delta:
        v.x += s.x;
        v.y += s.y;
        break;
    }
  }
}

std::string action_manifest_text(const input::ActionMap& map) {
  std::string out = "\"In Game Actions\"\n{\n\t\"actions\"\n\t{\n";
  std::string words;
  for (const input::Context& c : map.contexts) {
    out += "\t\t\"" + c.name + "\"\n\t\t{\n\t\t\t\"title\"\t\"#Set_" + c.name + "\"\n";
    words += "\t\t\t\"Set_" + c.name + "\"\t\"" + c.name + "\"\n";
    for (const std::string_view group : {"StickPadGyro", "AnalogTrigger", "Button"}) {
      std::string body;
      std::vector<bool> seen(map.defs.size(), false);
      for (const input::Binding& b : c.bindings) {
        if (b.action >= map.defs.size() || seen[b.action] || !store_readable(b.source)) continue;
        const input::ActionDef& d = map.defs[b.action];
        if (manifest_group(d) != group) continue;
        seen[b.action] = true;
        const std::string key = "Action_" + c.name + "_" + d.name;
        if (group == "StickPadGyro") {
          const char* mode =
              d.kind == input::ActionKind::delta ? "absolute_mouse" : "joystick_move";
          body += "\t\t\t\t\"" + d.name + "\"\n\t\t\t\t{\n\t\t\t\t\t\"title\"\t\"#" + key +
                  "\"\n\t\t\t\t\t\"input_mode\"\t\"" + mode + "\"\n\t\t\t\t}\n";
        } else {
          body += "\t\t\t\t\"" + d.name + "\"\t\"#" + key + "\"\n";
        }
        words += "\t\t\t\"" + key + "\"\t\"" + (d.help.empty() ? d.name : d.help) + "\"\n";
      }
      if (!body.empty())
        out += "\t\t\t\"" + std::string(group) + "\"\n\t\t\t{\n" + body + "\t\t\t}\n";
    }
    out += "\t\t}\n";
  }
  out += "\t}\n\t\"localization\"\n\t{\n\t\t\"english\"\n\t\t{\n" + words + "\t\t}\n\t}\n}\n";
  return out;
}

std::string default_config_text(const input::ActionMap& map) {
  std::string out = "# the store's default configuration: set action = source\n";
  for (const input::Context& c : map.contexts)
    for (const input::Binding& b : c.bindings) {
      if (b.action >= map.defs.size() || !store_readable(b.source)) continue;
      out += c.name + " " + map.defs[b.action].name + " = " + input::source_text(b.source) + "\n";
    }
  return out;
}

}  // namespace ge::services
