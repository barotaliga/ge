// The event log's writer and reader, and the features' names.
//
// ge:summary The event log's text writer and reader; feature names both ways
// ge:keywords input event-log writer reader features
// ge:skill input
// ge:test tests/input_sim_test.cpp
#include "input/event_log.hpp"

#include <array>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <map>

#include "input/names.hpp"
#include "input/text.hpp"

namespace ge::input {

namespace {

constexpr std::array<std::string_view, 10> kFeatureNames = {
    "rumble", "trigger_rumble", "light", "player_lights", "gyro",
    "accel",  "touchpad",       "force", "battery",       "raw_motion"};
constexpr std::array<std::string_view, 3> kPhases = {"down", "move", "up"};

std::string secs(uint64_t t_ns) { return text::num(static_cast<double>(t_ns) * 1e-9); }

std::optional<uint64_t> nanos(std::string_view w) {
  const auto t = text::number(w);
  if (!t || *t < 0) return std::nullopt;
  return static_cast<uint64_t>(std::llround(*t * 1e9));
}

std::string f(float v) { return text::num(v); }

std::string hex4(uint16_t v) {
  char buf[8];
  std::snprintf(buf, sizeof buf, "%04x", v);
  return buf;
}

std::string device_line(const DeviceInfo& d) {
  std::string s = "device " + std::to_string(d.id) + " " + std::string(kind_name(d.kind)) + " " +
                  std::string(family_name(d.family)) + " " + text::quote(d.key);
  if (d.vendor || d.product) s += " vendor=" + hex4(d.vendor) + " product=" + hex4(d.product);
  if (d.features) s += " features=" + feature_names(d.features);
  if (d.axes) s += " axes=" + std::to_string(d.axes);
  if (d.buttons) s += " buttons=" + std::to_string(d.buttons);
  if (d.hats) s += " hats=" + std::to_string(d.hats);
  if (d.touchpads) s += " touchpads=" + std::to_string(d.touchpads);
  if (d.player >= 0) s += " player=" + std::to_string(d.player);
  if (!d.name.empty()) s += " " + text::quote(d.name);
  return s;
}

std::string event_line(const Event& e) {
  std::string s =
      secs(e.t_ns) + " " + std::to_string(e.device) + " " + std::string(event_type_name(e.type));
  const auto updown = [&] { return std::string(e.v[0] != 0 ? " down" : " up"); };
  switch (e.type) {
    case EventType::device_added:
    case EventType::device_removed:
      break;
    case EventType::key:
      s += " " + key_name(e.code) + updown() + (e.aux ? " repeat" : "");
      break;
    case EventType::text:
      s += " " + f(e.v[0]);
      break;
    case EventType::mouse_motion:
      s += " " + f(e.v[0]) + " " + f(e.v[1]) + " " + f(e.v[2]) + " " + f(e.v[3]) +
           (e.aux ? " raw" : "");
      break;
    case EventType::mouse_button:
      s += " " + std::string(mouse_button_name(static_cast<MouseButton>(e.code))) + updown();
      break;
    case EventType::mouse_wheel:
      s += " " + f(e.v[0]) + " " + f(e.v[1]);
      break;
    case EventType::pad_button:
      s += " " + std::string(pad_button_name(static_cast<PadButton>(e.code))) + updown();
      break;
    case EventType::pad_axis:
      s += " " + std::string(pad_axis_name(static_cast<PadAxis>(e.code))) + " " + f(e.v[0]);
      break;
    case EventType::joy_button:
      s += " " + std::to_string(e.code) + updown();
      break;
    case EventType::joy_axis:
    case EventType::joy_hat:
      s += " " + std::to_string(e.code) + " " + f(e.v[0]);
      break;
    case EventType::sensor:
      s += std::string(e.code ? " accel " : " gyro ") + f(e.v[0]) + " " + f(e.v[1]) + " " +
           f(e.v[2]);
      break;
    case EventType::touch:
    case EventType::pad_touch:
      s += " " + std::to_string(e.code) + " " + std::string(kPhases[e.aux < 3 ? e.aux : 0]) + " " +
           f(e.v[0]) + " " + f(e.v[1]) + " " + f(e.v[2]);
      if (e.type == EventType::pad_touch) s += " " + f(e.v[3]);
      break;
    case EventType::battery:
      s += " " + f(e.v[0]);
      break;
  }
  return s;
}

}  // namespace

std::string feature_names(uint32_t features) {
  std::string out;
  for (size_t i = 0; i < kFeatureNames.size(); i++) {
    if (features & (1u << i)) out += (out.empty() ? "" : ",") + std::string(kFeatureNames[i]);
  }
  return out;
}

bool features_from_names(std::string_view names, uint32_t& features) {
  features = 0;
  while (!names.empty()) {
    const size_t comma = names.find(',');
    const std::string_view n = names.substr(0, comma);
    bool found = false;
    for (size_t i = 0; i < kFeatureNames.size(); i++) {
      if (kFeatureNames[i] == n) {
        features |= 1u << i;
        found = true;
      }
    }
    if (!found) return false;
    if (comma == std::string_view::npos) break;
    names.remove_prefix(comma + 1);
  }
  return true;
}

std::string write_events(const EventLog& log) {
  std::string out = "ge-events 1\n";
  for (const LoggedFrame& fr : log.frames) {
    for (const DeviceInfo& d : fr.q.plugged) out += device_line(d) + "\n";
    out += "frame " + secs(fr.t_ns) + "\n";
    for (const Event& e : fr.q.events) out += event_line(e) + "\n";
  }
  return out;
}

bool read_events(std::string_view src, EventLog& log, std::string& error) {
  std::map<DeviceId, DeviceInfo> infos;
  bool header = false;
  log.frames.clear();
  for (const auto& [line_no, line] : text::lines(src)) {
    const auto w = text::split_words(line);
    if (w.empty()) continue;
    const auto fail = [&, n = line_no](const std::string& what) {
      error = "events line " + std::to_string(n) + ": " + what +
              "; the format is in engine/input/event_log.hpp (skill: input)";
      return false;
    };
    if (!header) {
      if (w.size() != 2 || w[0] != "ge-events" || w[1] != "1") {
        return fail("the first line must be 'ge-events 1'");
      }
      header = true;
      continue;
    }
    if (w[0] == "device") {
      if (w.size() < 5) return fail("'device <id> <kind> <family> <key> [options] [\"name\"]'");
      DeviceInfo d;
      const auto id = text::number(w[1]);
      const auto kind = kind_from_name(w[2]);
      const auto fam = family_from_name(w[3]);
      if (!id || *id < 1 || !kind || !fam) return fail("the id, the kind or the family is wrong");
      d.id = static_cast<DeviceId>(*id);
      d.kind = *kind;
      d.family = *fam;
      d.key = w[4];
      for (size_t i = 5; i < w.size(); i++) {
        const auto [k, v] = text::option(w[i]);
        const std::string key(k);
        const auto num = text::number(v);
        if (key == "vendor" || key == "product") {
          const auto x = static_cast<uint16_t>(std::strtoul(std::string(v).c_str(), nullptr, 16));
          (key == "vendor" ? d.vendor : d.product) = x;
        } else if (key == "features") {
          if (!features_from_names(v, d.features)) return fail("unknown feature in '" + w[i] + "'");
        } else if (key == "axes" && num) {
          d.axes = static_cast<int>(*num);
        } else if (key == "buttons" && num) {
          d.buttons = static_cast<int>(*num);
        } else if (key == "hats" && num) {
          d.hats = static_cast<int>(*num);
        } else if (key == "touchpads" && num) {
          d.touchpads = static_cast<int>(*num);
        } else if (key == "player" && num) {
          d.player = static_cast<int>(*num);
        } else {
          d.name = w[i];
        }
      }
      infos[d.id] = d;
      continue;
    }
    if (w[0] == "frame") {
      const auto t = w.size() == 2 ? nanos(w[1]) : std::nullopt;
      if (!t) return fail("'frame <time s>'");
      log.frames.push_back({*t, {}});
      continue;
    }
    if (log.frames.empty()) return fail("an event before any 'frame' line");
    if (w.size() < 3) return fail("'<t> <device> <type> ...'");
    const auto t = nanos(w[0]);
    const auto dev = text::number(w[1]);
    const auto type = event_type_from_name(w[2]);
    if (!t || !dev || !type)
      return fail("the time, the device or the type '" + w[2] + "' is wrong");
    Queue& q = log.frames.back().q;
    Event e{*t, static_cast<DeviceId>(*dev), *type, 0, 0, {}};
    const auto need = [&](size_t k) { return w.size() >= 3 + k; };
    const auto val = [&](size_t i, float& out) {
      const auto x = i < w.size() ? text::number(w[i]) : std::nullopt;
      if (x) out = static_cast<float>(*x);
      return x.has_value();
    };
    const auto down = [&](size_t i) -> std::optional<bool> {
      if (i >= w.size()) return std::nullopt;
      if (w[i] == "down") return true;
      if (w[i] == "up") return false;
      return std::nullopt;
    };
    bool ok = true;
    switch (e.type) {
      case EventType::device_added: {
        const auto it = infos.find(e.device);
        if (it == infos.end()) return fail("device " + w[1] + " has no 'device' line before it");
        q.plug(e.t_ns, it->second);
        continue;
      }
      case EventType::device_removed:
        break;
      case EventType::key: {
        const auto k = need(2) ? key_from_name(w[3]) : std::nullopt;
        const auto d = down(4);
        ok = k && d;
        if (ok) {
          e.code = *k;
          e.v[0] = *d ? 1.0f : 0.0f;
          e.aux = w.size() > 5 && w[5] == "repeat";
        }
        break;
      }
      case EventType::text:
      case EventType::battery:
        ok = val(3, e.v[0]);
        break;
      case EventType::mouse_motion:
        ok = val(3, e.v[0]) && val(4, e.v[1]) && val(5, e.v[2]) && val(6, e.v[3]);
        e.aux = w.size() > 7 && w[7] == "raw";
        break;
      case EventType::mouse_wheel:
        ok = val(3, e.v[0]) && val(4, e.v[1]);
        break;
      case EventType::mouse_button:
      case EventType::pad_button: {
        std::optional<uint16_t> code;
        if (need(1) && e.type == EventType::mouse_button) {
          if (const auto b = mouse_button_from_name(w[3])) code = static_cast<uint16_t>(*b);
        } else if (need(1)) {
          if (const auto b = pad_button_from_name(w[3])) code = static_cast<uint16_t>(*b);
        }
        const auto d = down(4);
        ok = code && d;
        if (ok) {
          e.code = *code;
          e.v[0] = *d ? 1.0f : 0.0f;
        }
        break;
      }
      case EventType::pad_axis: {
        const auto a = need(1) ? pad_axis_from_name(w[3]) : std::nullopt;
        ok = a && val(4, e.v[0]);
        if (ok) e.code = static_cast<uint16_t>(*a);
        break;
      }
      case EventType::joy_button:
      case EventType::joy_axis:
      case EventType::joy_hat: {
        const auto i = need(1) ? text::number(w[3]) : std::nullopt;
        ok = i && *i >= 0 && *i < 256;
        if (ok) e.code = static_cast<uint16_t>(*i);
        if (ok && e.type == EventType::joy_button) {
          const auto d = down(4);
          ok = d.has_value();
          if (ok) e.v[0] = *d ? 1.0f : 0.0f;
        } else if (ok) {
          ok = val(4, e.v[0]);
        }
        break;
      }
      case EventType::sensor:
        ok = need(4) && (w[3] == "gyro" || w[3] == "accel") && val(4, e.v[0]) && val(5, e.v[1]) &&
             val(6, e.v[2]);
        e.code = need(1) && w[3] == "accel";
        break;
      case EventType::touch:
      case EventType::pad_touch: {
        const auto finger = need(1) ? text::number(w[3]) : std::nullopt;
        int phase = -1;
        for (int p = 0; p < 3 && need(2); p++) {
          if (w[4] == kPhases[static_cast<size_t>(p)]) phase = p;
        }
        ok = finger && phase >= 0 && val(5, e.v[0]) && val(6, e.v[1]) && val(7, e.v[2]);
        if (ok && e.type == EventType::pad_touch) ok = val(8, e.v[3]);
        if (ok && finger) {
          e.code = static_cast<uint16_t>(*finger);
          e.aux = static_cast<uint8_t>(phase);
        }
        break;
      }
    }
    if (!ok) return fail("the " + w[2] + " event's values do not read");
    q.push(e);
  }
  if (!header) {
    error = "events: empty; the first line must be 'ge-events 1' (skill: input)";
    return false;
  }
  return true;
}

}  // namespace ge::input
