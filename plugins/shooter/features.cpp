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
    "calm",       "pace",       "swimaim",    "screen",   "keep",      "lead",       "lane",
    "hop",        "shortgoals", "commit",     "danger",   "dodge",     "watch",      "spam",
    "packs",      "talk",       "plan",       "rj",       "gj",        "airsteer",   "surface",
    "glance",     "eye",        "selfaim",    "pools",    "routes",    "corner",     "mouse",
    "packinfer",  "listen",     "stairs",     "locked",   "escape",    "brink",      "gapjump",
    "headroom",   "liphop",     "circlejump", "ahead",    "bodies",    "beat",       "keys",
    "keysteer",   "lookway",    "press",      "slopes",   "counter",   "jumpkey",    "backstep",
    "runupkeys",  "gapbuild",   "curvekeys",  "underway", "recipes",   "chains",     "retarget",
    "uturn",      "jumpfly",    "gappast",    "lipjump",  "airgap",    "pilotcast",  "aimhop",
    "airkeys",    "airview",    "chainhop",   "airwall",  "landwall",  "circlekeys", "circleview",
    "circleaim",  "leadwall",   "leadmean",   "leadarc",  "airwalk",   "airarc",     "hopjump",
    "hopside",    "noback",     "levelview",  "hopopen",  "fightway",  "letgo",      "paceback",
    "retreatrun", "hide",       "nogoal",     "tripkeep", "routetime", "fetch",
    "pokestand",  "liftbrake",  "scriptfresh", "scriptentry",
    "calmvel",    "lookturn",   "viewbook",   "trial"};
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
constexpr Features kKeys =
    of({Feature::lookway, Feature::press, Feature::counter, Feature::chains, Feature::retarget});
// `runupkeys` and `underway` wait for their judgment (`gapbuild` and `curvekeys` on: together
// +7.0 frags a match head to head on e1m2, the gap jumps landed 2.5 a match; `recipes` on: even
// head to head over 12 seeds, people's hop starts taking off at ~405 against the bots' 341;
// `uturn` on: +4.5 frags a match head to head on dm2, stops 18.8 -> 14.8 a bot-minute; `jumpfly`
// on: +1.6 on dm2 and +2.4 on e1m2 a match head to head, run-ups 147 -> 86 a match; `lipjump`
// on: +0.4 on dm2 and +5.9 on e1m2 a match head to head, dm2's lava deaths 14.1 -> ~11 a match;
// `airkeys` on: +4.6 on dm2 and +1.0 on e1m2 a match head to head, the hop's air no longer
// braking: -1.5 -> -0.3 units/s a command, the expert's hop gain p50 -105 -> -0.7; `aimhop` on:
// -2.9 on dm2 and +1.3 on e1m2 a match head to head, the air share 0.33 -> 0.44 (people 0.46);
// `airview` on: +1.7 on dm2 and +4.8 on e1m2 a match head to head; `airwall` on: even head to
// head, +0.05 on dm2 and -0.4 on e1m2, hop flights colliding 36 -> 31 %; `chainhop` on: +4.5 on
// dm2 and +2.4 on e1m2 a match head to head, the side with it faster; `leadwall` on: even head
// to head, +0.4 on dm2 and 0.0 on e1m2, the rocket drill's direct hits on dm2 11.8 -> 12.5 %;
// `airwalk` on: +3.2 on dm2 and +1.4 on e1m2 a match head to head, the side with it 3-4 units/s
// faster, a chained hop's landing 176 -> 257 units/s on dm2; `hopjump` and `hopside` on: +2.3 on
// dm2 and +0.3 on e1m2 a match head to head, faster, the landing's jump on its first ground frame
// 2 -> 33 %, no forward key in a hop: a player's bunny hop; `noback` and `levelview` on: +0.8 on
// dm2 and +1.0 on e1m2 a match head to head, looks past 45 degrees down 4.6 -> 1.9 %, people's;
// `paceback` on: -2.9 on dm2 and +4.3 on e1m2 a match head to head, a lost aim back on the way by
// the hand's stroke, not creeping: backwards 20.6 -> 18.4 % by one rule, people 13.8; `hide` on:
// +5.0 on dm2 and -0.8 on e1m2 a match head to head, a launcher owner's weapon in hand at his
// death near people's: dm2 77 / 22 % launcher / scattergun, people 72 / 25; `pokestand` on:
// -0.2 on dm2 and -0.2 on e1m2 a match head to head, no more running round under a button it
// fires at without a shot; `scriptfresh` on: a bug fixed, +1.7 / -3.7 on e1m2 a match head to
// head (mean -1.0, inside the sides' noise), dm2 alike (no scripted way), the YA's climb no longer
// resumed at the walkway's step from the floor).
constexpr Features kJudging = of(
    {Feature::runupkeys, Feature::underway, Feature::gappast, Feature::airgap, Feature::pilotcast,
     Feature::landwall, Feature::circlekeys, Feature::circleview, Feature::circleaim,
     Feature::leadmean, Feature::leadarc, Feature::airarc, Feature::hopopen, Feature::fightway,
     Feature::letgo, Feature::retreatrun, Feature::nogoal, Feature::tripkeep, Feature::routetime,
     Feature::fetch, Feature::liftbrake, Feature::scriptentry, Feature::calmvel, Feature::lookturn,
     Feature::viewbook});
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
