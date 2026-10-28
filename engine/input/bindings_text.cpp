// The bindings file's writer and reader, and a source as one word both ways.
//
// ge:summary The bindings file's writer and reader; source_text and parse_source
// ge:keywords input bindings text writer reader source word
// ge:skill input
// ge:test tests/input_actions_test.cpp
#include "input/bindings_text.hpp"

#include "input/names.hpp"
#include "input/text.hpp"

namespace ge::input {

namespace {

std::string half_suffix(int8_t half) { return half > 0 ? "+" : (half < 0 ? "-" : ""); }

constexpr std::string_view kHatDirs[] = {"up", "right", "down", "left"};

std::optional<unsigned> index_after(std::string_view word, std::string_view prefix) {
  if (word.size() <= prefix.size() || word.substr(0, prefix.size()) != prefix) return std::nullopt;
  unsigned v = 0;
  for (const char c : word.substr(prefix.size())) {
    if (c < '0' || c > '9') return std::nullopt;
    v = v * 10 + static_cast<unsigned>(c - '0');
    if (v > 255) return std::nullopt;
  }
  return v;
}

bool pair_of(std::string_view v, float& a, float& b) {
  const size_t comma = v.find(',');
  if (comma == std::string_view::npos) return false;
  const auto x = text::number(v.substr(0, comma)), y = text::number(v.substr(comma + 1));
  if (!x || !y) return false;
  a = static_cast<float>(*x);
  b = static_cast<float>(*y);
  return true;
}

}  // namespace

std::string source_text(const Source& s) {
  const std::string h = half_suffix(s.half);
  switch (s.kind) {
    case SourceKind::none:
      return "none";
    case SourceKind::key:
      return "key:" + key_name(s.code);
    case SourceKind::mouse_button:
      return "mouse:" + std::string(mouse_button_name(static_cast<MouseButton>(s.code)));
    case SourceKind::mouse_motion:
      return std::string("mouse:") + (s.code ? "y" : "x") + h;
    case SourceKind::mouse_wheel:
      return std::string("mouse:") + (s.code ? "wheel_y" : "wheel_x") + h;
    case SourceKind::pad_button:
      return "pad:" + std::string(pad_button_name(static_cast<PadButton>(s.code)));
    case SourceKind::pad_axis:
      return "pad:" + std::string(pad_axis_name(static_cast<PadAxis>(s.code))) + h;
    case SourceKind::pad_stick:
      return s.code ? "pad:right_stick" : "pad:left_stick";
    case SourceKind::joy_button:
      return "joy:button" + std::to_string(s.code);
    case SourceKind::joy_axis:
      return "joy:axis" + std::to_string(s.code) + h;
    case SourceKind::joy_hat: {
      std::string dir = "up";
      for (int i = 0; i < 4; i++) {
        if (s.dir == (1 << i)) dir = kHatDirs[i];
      }
      return "joy:hat" + std::to_string(s.code) + "_" + dir;
    }
    case SourceKind::gyro:
      return std::string("gyro:") + "xyz"[s.code < 3 ? s.code : 0] + h;
    case SourceKind::touch:
      return "touch:" + s.name + h;
    case SourceKind::role:
      return "role:" + std::string(role_name(static_cast<Role>(s.code))) + h;
  }
  return "none";
}

std::optional<Source> parse_source(std::string_view word) {
  const size_t colon = word.find(':');
  if (colon == std::string_view::npos) return std::nullopt;
  const std::string_view dev = word.substr(0, colon);
  std::string_view what = word.substr(colon + 1);
  Source s;
  if (!what.empty() && (what.back() == '+' || what.back() == '-') && what.size() > 1) {
    s.half = what.back() == '+' ? 1 : -1;
    what.remove_suffix(1);
  }
  const auto done = [&](SourceKind k, unsigned code) -> std::optional<Source> {
    s.kind = k;
    s.code = static_cast<uint16_t>(code);
    return s;
  };
  if (dev == "key") {
    if (s.half) return std::nullopt;
    const auto k = key_from_name(what);
    return k ? done(SourceKind::key, *k) : std::nullopt;
  }
  if (dev == "mouse") {
    if (what == "x" || what == "y") return done(SourceKind::mouse_motion, what == "y");
    if (what == "wheel_x" || what == "wheel_y") {
      return done(SourceKind::mouse_wheel, what == "wheel_y");
    }
    const auto b = mouse_button_from_name(what);
    return b && !s.half ? done(SourceKind::mouse_button, static_cast<unsigned>(*b)) : std::nullopt;
  }
  if (dev == "pad") {
    if (what == "left_stick" || what == "right_stick") {
      if (s.half) return std::nullopt;
      return done(SourceKind::pad_stick, what == "right_stick");
    }
    if (const auto a = pad_axis_from_name(what)) {
      return done(SourceKind::pad_axis, static_cast<unsigned>(*a));
    }
    const auto b = pad_button_from_name(what);
    return b && !s.half ? done(SourceKind::pad_button, static_cast<unsigned>(*b)) : std::nullopt;
  }
  if (dev == "joy") {
    if (const auto i = index_after(what, "button"); i && !s.half) {
      return done(SourceKind::joy_button, *i);
    }
    if (const auto i = index_after(what, "axis")) return done(SourceKind::joy_axis, *i);
    if (what.substr(0, 3) == "hat" && !s.half) {
      const size_t us = what.find('_');
      if (us == std::string_view::npos) return std::nullopt;
      const auto i = index_after(what.substr(0, us), "hat");
      const std::string_view dir = what.substr(us + 1);
      for (int d = 0; d < 4 && i; d++) {
        if (dir == kHatDirs[d]) {
          s.dir = static_cast<uint8_t>(1 << d);
          return done(SourceKind::joy_hat, *i);
        }
      }
    }
    return std::nullopt;
  }
  if (dev == "gyro") {
    if (what.size() != 1 || what[0] < 'x' || what[0] > 'z') return std::nullopt;
    return done(SourceKind::gyro, static_cast<unsigned>(what[0] - 'x'));
  }
  if (dev == "touch") {
    if (what.empty()) return std::nullopt;
    s.name = std::string(what);
    return done(SourceKind::touch, 0);
  }
  if (dev == "role") {
    const auto r = role_from_name(what);
    return r ? done(SourceKind::role, static_cast<unsigned>(*r)) : std::nullopt;
  }
  return std::nullopt;
}

std::string write_bindings(const ActionMap& map) {
  std::string out = "ge-bindings 1\n";
  for (const Context& c : map.contexts) {
    out += "context " + text::quote(c.name);
    if (c.blocking) out += " blocking";
    if (!c.active) out += " off";
    out += "\n";
    for (const Binding& b : c.bindings) {
      if (b.action >= map.defs.size()) continue;
      const Binding dflt = make_binding(b.action, b.source);
      out += "bind " + map.defs[b.action].name + " " + source_text(b.source);
      if (b.value != 1) out += " value=" + text::num(b.value);
      if (b.component) out += " component=y";
      const bool stick =
          b.source.kind == SourceKind::pad_stick ||
          (b.source.kind == SourceKind::touch && map.defs[b.action].kind == ActionKind::stick);
      const float inner = stick ? b.stick.inner : b.shape.inner;
      const float outer = stick ? b.stick.outer : b.shape.outer;
      const float d_inner = stick ? dflt.stick.inner : dflt.shape.inner;
      const float d_outer = stick ? dflt.stick.outer : dflt.shape.outer;
      if (stick && b.stick.zone != dflt.stick.zone) {
        out += " zone=" + std::string(zone_name(b.stick.zone));
      }
      if (inner != d_inner || outer != d_outer) {
        out += " deadzone=" + text::num(inner) + "," + text::num(outer);
      }
      const float curve = stick ? b.stick.curve.exponent : b.shape.curve.exponent;
      if (curve != 1) out += " curve=" + text::num(curve);
      const float anti = stick ? b.stick.anti : b.shape.anti;
      if (anti != 0) out += " anti=" + text::num(anti);
      if (b.threshold.on != dflt.threshold.on || b.threshold.off != dflt.threshold.off) {
        out += " threshold=" + text::num(b.threshold.on) + "," + text::num(b.threshold.off);
      }
      if (b.ramp.attack != 0 || b.ramp.release != 0) {
        out += " ramp=" + text::num(b.ramp.attack) + "," + text::num(b.ramp.release);
      }
      if (b.rate != 0) out += " rate=" + text::num(b.rate);
      if (b.with.kind != SourceKind::none) out += " with=" + source_text(b.with);
      if (!b.device.empty()) out += " device=" + b.device;
      out += "\n";
    }
  }
  return out;
}

bool read_bindings(std::string_view src, ActionMap& map, std::string& error) {
  std::vector<Context> contexts;
  bool header = false;
  for (const auto& [line_no, line] : text::lines(src)) {
    const auto w = text::split_words(line);
    if (w.empty()) continue;
    const auto fail = [&, n = line_no](const std::string& what) {
      error = "bindings line " + std::to_string(n) + ": " + what +
              "; the format is in engine/input/bindings_text.hpp (skill: input)";
      return false;
    };
    if (!header) {
      if (w.size() != 2 || w[0] != "ge-bindings" || w[1] != "1") {
        return fail("the first line must be 'ge-bindings 1'");
      }
      header = true;
      continue;
    }
    if (w[0] == "context") {
      if (w.size() < 2) return fail("'context <name> [blocking] [off]'");
      Context c;
      c.name = w[1];
      for (size_t i = 2; i < w.size(); i++) {
        if (w[i] == "blocking") {
          c.blocking = true;
        } else if (w[i] == "off") {
          c.active = false;
        } else {
          return fail("unknown context word '" + w[i] + "' (blocking, off)");
        }
      }
      contexts.push_back(std::move(c));
      continue;
    }
    if (w[0] != "bind") return fail("unknown line '" + w[0] + "' (context, bind)");
    if (contexts.empty()) return fail("'bind' before any 'context' line");
    if (w.size() < 3) return fail("'bind <action> <source> [options]'");
    const int action = map.find_action(w[1]);
    if (action < 0) {
      std::string names;
      for (const ActionDef& d : map.defs) names += (names.empty() ? "" : ", ") + d.name;
      return fail("no action '" + w[1] + "' (the game's actions: " + names + ")");
    }
    const auto source = parse_source(w[2]);
    if (!source) return fail("'" + w[2] + "' is not a source (key:a, pad:south, joy:axis2, ...)");
    Binding b = make_binding(static_cast<uint16_t>(action), *source);
    for (size_t i = 3; i < w.size(); i++) {
      const auto [k, v] = text::option(w[i]);
      const auto num = text::number(v);
      const std::string key(k);
      if (key == "value" && num) {
        b.value = static_cast<float>(*num);
      } else if (key == "component" && (v == "x" || v == "y")) {
        b.component = v == "y";
      } else if (key == "deadzone" && pair_of(v, b.shape.inner, b.shape.outer)) {
        b.stick.inner = b.shape.inner;
        b.stick.outer = b.shape.outer;
      } else if (key == "curve" && num) {
        b.shape.curve.exponent = b.stick.curve.exponent = static_cast<float>(*num);
      } else if (key == "anti" && num) {
        b.shape.anti = b.stick.anti = static_cast<float>(*num);
      } else if (key == "zone" && zone_from_name(v)) {
        b.stick.zone = *zone_from_name(v);
      } else if ((key == "threshold" && pair_of(v, b.threshold.on, b.threshold.off)) ||
                 (key == "ramp" && pair_of(v, b.ramp.attack, b.ramp.release))) {
        // pair_of has filled the pair in
      } else if (key == "rate" && num) {
        b.rate = static_cast<float>(*num);
      } else if (key == "with" && parse_source(v)) {
        b.with = *parse_source(v);
      } else if (key == "device" && !v.empty()) {
        b.device = std::string(v);
      } else {
        return fail("option '" + w[i] +
                    "' is not one of value= component= deadzone= curve= anti= zone= threshold= "
                    "ramp= rate= with= device=, or its value does not read");
      }
    }
    contexts.back().bindings.push_back(std::move(b));
  }
  if (!header) {
    error = "bindings: empty; the first line must be 'ge-bindings 1' (skill: input)";
    return false;
  }
  map.contexts = std::move(contexts);
  return true;
}

}  // namespace ge::input
