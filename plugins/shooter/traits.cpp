// A player's personality (traits.hpp): the traits, the presets, the steps and the smooth reading.
//
// ge:summary A player's personality: the presets, tier_of, scale, the text form of the traits
// ge:keywords traits_of tier_of temper_of selfish scale apply_traits traits_text
// ge:skill shooter bots
// ge:test tests/shooter_traits_test.cpp
#include "shooter/traits.hpp"

#include <algorithm>

namespace ge::shooter {

namespace {

constexpr const char* kNames[kTraits] = {
    "aim_sg",  "aim_ng",     "aim_gl", "aim_rl",  "aim_lg", "reaction", "timing",     "knowledge",
    "camping", "discipline", "moves",  "hearing", "comms",  "teamplay", "aggression", "dodge"};

uint8_t preset_of(AimSkill skill) {
  switch (skill) {
    case AimSkill::novice:
      return 2;
    case AimSkill::average:
      return 5;
    case AimSkill::expert:
      return 8;
    case AimSkill::perfect:
      return 10;
  }
  return 5;
}

}  // namespace

const char* trait_name(Trait t) { return kNames[static_cast<size_t>(t)]; }

Traits traits_of(AimSkill skill, Temper temper) {
  Traits t;
  t.v.fill(preset_of(skill));
  switch (temper) {
    case Temper::aggressive:
      t[Trait::aggression] = 9;
      if (t[Trait::teamplay] > 3) t[Trait::teamplay] = 3;
      break;
    case Temper::balanced:
      t[Trait::aggression] = 5;
      break;
    case Temper::coward:
      t[Trait::aggression] = 2;
      break;
  }
  return t;
}

AimSkill tier_of(uint8_t v) {
  return v <= 3 ? AimSkill::novice : v <= 6 ? AimSkill::average : AimSkill::expert;
}

Temper temper_of(const Traits& t) {
  const uint8_t a = t[Trait::aggression];
  return a >= 7 ? Temper::aggressive : a <= 3 ? Temper::coward : Temper::balanced;
}

bool selfish(const Traits& t) { return t[Trait::teamplay] <= 3; }

double scale(double v, const std::array<double, 4>& at) {
  constexpr double kAt[4] = {2, 5, 8, 10};
  for (size_t i = 0; i < 4; i++)
    if (v == kAt[i]) return at[i];
  // The segment `v` is in, or the end one beyond it.
  const size_t i = v < 5 ? 0 : v < 8 ? 1 : 2;
  const double f = (v - kAt[i]) / (kAt[i + 1] - kAt[i]);
  return at[i] + (at[i + 1] - at[i]) * f;
}

double mean_aim(const Traits& t) {
  double sum = 0;
  for (size_t i = 0; i < 5; i++) sum += t.v[i];
  return sum / 5;
}

bool apply_traits(std::string_view text, Traits& t, std::string& error) {
  size_t at = 0;
  while (at < text.size()) {
    const size_t end = std::min(text.find('/', at), text.size());
    const std::string_view kv = text.substr(at, end - at);
    at = end + 1;
    if (kv.empty()) continue;
    const size_t eq = kv.find('=');
    const std::string_view name = kv.substr(0, eq);
    int value = 0;
    if (eq != std::string_view::npos)
      for (char c : kv.substr(eq + 1)) value = c >= '0' && c <= '9' ? value * 10 + (c - '0') : -99;
    size_t k = 0;
    while (k < kTraits && name != kNames[k]) k++;
    if (k == kTraits) {
      std::string names;
      for (const char* n : kNames) names += std::string(names.empty() ? "" : ", ") + n;
      error = "no trait '" + std::string(name) + "'; the traits are " + names;
      return false;
    }
    if (eq == std::string_view::npos || value < 1 || value > 10) {
      error = "trait '" + std::string(name) + "' wants a value from 1 to 10 ('" +
              std::string(name) + "=7')";
      return false;
    }
    t.v[k] = static_cast<uint8_t>(value);
  }
  return true;
}

std::string traits_text(const Traits& t) {
  std::string out;
  for (size_t k = 0; k < kTraits; k++) {
    if (k) out += '/';
    out += kNames[k];
    out += '=';
    out += std::to_string(t.v[k]);
  }
  return out;
}

}  // namespace ge::shooter
