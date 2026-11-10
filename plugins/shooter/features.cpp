// A bot's features (features.hpp): the names, the presets and the text form.
//
// ge:summary A bot's features: the names, the presets and the text form
// ge:keywords features feature_name features_all features_none apply_features features_text
// ge:skill shooter bots
// ge:test tests/shooter_features_test.cpp
#include "shooter/features.hpp"

namespace ge::shooter {

namespace {

constexpr const char* kNames[kFeatures] = {
    "calm",       "pace",   "swimaim", "screen",   "keep",      "lead",   "lane",   "hop",
    "shortgoals", "commit", "danger",  "dodge",    "watch",     "spam",   "packs",  "talk",
    "plan",       "rj",     "gj",      "airsteer", "surface",   "glance", "eye",    "selfaim",
    "pools",      "routes", "corner",  "mouse",    "packinfer", "listen", "stairs", "locked",
    "escape",     "brink",  "gapjump", "headroom", "liphop",    "trial"};

// Every switch but `trial` (the last).
constexpr uint64_t kTrial = uint64_t{1} << (kFeatures - 1);
constexpr uint64_t kAll = kTrial - 1;

bool separator(char c) { return c == ',' || c == ' ' || c == '/' || c == '\t'; }

}  // namespace

const char* feature_name(Feature f) { return kNames[static_cast<size_t>(f)]; }

Features features_all() { return {kAll}; }
Features features_none() { return {0}; }
Features features_of(AimSkill skill) {
  Features f = features_all();
  // No skill reads a turn a whole reaction late (`eye`): it held every level's aim under people's
  // (e1m2, six seeds: average players' scattergun 34 % with it, 40 % without; people 42 %).
  f.set(Feature::eye, false);
  // Nor does any hop along a lip of lava yet (`liphop`): it raised dm2's hop share 9 -> 14 %
  // (people 36 %) with no more lava deaths, but head to head the side with it lost ~50 frags a
  // six-match set (the hop's view rides the velocity, so the side hopping more sights fewer
  // enemies). On once the hop's view sweeps the room as people's weave does.
  f.set(Feature::liphop, false);
  // A novice looks where he goes: he does not check the open ground beside him.
  if (skill == AimSkill::novice) f.set(Feature::glance, false);
  return f;
}

bool apply_features(std::string_view text, Features& f, std::string& error) {
  size_t at = 0;
  while (at < text.size()) {
    while (at < text.size() && separator(text[at])) at++;
    size_t end = at;
    while (end < text.size() && !separator(text[end])) end++;
    std::string_view word = text.substr(at, end - at);
    at = end;
    if (word.empty()) continue;
    if (word == "all") {
      f = features_all();
      continue;
    }
    if (word == "none" || word == "plain") {
      f = features_none();
      continue;
    }
    bool on = true;
    if (word[0] == '+' || word[0] == '-') {
      on = word[0] == '+';
      word.remove_prefix(1);
    }
    size_t k = 0;
    while (k < kFeatures && word != kNames[k]) k++;
    if (k == kFeatures) {
      std::string names;
      for (const char* n : kNames) names += std::string(names.empty() ? "" : ", ") + n;
      error = "no bot feature '" + std::string(word) + "' (write +name, -name, all or none); the " +
              "features are " + names;
      return false;
    }
    f.set(static_cast<Feature>(k), on);
  }
  return true;
}

std::string features_text(const Features& f) {
  const std::string trial = (f.bits & kTrial) ? ",+trial" : "";
  const uint64_t bits = f.bits & kAll;
  if (bits == kAll) return "all" + trial;
  if (bits == 0) return "none" + trial;
  std::string off = "all", on = "none";
  for (size_t k = 0; k + 1 < kFeatures; k++) {
    if (f[static_cast<Feature>(k)])
      on += std::string(",+") + kNames[k];
    else
      off += std::string(",-") + kNames[k];
  }
  return (off.size() <= on.size() ? off : on) + trial;
}

}  // namespace ge::shooter
