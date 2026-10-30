// The knob table (every MoveSpec field's row, written once), lookup, get and set, the parser of
// `name=value` lists, and `scale_lengths` read off the rows' dimensions.
//
// ge:summary The knob table's rows, lookup, set and clamp, the name=value parser, scale_lengths
// ge:keywords move knobs table rows parse name-value scale_lengths clamp
// ge:skill move
// ge:test tests/move_step_test.cpp
#include "move/knobs.hpp"

#include <cstdlib>
#include <cstring>
#include <limits>

#include "core/num.hpp"
#include "core/num_text.hpp"

namespace ge::move {

namespace {

constexpr double kInf = std::numeric_limits<double>::infinity();

constexpr Knob num(const char* name, const char* unit, const char* help, double lo, double hi,
                   Dim dim, double MoveSpec::*field) {
  return Knob{name, unit, help, lo, hi, KnobKind::number, dim, field, nullptr, nullptr};
}
constexpr Knob flag(const char* name, const char* help, bool MoveSpec::*field) {
  return Knob{name, "", help, 0, 1, KnobKind::flag, Dim::none, nullptr, field, nullptr};
}
constexpr Knob count(const char* name, const char* help, double hi, int32_t MoveSpec::*field) {
  return Knob{name, "", help, 0, hi, KnobKind::count, Dim::none, nullptr, nullptr, field};
}

constexpr Dim L = Dim::length;
constexpr Dim N = Dim::none;

const Knob kKnobs[] = {
    num("radius", "L", "half the hull's width", 0.001, kInf, L, &MoveSpec::radius),
    num("height", "L", "the hull standing", 0.001, kInf, L, &MoveSpec::height),
    num("crouchheight", "L", "the hull crouched", 0.001, kInf, L, &MoveSpec::crouch_height),
    num("eyeheight", "L", "the eyes above the feet, standing", 0, kInf, L, &MoveSpec::eye_height),
    num("croucheye", "L", "the eyes above the feet, crouched", 0, kInf, L, &MoveSpec::crouch_eye),
    num("maxspeed", "L/s", "the wish speed, stick fully over", 0, kInf, L, &MoveSpec::max_speed),
    num("backscale", "share", "max speed's share going backwards", 0, 4, N, &MoveSpec::back_scale),
    num("sidescale", "share", "max speed's share sideways", 0, 4, N, &MoveSpec::side_scale),
    num("walkscale", "share", "max speed's share walking", 0, 4, N, &MoveSpec::walk_scale),
    num("sprintscale", "share", "max speed's share sprinting", 0, 8, N, &MoveSpec::sprint_scale),
    num("crouchscale", "share", "max speed's share crouched", 0, 4, N, &MoveSpec::crouch_scale),
    num("accelerate", "1/s", "the ground's acceleration factor", 0, kInf, N, &MoveSpec::accelerate),
    num("friction", "1/s", "the ground's friction factor", 0, kInf, N, &MoveSpec::friction),
    num("stopspeed", "L/s", "friction acts as if at least this fast", 0, kInf, L,
        &MoveSpec::stop_speed),
    num("edgefriction", "share", "friction's factor at a drop ahead", 0, 100, N,
        &MoveSpec::edge_friction),
    num("edgeprobe", "L", "how far ahead a drop is looked for", 0, kInf, L, &MoveSpec::edge_probe),
    num("walknormal", "share", "least up part of a walkable normal", 0, 1, N,
        &MoveSpec::walk_normal),
    num("stepheight", "L", "the tallest step walked up", 0, kInf, L, &MoveSpec::step_height),
    num("groundsnap", "L", "a drop this small is followed down", 0, kInf, L,
        &MoveSpec::ground_snap),
    num("contact", "L", "a gap this small under the feet is standing", 0, kInf, L,
        &MoveSpec::contact),
    num("ungroundspeed", "L/s", "rising faster than this leaves the ground", 0, kInf, L,
        &MoveSpec::unground_speed),
    flag("frictionwhole", "ground friction counts a kept fall's speed", &MoveSpec::friction_whole),
    flag("clipfromstart", "each surface clips the move's first velocity",
         &MoveSpec::clip_from_start),
    num("landclipspeed", "L/s", "the speed into the ground is cut only past this (0: always)", 0,
        kInf, L, &MoveSpec::land_clip_speed),
    num("gravity", "L/s^2", "down", -kInf, kInf, L, &MoveSpec::gravity),
    num("fallgravity", "share", "gravity's factor while falling", 0, 100, N,
        &MoveSpec::fall_gravity_scale),
    num("terminalspeed", "L/s", "the fastest fall (0: none)", 0, kInf, L,
        &MoveSpec::terminal_speed),
    num("gravitylead", "share", "gravity's share applied before the move (0.5: halves)", 0, 1, N,
        &MoveSpec::gravity_lead),
    num("airaccelerate", "1/s", "the air's acceleration factor", 0, kInf, N,
        &MoveSpec::air_accelerate),
    num("airspeedcap", "L/s", "speed along the wish held to this in the air (0: none)", 0, kInf, L,
        &MoveSpec::air_speed_cap),
    num("aircontrol", "L/s^2", "forward-only steering in the air (0: none)", 0, kInf, L,
        &MoveSpec::air_control),
    num("airstrafeaccelerate", "1/s", "air factor with only sideways held (0: as air)", 0, kInf, N,
        &MoveSpec::air_strafe_accelerate),
    num("airstrafecap", "L/s", "wish speed with only sideways held (0: as air)", 0, kInf, L,
        &MoveSpec::air_strafe_cap),
    num("airstopaccelerate", "1/s", "air factor against the velocity (0: as air)", 0, kInf, N,
        &MoveSpec::air_stop_accelerate),
    num("airmaxspeed", "L/s", "the air never accelerates the plan speed past this (0: none)", 0,
        kInf, L, &MoveSpec::air_max_speed),
    num("jumpspeed", "L/s", "upward speed a jump gives", 0, kInf, L, &MoveSpec::jump_speed),
    flag("jumpadditive", "a jump adds to a rising speed", &MoveSpec::jump_additive),
    flag("jumphold", "a held jump jumps again on landing", &MoveSpec::jump_hold),
    num("jumpbuffer", "s", "an early press still jumps on landing", 0, 10, N,
        &MoveSpec::jump_buffer),
    num("coyotetime", "s", "a late jump after leaving the ground", 0, 10, N,
        &MoveSpec::coyote_time),
    num("jumpcut", "share", "rising speed's factor on letting go", 0, 1, N, &MoveSpec::jump_cut),
    count("airjumps", "jumps in the air before landing", 100, &MoveSpec::air_jumps),
    num("airjumpspeed", "L/s", "upward speed an air jump sets (0: jumpspeed)", 0, kInf, L,
        &MoveSpec::air_jump_speed),
    num("crouchtime", "s", "the eyes' travel crouching", 0, 10, N, &MoveSpec::crouch_time),
    flag("crouchjump", "crouching in the air pulls the feet up", &MoveSpec::crouch_jump),
    num("slideminspeed", "L/s", "crouching faster than this slides (0: no slide)", 0, kInf, L,
        &MoveSpec::slide_min_speed),
    num("slidefriction", "1/s", "friction's factor in a slide", 0, kInf, N,
        &MoveSpec::slide_friction),
    num("slideboost", "L/s", "speed added as a slide starts", 0, kInf, L, &MoveSpec::slide_boost),
    num("slidecooldown", "s", "before the boost is given again", 0, 100, N,
        &MoveSpec::slide_cooldown),
    num("slidesteer", "share", "ground acceleration's share in a slide", 0, 1, N,
        &MoveSpec::slide_steer),
    num("flyscale", "share", "max speed's share flying", 0, 100, N, &MoveSpec::fly_scale),
    num("ladderspeed", "L/s", "speed on a ladder's rungs, up, down and sideways", 0, kInf, L,
        &MoveSpec::ladder_speed),
    num("ladderjump", "L/s", "the push off a ladder a jump gives", 0, kInf, L,
        &MoveSpec::ladder_jump),
    num("ladderprobe", "L", "how far ahead of the body a ladder is found", 0, kInf, L,
        &MoveSpec::ladder_probe),
};

bool is_sep(char c) { return c == ',' || c == ' ' || c == '\t' || c == '\n'; }

}  // namespace

std::span<const Knob> knobs() { return kKnobs; }

const Knob* find_knob(std::string_view name) {
  for (const Knob& k : kKnobs) {
    if (name == k.name) return &k;
  }
  return nullptr;
}

double knob_value(const MoveSpec& spec, const Knob& knob) {
  switch (knob.kind) {
    case KnobKind::number:
      return spec.*knob.number;
    case KnobKind::flag:
      return spec.*knob.flag ? 1.0 : 0.0;
    case KnobKind::count:
      return spec.*knob.count;
  }
  return 0;
}

void set_knob(MoveSpec& spec, const Knob& knob, double value) {
  if (value != value) return;  // NaN sets nothing
  const double v = value < knob.min ? knob.min : value > knob.max ? knob.max : value;
  switch (knob.kind) {
    case KnobKind::number:
      spec.*knob.number = v;
      break;
    case KnobKind::flag:
      spec.*knob.flag = value != 0;
      break;
    case KnobKind::count:
      spec.*knob.count = static_cast<int32_t>(v < 0 ? -core::num::round(-v) : core::num::round(v));
      break;
  }
}

double* number_field(MoveSpec& spec, const Knob& knob) {
  return knob.kind == KnobKind::number ? &(spec.*knob.number) : nullptr;
}
bool* flag_field(MoveSpec& spec, const Knob& knob) {
  return knob.kind == KnobKind::flag ? &(spec.*knob.flag) : nullptr;
}
int32_t* count_field(MoveSpec& spec, const Knob& knob) {
  return knob.kind == KnobKind::count ? &(spec.*knob.count) : nullptr;
}

bool apply_knobs(MoveSpec& spec, std::string_view text, std::string& error) {
  size_t i = 0;
  while (i < text.size()) {
    while (i < text.size() && is_sep(text[i])) i++;
    size_t j = i;
    while (j < text.size() && !is_sep(text[j])) j++;
    if (j == i) break;
    const std::string_view pair = text.substr(i, j - i);
    i = j;
    const size_t eq = pair.find('=');
    const std::string_view name = pair.substr(0, eq);
    const Knob* k = find_knob(name);
    if (eq == std::string_view::npos || k == nullptr) {
      error = "move knobs: '" + std::string(pair) +
              "' is not name=value with a known name; the names are in engine/move/knobs.cpp "
              "(skill: move)";
      return false;
    }
    const std::string value(pair.substr(eq + 1));
    char* end = nullptr;
    const double v = std::strtod(value.c_str(), &end);
    if (value.empty() || end == nullptr || *end != '\0') {
      error = "move knobs: " + std::string(name) + "'s value '" + value +
              "' is not a number; write it as plain decimal (" + std::string(name) + "=" +
              core::num::to_string(knob_value(spec, *k)) + ")";
      return false;
    }
    set_knob(spec, *k, v);
  }
  return true;
}

std::string knob_text(const MoveSpec& spec, const MoveSpec& base) {
  std::string out;
  for (const Knob& k : kKnobs) {
    const double v = knob_value(spec, k);
    if (v == knob_value(base, k)) continue;
    if (!out.empty()) out += ',';
    out += k.name;
    out += '=';
    out += core::num::to_string(v);
  }
  return out;
}

MoveSpec scale_lengths(const MoveSpec& spec, double k) {
  MoveSpec out = spec;
  out.unit = spec.unit * k;
  for (const Knob& knob : kKnobs) {
    if (knob.dim == Dim::length && knob.kind == KnobKind::number) {
      out.*knob.number = spec.*knob.number * k;
    }
  }
  return out;
}

}  // namespace ge::move
