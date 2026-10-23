// The device database's rows, the kinds' defaults, a role read off a slot, its text, and
// learning a row.
//
// ge:summary The device database: role table, defaults, role_value, the text, learning
// ge:keywords input device-db role defaults text learn calibration
// ge:skill input
// ge:test tests/input_device_db_test.cpp
#include "input/device_db.hpp"

#include <array>
#include <cmath>

#include "input/names.hpp"
#include "input/text.hpp"

namespace ge::input {

namespace {

constexpr std::array<std::string_view, kRoles> kRoleNames = {
    "none",   "steer",    "stick_x",    "stick_y",     "twist",     "rudder",   "mini_x",
    "mini_y", "throttle", "brake",      "clutch",      "handbrake", "toe_left", "toe_right",
    "slider", "shift_up", "shift_down", "gear_1",      "gear_2",    "gear_3",   "gear_4",
    "gear_5", "gear_6",   "gear_7",     "gear_reverse"};

}  // namespace

bool is_signed(Role r) { return r >= Role::steer && r <= Role::mini_y; }
bool is_button(Role r) { return r >= Role::shift_up; }
std::string_view role_name(Role r) { return kRoleNames[static_cast<size_t>(r)]; }
std::optional<Role> role_from_name(std::string_view name) {
  for (size_t i = 0; i < kRoleNames.size(); i++) {
    if (kRoleNames[i] == name) return static_cast<Role>(i);
  }
  return std::nullopt;
}

const DeviceRow* DeviceDb::find(std::string_view key) const {
  for (const DeviceRow& r : rows) {
    if (r.key == key) return &r;
  }
  return nullptr;
}

void DeviceDb::put(DeviceRow row) {
  for (DeviceRow& r : rows) {
    if (r.key == row.key) {
      r = std::move(row);
      return;
    }
  }
  rows.push_back(std::move(row));
}

DeviceRow row_for(const DeviceDb* db, const DeviceInfo& info) {
  if (db) {
    if (const DeviceRow* r = db->find(info.key)) return *r;
  }
  DeviceRow row;
  row.key = info.key;
  row.kind = info.kind;
  row.name = info.name;
  if (info.kind == DeviceKind::wheel && info.axes > 0) row.axes.push_back({Role::steer, 0});
  if (info.kind == DeviceKind::flight_stick && info.axes > 1) {
    row.axes.push_back({Role::stick_x, 0});
    row.axes.push_back({Role::stick_y, 1});
  }
  return row;
}

float role_value(const DeviceRow& row, const Slot& slot, Role role, bool* found) {
  if (found) *found = false;
  if (is_button(role)) {
    for (const ButtonRole& b : row.buttons) {
      if (b.role == role) {
        if (found) *found = true;
        return slot.buttons.down(b.button) ? 1.0f : 0.0f;
      }
    }
    return 0;
  }
  for (const AxisRole& a : row.axes) {
    if (a.role != role || a.axis >= kMaxAxes) continue;
    if (found) *found = true;
    const double span = static_cast<double>(a.hi) - a.lo;
    if (span == 0) return 0;
    double t = (slot.axes[a.axis] - a.lo) / span;  // 0 .. 1 along lo -> hi
    t = t < 0 ? 0 : (t > 1 ? 1 : t);
    return static_cast<float>(is_signed(role) ? t * 2 - 1 : t);
  }
  return 0;
}

std::string write_db(const DeviceDb& db) {
  std::string out = "ge-devices 1\n";
  for (const DeviceRow& r : db.rows) {
    out += "device " + r.key + " " + std::string(kind_name(r.kind));
    if (r.degrees > 0) out += " degrees=" + text::num(r.degrees);
    if (!r.name.empty()) out += " " + text::quote(r.name);
    out += "\n";
    for (const AxisRole& a : r.axes) {
      out += "axis " + std::string(role_name(a.role)) + " " + std::to_string(a.axis) + " " +
             text::num(a.lo) + " " + text::num(a.hi) + "\n";
    }
    for (const ButtonRole& b : r.buttons) {
      out += "button " + std::string(role_name(b.role)) + " " + std::to_string(b.button) + "\n";
    }
  }
  return out;
}

bool read_db(std::string_view src, DeviceDb& db, std::string& error) {
  DeviceRow* row = nullptr;
  bool header = false;
  for (const auto& [line_no, line] : text::lines(src)) {
    const auto w = text::split_words(line);
    if (w.empty()) continue;
    const auto fail = [&, n = line_no](const std::string& what) {
      error = "devices line " + std::to_string(n) + ": " + what +
              "; the format is in engine/input/device_db.hpp (skill: input)";
      return false;
    };
    if (!header) {
      if (w.size() != 2 || w[0] != "ge-devices" || w[1] != "1") {
        return fail("the first line must be 'ge-devices 1'");
      }
      header = true;
      continue;
    }
    if (w[0] == "device") {
      if (w.size() < 3) return fail("'device <key> <kind> [degrees=<d>] [\"name\"]'");
      const auto kind = kind_from_name(w[2]);
      if (!kind) return fail("unknown kind '" + w[2] + "'");
      DeviceRow r;
      r.key = w[1];
      r.kind = *kind;
      for (size_t i = 3; i < w.size(); i++) {
        const auto [k, v] = text::option(w[i]);
        if (k == "degrees" && !v.empty()) {
          const auto d = text::number(v);
          if (!d) return fail("degrees '" + std::string(v) + "' is not a number");
          r.degrees = static_cast<float>(*d);
        } else {
          r.name = w[i];
        }
      }
      db.put(std::move(r));
      row = nullptr;
      for (DeviceRow& x : db.rows) {
        if (x.key == w[1]) row = &x;
      }
    } else if (w[0] == "axis" || w[0] == "button") {
      if (!row) return fail("'" + w[0] + "' before any 'device' line");
      const auto role = w.size() > 2 ? role_from_name(w[1]) : std::nullopt;
      if (!role) return fail("unknown role '" + (w.size() > 1 ? w[1] : "") + "'");
      const auto index = text::number(w[2]);
      if (!index || *index < 0 || *index > 63) return fail("index '" + w[2] + "' out of range");
      if (w[0] == "button") {
        if (!is_button(*role)) return fail("'" + w[1] + "' is an axis role");
        row->buttons.push_back({*role, static_cast<uint8_t>(*index)});
      } else {
        if (is_button(*role)) return fail("'" + w[1] + "' is a button role");
        AxisRole a{*role, static_cast<uint8_t>(*index)};
        if (w.size() >= 5) {
          const auto lo = text::number(w[3]), hi = text::number(w[4]);
          if (!lo || !hi) return fail("the range must be two numbers");
          a.lo = static_cast<float>(*lo);
          a.hi = static_cast<float>(*hi);
        }
        row->axes.push_back(a);
      }
    } else {
      return fail("unknown line '" + w[0] + "'");
    }
  }
  if (!header) {
    error = "devices: empty; the first line must be 'ge-devices 1' (skill: input)";
    return false;
  }
  return true;
}

void learn_begin(Learn& l, const Slot& slot, Role role) {
  l.role = role;
  l.active = true;
  l.rest = slot.axes;
  l.lo = slot.axes;
  l.hi = slot.axes;
  l.pressed = 0;
}

void learn_feed(Learn& l, const Slot& slot) {
  if (!l.active) return;
  for (int i = 0; i < kMaxAxes; i++) {
    if (slot.axes[i] < l.lo[i]) l.lo[i] = slot.axes[i];
    if (slot.axes[i] > l.hi[i]) l.hi[i] = slot.axes[i];
  }
  l.pressed |= slot.buttons.pressed;
}

std::optional<AxisRole> learn_axis(const Learn& l) {
  if (!l.active || is_button(l.role)) return std::nullopt;
  int best = -1;
  float travel = 1.0f;  // more than half the range (-1 .. 1)
  for (int i = 0; i < kMaxAxes; i++) {
    const float t = l.hi[i] - l.lo[i];
    if (t > travel) {
      travel = t;
      best = i;
    }
  }
  if (best < 0) return std::nullopt;
  AxisRole a{l.role, static_cast<uint8_t>(best), l.lo[best], l.hi[best]};
  // A lever starts where it rests: its rest is at the end it was nearest.
  if (!is_signed(l.role) &&
      std::fabs(l.rest[best] - l.hi[best]) < std::fabs(l.rest[best] - l.lo[best])) {
    a.lo = l.hi[best];
    a.hi = l.lo[best];
  }
  return a;
}

std::optional<ButtonRole> learn_button(const Learn& l) {
  if (!l.active || !is_button(l.role) || l.pressed == 0) return std::nullopt;
  for (int i = 0; i < 64; i++) {
    if ((l.pressed >> i) & 1u) return ButtonRole{l.role, static_cast<uint8_t>(i)};
  }
  return std::nullopt;
}

}  // namespace ge::input
