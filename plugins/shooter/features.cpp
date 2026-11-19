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
    "calm",       "pace",       "swimaim",     "screen",       "keep",      "lead",
    "lane",       "hop",        "shortgoals",  "commit",       "danger",    "dodge",
    "watch",      "spam",       "packs",       "talk",         "plan",      "rj",
    "gj",         "airsteer",   "surface",     "glance",       "eye",       "selfaim",
    "pools",      "routes",     "corner",      "mouse",        "packinfer", "listen",
    "stairs",     "locked",     "escape",      "brink",        "gapjump",   "headroom",
    "liphop",     "circlejump", "ahead",       "bodies",       "beat",      "keys",
    "keysteer",   "lookway",    "press",       "slopes",       "counter",   "jumpkey",
    "backstep",   "runupkeys",  "gapbuild",    "curvekeys",    "underway",  "recipes",
    "chains",     "retarget",   "uturn",       "jumpfly",      "gappast",   "lipjump",
    "airgap",     "pilotcast",  "aimhop",      "airkeys",      "airview",   "chainhop",
    "airwall",    "landwall",   "circlekeys",  "circleview",   "circleaim", "leadwall",
    "leadmean",   "leadarc",    "airwalk",     "airarc",       "hopjump",   "hopside",
    "noback",     "levelview",  "hopopen",     "fightway",     "letgo",     "paceback",
    "retreatrun", "hide",       "nogoal",      "tripkeep",     "routetime", "fetch",
    "pokestand",  "liftbrake",  "scriptfresh", "scriptentry",  "calmvel",   "lookturn",
    "viewbook",   "gapturn",    "gapnear",     "placeheard",   "gaplava",   "brinkline",
    "sidebrink",  "fallseen",   "liplook",     "brinkkeys",    "floortrap", "trapflee",
    "bridgeseen", "sidehop",    "lostdry",     "liftoff",      "shotjump",  "rjready",
    "jumpcuts",   "brinkfall",  "aimland",     "starthead",    "airturn",   "startclear",
    "gaphold",    "keyswap",    "avghop",      "hopsee",       "hopturn",   "hoparc",
    "hopland",    "hoppitch",   "hoprhythm",   "aimstrafe",    "walkmash",  "giveup",
    "stairbump",  "mashstart",  "aimground",   "straightfly",  "midroute",  "noroute",
    "itemfirst",  "rushhold",   "quadlead",    "bridgebutton", "rushrun",   "quadseen",
    "doorway",    "trapbutton", "trappress",   "blastsafe",    "boregive",  "detour",
    "trial"};
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
// resumed at the walkway's step from the floor); `gapnear` on: +6.0 on dm2 (+4.2 / +7.8) and -0.2
// on e1m2 a match head to head, e1m2's gaps given up 75 -> 63 a match for half the field;
// `placeheard` on: a heard place read as the room round the speaker, not the middle of every point
// of its name in a loc file: -1.25 on dm2 (-7.8 / +5.2) and +0.5 on e1m2 a match head to head;
// `brinkkeys` on: +6.2 on dm2 (+11.1 / +1.3) a match head to head, lava deaths 12.75 -> 9.0 a match
// with both sides on it, frags a minute 9.33 -> 10.71 (e1m2 has no lava or slime: no change);
// `gaplava` on (over `brinkkeys`): even head to head on dm2 (-4.7 / +4.8), dm2's gap tricks over
// lava jumped 0 -> ~0.8 a match a side (about half landed; people ~7), lava deaths 9.0 -> 9.1-9.7;
// `floortrap` on: +5.0 on dm2 (-4.8 / +14.8) a match head to head, ~2.5 presses a match each
// firing, the enemy's lava deaths +2.5 / +3.7 a match and the presser's own +0.25 / -0.9;
// `bridgeseen` on: +7.4 on dm2 (+7.9 / +6.9) and -0.3 on e1m2 a match head to head, the side with
// it dying in lava 8.8 -> 5.6 and 7.2 -> 5.2 a match (the shut links onto a bridge still in were
// never honoured); `trapflee` on (over `bridgeseen`): -2.7 on dm2 (+2.1 / -7.4, inside the sides'
// noise; it runs ~2 times a match) a match head to head, the side with it in lava 6.4 -> 4.5 and
// 5.4 -> 5.0: a person runs off a floor going from under him. `liftoff`, `shotjump` and `rjready`
// on together: even head to head (dm2 -1.9: +3.3 / -7.0; e1m2 +2.7, where no rocket jump is made),
// dm2's south pit falls 16 -> 3 over 8 seeds, its lava deaths 9.5 -> 7.7-8.9 a match: the flips
// that burst under a player still on the ground (the jump key late) or with another weapon in
// hand (no blast) were a third of every rocket jump asked for. `brinkfall` on: +0.9 on dm2 (+2.1 /
// -0.2) a match head to head; with both sides on it lava deaths 10.4 -> 6.75 a match (8 traced
// seeds: the YA stair's 33 -> 3, the RA pit's 11 -> 3), frags a minute 9.38 -> 10.26, speed
// 227.9 -> 231.6 (e1m2 has no lava or slime: no change). `aimland` on: -3.6 on dm2 (-6.1 / -1.1)
// and +1.2 on e1m2 (+6.8 / -4.3) a match head to head, inside the sides' noise; with both sides on
// it dm2's fast hop flights meet a wall 60.2 -> 57.0 % (people 27 %), speed even. `starthead` on:
// -2.3 on dm2 (-16.7 / +12.0) and +0.4 on e1m2 (-2.3 / +3.2) a match head to head, inside the
// sides' noise; with both sides on it dm2's expert fast flights 698 -> 788, 320+ 24.9 -> 25.4 %
// of the time (people 38), p95 407 -> 413, walls 59 -> 57 %. `airturn` on: +2.7 on dm2 (-6.8 /
// +12.1) and +3.9 on e1m2 (+2.2 / +5.6) a match head to head; with both sides on it dm2's expert
// fast flights meet a wall 59 -> 53 % (people 27), the speed a fast flight keeps -126 -> -104, the
// air turning 150+ deg/s 20 -> 26 % of the fast air (people 37). `startclear` on: -3.8 on dm2
// (-3.4 / -4.2) and -2.0 on e1m2 (+0.7 / -4.7) a match head to head, inside the sides' noise;
// with both sides on it dm2's expert fast flights meet a wall 54 -> 49 % and keep -111 -> -67 of
// their speed (people 27 % and -1), 320+ even. `gaphold` on: +4.3 on dm2 (+1.9 / +6.7) and +1.8 on
// e1m2 (-3.2 / +6.8) a match head to head; with both sides on it dm2's experts at 320+ 25.8 -> 26.5
// % of the time, fast flights 790 -> 852 (walls 49 -> 53 %). `avghop` on: averages on dm2 -1.4 a
// match head to head (-5.0 / +2.2), speed even; the hop course's average lap 35.0 -> 25.3 s, walls
// 9.3 -> 4.5, no key in the air 36 -> 3 %: the maintainer's hop. `hopsee` on: experts on dm2 -2.3
// a match head to head (-1.3 / -3.2); with both sides on it fast hop flights meet a wall 53.9 ->
// 50.9 % and keep -114 -> -82 units/s of their speed, speed 233.9 -> 235.0. `hopturn` and `hoparc`
// on together: +5.8 on dm2 a match head to head (+2.7 / +8.8); with both sides on it the air turns
// 97 -> 117 degrees a second at 400-499 units/s (people 124), fast hop flights 132 -> 154 a match
// keeping -105 -> -58 of their speed, walls 53 -> 47 %, 320+ even (23.7 -> 23.5 %).
// `aimstrafe` on: +1.0 on dm2 a match head to head (+0.4 / +1.6); with both sides on it no key in a
// hop's air under the aim 64 -> 26 %, its velocity curving with the aim's turn 7.9 -> 13.2 % of
// the time (people 51 %), fast hop flights 103 -> 112 a set, speed 239.7 -> 243.1 (12 seeds).
// `walkmash` on: +3.7 on dm2 a match head to head (+8.8 / -1.4); with both sides on it (2on2, 12
// seeds) speed 240.7 -> 248.9, 320+ 28.0 -> 30.7 % of the time, walks (0.4 s+ on the ground) p50
// 239 -> 264 units/s (people 322).
// `giveup` on: +1.5 on dm2 a match head to head (+4.3 / -1.4); with both sides on it (2on2, 12
// seeds) the run-ups at jumps 67 106 -> 39 606 commands (a flip to the quad was lined up for 40-90
// s on end), 320+ 30.7 -> 31.4 % of the time, walks p50 264 -> 281 units/s. `stairbump` on: -0.4 on
// dm2 a match head to head (-5.0 / +4.2); with both sides on it (2on2, 12 seeds) speed 247.4 ->
// 251.4, 320+ 31.4 -> 32.0 % of the time, climbs at 360+ 2.8 -> 3.2 a minute. `mashstart` on: -0.6
// on dm2 a match head to head over 24 seeds (+1.1 / -7.0 on 1-12, -2.7 / +6.3 on 13-24); with both
// sides on it (2on2, 12 seeds) speed 251.4 -> 256.6, 320+ 32.0 -> 33.3 % of the time, walks p50 281
// -> 292, accuracy 31.4 -> 34.5 %.
// `circleview` on (re-judged): +1.8 on dm2 a match head to head (+2.4 / +1.2); with both sides on
// it (4on4, 12 seeds) own-view circle takeoffs p50 339 -> 371 (p75 355 -> 416), 400+ 2.5 -> 3.1 %
// of the time, fast flights 9.4 -> 9.9 a bot-minute.
// `aimground` on: +1.8 on dm2 a match head to head (+0.6 / +2.9); with both sides on it (4on4, 12
// seeds) speed 246.2 -> 248.1, 320+ 32.2 -> 33.7 % of the time, 400+ 3.1 -> 3.4 %, flights 22.2 ->
// 19.8 a minute (people 14.1), flying backwards 6.0 -> 5.0 % (people 3.1).
// `aimhop` OFF (re-judged 2026-11-19): -aimhop +3.4 on dm2 a match head to head (+2.9 / +3.8); with
// both sides off (4on4, 12 seeds) speed 248.1 -> 253.3, 320+ 33.7 -> 37.0 % of the time, flights
// 19.8 -> 17.0 a minute (people 14.1) each losing less, flying backwards 5.0 -> 3.4 % (people 3.1),
// walks p50 296 -> 309 (people 307).
// `straightfly` on: +4 on dm2 a match head to head over 24 seeds (+13.6 / -1.6 on 1-12, +0.2 /
// +3.9 on 13-24); with both sides on it (4on4, 12 seeds) the mouse in a straight corridor turns 23
// degrees a second (people 25, before 71), speed 253.3 -> 251.8 (within a set's noise).
// `midroute` on (a share of 0.8): +2.1 on dm2 a match head to head over 24 seeds (+3.4 / +4.6 on
// 1-12, -5.9 / +6.4 on 13-24), a side's speed +5 with it; with both sides on it (4on4, 12 seeds)
// speed 251.8 -> 258.3, 400+ 4.3 -> 5.2 % of the time (people 14.5), fast takeoffs meeting a wall
// 45 -> 41 %, walks p50 307 -> 316.
// `itemfirst` on: head to head even over 24 seeds (-9.1 / +8.2 on 1-12, +3.6 / -1.0 on 13-24);
// with both sides on it (4on4, 12 seeds) the red armour lying 82 -> 77 % (people 19), the
// launcher 46 -> 39 % (people 10), the first launcher's area open 19 -> 15 %, the hub 37.5 ->
// 34.6 % of the time (people 20), chases 21 -> 11 % of it, speed even; the quad 56 -> 62 %
// (more bots near it and not taking it: its last steps, next).
// `rushhold` on: with both sides on it (4on4, 12 seeds, itemfirst on) the quad lying 62 -> 47 %
// (people 4), the armour and the launcher even; head to head -1.2 a match over 24 seeds (+8.0 /
// -6.2 on 1-12, -1.7 / -5.0 on 13-24): within a set's noise, kept for the quad's control.
// `quadlead` on (the maintainer's rule: a powerup's area is worth holding only from 15 s before
// it is back): head to head -0.3 a match over 24 seeds; four 20-minute matches with both sides on
// it: the quad lying 45 -> 48 %, held in its window 9 -> 6 % (people 21), within the noise.
// `bridgebutton` on (the maintainer's way into dm2's red armour's ledge: the button by the lava
// opens a floor): four 20-minute matches with both sides on it, the ledge RA held 0 -> 49 %
// (people 63), its mega 0 -> 43 % (47), three-a-side brawls 26.6 -> 13.0 % of the time near an
// enemy (9.5), the time by cell's distance from people's 24 -> 13 %; frags a minute 15.9 ->
// 12.4 (fewer brawls), lava deaths 4.75 -> 6.25; head to head -4.0 / -10.3 (1-12), -2.9 / +0.5
// (13-24): -4.2 a match, kept for how much nearer people it plays.
// `rushrun` on (the quad's rushers by their run there, not across): sixteen 20-minute matches
// with both sides on it, the quad lying 45.5 -> 38.3 % (people 4), taken 11.3 -> 12.6 a match,
// its median wait 36 -> 23 s; head to head -0.7 / -1.8 (1-12): -1.25 a match, within noise.
// `quadseen` on (an enemy's quad glow seen: the quad gone, back no sooner than 30 s on, the team
// told): the bots' time believing the quad there while it is gone 42 -> 27 %; the quad lying even
// (38.3 -> 37.6 %, sixteen 20-minute matches); head to head +8.2 / -1.9 (1-12): +3.1 a match.
// `doorway` on (a door keeps an item only when firing it opens the way and no way goes round
// its button's doors): dm2's pillar room YAs, read as behind the crushing walls, taken 1.3 / 2.0
// -> 6.6 / 7.8 a team in 20 minutes (people 17 / 18), no armour 71 -> 65 % of the time (people
// 48); head to head +4.0 / +2.7 on dm2 and -0.8 / +2.3 on e1m2 (1-12): +3.3 and +0.75 a match.
// The pillar room's lava deaths 2.0 -> 4.9 a match (its floor drops at the buttons by the YAs).
// `trapbutton` on (the nav's links through a floor trap's touch button shut unless pressing it):
// eight 20-minute dm2 matches, the buttons touched 9.0 -> 7.8 a match (people 1.0), the floor
// dropped 14.5 -> 13, lava deaths 12 -> 11.6, frags a minute 12.2 -> 12.9; head to head +0.9 /
// +2.6 (1-12): +1.75 a match.
// `trappress` kept on (the press on an enemy over a floor trap, split out of `floortrap`): with it
// off the floor dropped 1.5 times a match (people's touches 1.0) and lava deaths 6.9, but head to
// head the side with it +9.7 / +2.9 (1-12): +6.3 a match, too dear to drop.
// `blastsafe` on (no rocket or grenade bursts within 160 units of its shooter: the trigger and the
// finger's press held, another weapon taken up with the enemy that near, the spam's shots too):
// eight 20-minute dm2 matches, the bots' own blasts 31.5 -> 7.5 deaths a match (people's own
// deaths 2.2 per 100 kills), the launcher's carriers dying 2.74 -> 1.75 a minute alive (the rest
// 2.24; people 1.13 and 4.13), a launcher kept 31 -> 38.5 s from the low one, frags a minute 12.9
// -> 13.2; head to head +16.7 / +13.7 (1-12): +15.2 a match on dm2, +4.6 on e1m2.
// `boregive` on (the maintainer's bore: only for a mate with better use of the launcher when it
// matters: the quad's or now the pent's carrier, or a better player while a key area just lost
// can be taken back): bores 1.25 a 20-minute match on dm2 (unchanged, bit for bit: the lost-area
// handoff is rare), 2.25 -> 2.4 on dm3 (the pent's); head to head on dm3 0.0 / +0.1: even.
constexpr Features kJudging =
    of({Feature::runupkeys,   Feature::underway,  Feature::gappast,    Feature::airgap,
        Feature::pilotcast,   Feature::landwall,  Feature::circlekeys, Feature::circleaim,
        Feature::leadmean,    Feature::leadarc,   Feature::airarc,     Feature::hopopen,
        Feature::fightway,    Feature::letgo,     Feature::retreatrun, Feature::nogoal,
        Feature::tripkeep,    Feature::routetime, Feature::fetch,      Feature::liftbrake,
        Feature::scriptentry, Feature::calmvel,   Feature::lookturn,   Feature::viewbook,
        Feature::gapturn,     Feature::brinkline, Feature::sidebrink,  Feature::fallseen,
        Feature::liplook,     Feature::sidehop,   Feature::lostdry,    Feature::jumpcuts,
        Feature::hopland,     Feature::hoppitch,  Feature::aimhop,     Feature::noroute,
        Feature::detour});
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
