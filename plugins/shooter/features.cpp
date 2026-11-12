// A bot's features (features.hpp): the names, the presets and the text form.
//
// ge:summary A bot's features: the names, the presets and the text form
// ge:keywords features feature_name features_all features_none apply_features features_text
// ge:skill shooter bots
// ge:test tests/shooter_features_test.cpp
#include "shooter/features.hpp"

#include <initializer_list>

namespace ge::shooter {

namespace {

constexpr const char* kNames[kFeatures] = {
    "calm",      "pace",       "swimaim",    "screen",   "keep",    "lead",      "lane",
    "hop",       "shortgoals", "commit",     "danger",   "dodge",   "watch",     "spam",
    "packs",     "talk",       "plan",       "rj",       "gj",      "airsteer",  "surface",
    "glance",    "eye",        "selfaim",    "pools",    "routes",  "corner",    "mouse",
    "packinfer", "listen",     "stairs",     "locked",   "escape",  "brink",     "gapjump",
    "headroom",  "liphop",     "circlejump", "ahead",    "bodies",  "beat",      "keys",
    "keysteer",  "lookway",    "press",      "slopes",   "counter", "jumpkey",   "backstep",
    "runupkeys", "gapbuild",   "curvekeys",  "underway", "recipes", "chains",    "retarget",
    "uturn",     "jumpfly",    "gappast",    "lipjump",  "airgap",  "pilotcast", "aimhop",
    "airkeys",   "trial"};
// A name for every feature: one missing leaves the last null (and `feature_name` crashing).
static_assert(kNames[kFeatures - 1] != nullptr, "a feature has no name in kNames");

// Every switch but `trial` (the last) and those not judged or dropped (`lookway`, `press`,
// `counter`, `chains`, `retarget`). The keyboard (`keys`, `keysteer`) is on: a person's hands are
// on keys (realism first; the frags it costs, -3.8 a match head to head, won back by fixing
// movement and strategy).
constexpr Features of(std::initializer_list<Feature> list) {
  Features f;
  for (Feature x : list) f.set(x, true);
  return f;
}
constexpr Features kKeys = of({Feature::lookway, Feature::press, Feature::counter, Feature::chains,
                               Feature::retarget});
// `runupkeys` and `underway` wait for their judgment (`gapbuild` and `curvekeys` on: together
// +7.0 frags a match head to head on e1m2, the gap jumps landed 2.5 a match; `recipes` on: even
// head to head over 12 seeds, people's hop starts taking off at ~405 against the bots' 341;
// `uturn` on: +4.5 frags a match head to head on dm2, stops 18.8 -> 14.8 a bot-minute; `jumpfly`
// on: +1.6 on dm2 and +2.4 on e1m2 a match head to head, run-ups 147 -> 86 a match; `lipjump`
// on: +0.4 on dm2 and +5.9 on e1m2 a match head to head, dm2's lava deaths 14.1 -> ~11 a match;
// `airkeys` on: +4.6 on dm2 and +1.0 on e1m2 a match head to head, the hop's air no longer
// braking: -1.5 -> -0.3 units/s a command, the expert's hop gain p50 -105 -> about -10).
constexpr Features kJudging = of({Feature::runupkeys, Feature::underway, Feature::gappast,
                                  Feature::airgap, Feature::pilotcast, Feature::aimhop});
// Every feature below `trial`, less those two.
constexpr Features all_bits() {
  Features f;
  for (size_t k = 0; k + 1 < kFeatures; k++) {
    const auto x = static_cast<Feature>(k);
    f.set(x, !kKeys[x] && !kJudging[x]);
  }
  return f;
}
constexpr Features kAll = all_bits();

bool separator(char c) { return c == ',' || c == ' ' || c == '/' || c == '\t'; }

}  // namespace

const char* feature_name(Feature f) { return kNames[static_cast<size_t>(f)]; }

Features features_all() { return kAll; }
Features features_none() { return {}; }
Features features_of(AimSkill skill) {
  Features f = features_all();
  // No skill reads a turn a whole reaction late (`eye`): it held every level's aim under people's
  // (e1m2, six seeds: average players' scattergun 34 % with it, 40 % without; people 42 %).
  f.set(Feature::eye, false);
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
  // What no preset holds (`trial`, the ones judged) is written after the preset's part, when on.
  std::string extra;
  bool all = true, none = true;
  for (size_t k = 0; k < kFeatures; k++) {
    const auto x = static_cast<Feature>(k);
    if (!kAll[x]) {
      if (f[x]) extra += std::string(",+") + kNames[k];
      continue;
    }
    all = all && f[x];
    none = none && !f[x];
  }
  if (all) return "all" + extra;
  if (none) return "none" + extra;
  std::string off = "all", on = "none";
  for (size_t k = 0; k < kFeatures; k++) {
    if (!kAll[static_cast<Feature>(k)]) continue;
    if (f[static_cast<Feature>(k)])
      on += std::string(",+") + kNames[k];
    else
      off += std::string(",-") + kNames[k];
  }
  return (off.size() <= on.size() ? off : on) + extra;
}

}  // namespace ge::shooter
