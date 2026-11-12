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
    "calm",      "pace",       "swimaim",    "screen",   "keep",    "lead",     "lane",
    "hop",       "shortgoals", "commit",     "danger",   "dodge",   "watch",    "spam",
    "packs",     "talk",       "plan",       "rj",       "gj",      "airsteer", "surface",
    "glance",    "eye",        "selfaim",    "pools",    "routes",  "corner",   "mouse",
    "packinfer", "listen",     "stairs",     "locked",   "escape",  "brink",    "gapjump",
    "headroom",  "liphop",     "circlejump", "ahead",    "bodies",  "beat",     "keys",
    "keysteer",  "lookway",    "press",      "slopes",   "counter", "jumpkey",  "backstep",
    "runupkeys", "gapbuild",   "curvekeys",  "underway", "recipes", "chains",
    "retarget",  "uturn",     "jumpfly",    "gappast",  "trial"};
// A name for every feature: one missing leaves the last null (and `feature_name` crashing).
static_assert(kNames[kFeatures - 1] != nullptr, "a feature has no name in kNames");

// Every switch but `trial` (the last) and those not judged or dropped (`lookway`, `press`,
// `counter`, `chains`, `retarget`). The keyboard (`keys`, `keysteer`) is on: a person's hands are on keys
// (realism first; the frags it costs, -3.8 a match head to head, won back by fixing movement and
// strategy).
constexpr uint64_t kTrial = uint64_t{1} << (kFeatures - 1);
constexpr uint64_t kKeys = (uint64_t{1} << static_cast<uint64_t>(Feature::lookway)) |
                           (uint64_t{1} << static_cast<uint64_t>(Feature::press)) |
                           (uint64_t{1} << static_cast<uint64_t>(Feature::counter)) |
                           (uint64_t{1} << static_cast<uint64_t>(Feature::chains)) |
                           (uint64_t{1} << static_cast<uint64_t>(Feature::retarget));
// `runupkeys` and `underway` wait for their judgment (`gapbuild` and `curvekeys` on: together
// +7.0 frags a match head to head on e1m2, the gap jumps landed 2.5 a match; `recipes` on: even
// head to head over 12 seeds, people's hop starts taking off at ~405 against the bots' 341;
// `uturn` on: +4.5 frags a match head to head on dm2, stops 18.8 -> 14.8 a bot-minute; `jumpfly`
// on: +1.6 on dm2 and +2.4 on e1m2 a match head to head, run-ups 147 -> 86 a match).
constexpr uint64_t kJudging = (uint64_t{1} << static_cast<uint64_t>(Feature::runupkeys)) |
                              (uint64_t{1} << static_cast<uint64_t>(Feature::underway)) |
                              (uint64_t{1} << static_cast<uint64_t>(Feature::gappast));
constexpr uint64_t kAll = (kTrial - 1) & ~kKeys & ~kJudging;

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
  // What no preset holds (`trial`, `ahead`) is written after the preset's part, when on.
  constexpr uint64_t kExtra = ~kAll & ((kTrial << 1) - 1);
  std::string extra;
  for (size_t k = 0; k < kFeatures; k++)
    if (((kExtra >> k) & 1u) && f[static_cast<Feature>(k)]) extra += std::string(",+") + kNames[k];
  const uint64_t bits = f.bits & kAll;
  if (bits == kAll) return "all" + extra;
  if (bits == 0) return "none" + extra;
  std::string off = "all", on = "none";
  for (size_t k = 0; k < kFeatures; k++) {
    if ((kExtra >> k) & 1u) continue;
    if (f[static_cast<Feature>(k)])
      on += std::string(",+") + kNames[k];
    else
      off += std::string(",-") + kNames[k];
  }
  return (off.size() <= on.size() ? off : on) + extra;
}

}  // namespace ge::shooter
