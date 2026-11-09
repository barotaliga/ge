// FIGHTERS ON THE BASELINE MOD (games/shooters_edge/bots/fighter.hpp, fight_run.hpp): four
// fighters (two expert aims, two novice) play the baseline mod's deathmatch on the arena for two
// minutes, on four seeds. Every one scores and dies, picks items up, switches weapons and holds one
// better than the starting scattergun for a while, and over the four seeds spends time in each of
// its four modes; the mod never faults; the experts outscore the novices over the four seeds (one
// seed is a coin toss); aggressive novices fight more than cowardly ones, who back off
// (shooter/temper.hpp); and a match replays to the bit and matches its golden
// (tests/golden/shooters_edge_fight.txt, GE_GOLDEN_WRITE=1 rewrites it; the PR says why).
//
// ge:summary Tests: fighters play the baseline mod's deathmatch: score, items, modes, skill, bits
// ge:keywords test fighter deathmatch frags deaths items weapon modes skill golden replay
// ge:skill shooter bots testing
#include <array>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <map>
#include <string>
#include <vector>

#include "check.hpp"
#include "core/parallel.hpp"
#include "golden.hpp"
#include "shooters_edge/bots/fight_run.hpp"

namespace sh = ge::shooter;
namespace fb = shooters_edge::bots;

namespace {

constexpr const char* kGolden = "tests/golden/shooters_edge_fight.txt";

bool env_set(const char* name) {
  const char* v = std::getenv(name);
  return v != nullptr && v[0] != '\0' && v[0] != '0';
}

fb::FightOptions duel_of_skills(uint32_t seed, int frames) {
  fb::FightOptions o;
  o.root = GE_SOURCE_DIR;
  o.frames = frames;
  o.seed = seed;
  o.skills = {sh::AimSkill::expert, sh::AimSkill::novice, sh::AimSkill::expert,
              sh::AimSkill::novice};
  return o;
}

}  // namespace

TEST(fighters_play_the_baseline_mod_and_aim_tells) {
  float expert = 0, novice = 0;
  // Each player's commands in each mode over the four seeds: a balanced novice goes after no fresh
  // enemy (shooter/temper.hpp), so in one two-minute match he may never chase.
  std::map<std::string, std::array<int64_t, 4>> modes;
  std::map<std::string, float> frags;      // each player's over the four seeds
  std::map<std::string, int64_t> pickups;  // the same
  for (uint32_t seed = 1; seed <= 4; seed++) {
    const fb::FightReport r = fb::run_fight(duel_of_skills(seed, 9000));
    CHECK_MSG(r.ok, r.error);
    if (!r.ok) return;
    CHECK_MSG(r.faults == 0, "the baseline mod faulted under the fighters");
    for (const fb::FightPlayer& p : r.players) {
      const std::string name = p.name + " seed " + std::to_string(seed);
      const fb::FighterStats& s = p.stats;
      // Every seed he scores and dies; three frags a seed is held over the four (a novice's one
      // two-minute match is a coin toss: 2 frags in one seed is no fault).
      CHECK_MSG(p.frags >= 1 && p.deaths >= 3, name + " scored " + std::to_string(p.frags) +
                                                   " and died " + std::to_string(p.deaths) +
                                                   " times in two minutes");
      frags[p.name] += p.frags;
      // Every seed he picks something up; two a seed is held over the four (an expert in a
      // fight most of a two-minute match took 1 to 8 a seed, 36 to 50 in four).
      CHECK_MSG(s.pickups >= 1, name + " picked up " + std::to_string(s.pickups) + " items");
      pickups[p.name] += s.pickups;
      CHECK_MSG(s.switches >= 3,
                name + " switched weapons " + std::to_string(s.switches) + " times");
      int64_t better = 0;
      for (int w = 3; w <= 8; w++) better += s.weapon_commands[w];
      CHECK_MSG(better >= 200, name + " never held a weapon better than the scattergun for long");
      for (int m = 0; m < 4; m++) modes[p.name][static_cast<size_t>(m)] += s.mode_commands[m];
      (p.skill == sh::AimSkill::expert ? expert : novice) += p.frags;
    }
    std::printf("seed %u: experts %.0f novices %.0f so far\n", seed, expert, novice);
  }
  for (const auto& [name, n] : frags)
    CHECK_MSG(n >= 12, name + " scored " + std::to_string(n) + " in four seeds (under 3 a seed)");
  for (const auto& [name, n] : pickups)
    CHECK_MSG(n >= 8,
              name + " picked up " + std::to_string(n) + " items in four seeds (under 2 a seed)");
  for (const auto& [name, m] : modes)
    for (size_t k = 0; k < 4; k++)
      CHECK_MSG(m[k] > 0, name + " never entered " +
                              fb::fight_mode_name(static_cast<fb::FightMode>(k)) +
                              " in four seeds");
  CHECK_MSG(expert > novice, "over four seeds the experts scored " + std::to_string(expert) +
                                 ", the novices " + std::to_string(novice));
}

TEST(aggressive_novices_fight_more_than_cowardly_ones) {
  // Four novices, two aggressive and two cowards, on twelve seeds: the aggressive are in a fight
  // far more of their time; the cowards spend two fifths or more of theirs backing
  // off. Not who dies more: in this small hall a coward is run down about as often as an aggressive
  // novice is shot in his fights (12 seeds: 249 deaths against 253, each side ahead in half the
  // seeds; backing off to cover, or fighting back nearer, did not change it).
  int64_t deaths[2] = {}, frags[2] = {}, fight[2] = {}, alive[2] = {}, turned[2] = {};
  // Twelve seeds, played side by side: a frag count over four swings by block (seeds 1-4 gave the
  // cowards 78 to 72, all twelve 241 to 215 the other way).
  constexpr uint32_t kSeeds = 12;
  std::vector<fb::FightReport> reports(kSeeds);
  ge::core::parallel_bands(kSeeds, kSeeds, [&](size_t lo, size_t hi) {
    for (size_t i = lo; i < hi; i++) {
      fb::FightOptions o = duel_of_skills(static_cast<uint32_t>(i + 1), 9000);
      o.skills.assign(4, sh::AimSkill::novice);
      o.tempers = {sh::Temper::aggressive, sh::Temper::aggressive, sh::Temper::coward,
                   sh::Temper::coward};
      reports[i] = fb::run_fight(o);
    }
  });
  for (const fb::FightReport& r : reports) {
    CHECK_MSG(r.ok, r.error);
    if (!r.ok) return;
    for (const fb::FightPlayer& p : r.players) {
      const int k = p.temper == sh::Temper::aggressive ? 0 : 1;
      deaths[k] += p.deaths;
      frags[k] += static_cast<int64_t>(p.frags);
      fight[k] += p.stats.mode_commands[static_cast<int>(fb::FightMode::fight)];
      turned[k] += p.stats.mode_commands[static_cast<int>(fb::FightMode::retreat)];
      for (int m = 0; m < 4; m++) alive[k] += p.stats.mode_commands[m];
    }
  }
  std::printf(
      "aggressive: %lld frags, %lld deaths, fight %.2f; cowards: %lld frags, %lld deaths, fight "
      "%.2f, retreat %.2f\n",
      static_cast<long long>(frags[0]), static_cast<long long>(deaths[0]),
      static_cast<double>(fight[0]) / static_cast<double>(alive[0]),
      static_cast<long long>(frags[1]), static_cast<long long>(deaths[1]),
      static_cast<double>(fight[1]) / static_cast<double>(alive[1]),
      static_cast<double>(turned[1]) / static_cast<double>(alive[1]));
  // Who scores more is not claimed: with a person's hand on the mouse (ai/mouse.hpp, a novice's
  // flick landing within about 45 degrees) a novice's eagerness no longer pays in frags (12
  // seeds: 197 to the cowards' 209; with the old spring's aim 241 to 215 the other way).
  // Twice, not more: an aggressive player stands and fights within the double's range rather than
  // breaking off, so he corners the cowards (who must fight one that near) more often.
  CHECK_MSG(fight[0] * alive[1] > 2 * fight[1] * alive[0],
            "the aggressive novices were not in a fight twice the cowards' share");
  CHECK_MSG(turned[1] * 5 > alive[1] * 2,
            "the cowards spent less than two fifths of their time backing off");
}

TEST(a_fighters_match_replays_to_the_bit_and_matches_its_golden) {
  uint64_t a = 0, b = 0;
  for (int run = 0; run < 2; run++) {
    const fb::FightReport r = fb::run_fight(duel_of_skills(3, 3000));
    CHECK_MSG(r.ok, r.error);
    (run == 0 ? a : b) = r.final_digest;
  }
  CHECK_MSG(a == b, "two fighters' matches on one seed differ");
  if (env_set("GE_GOLDEN_WRITE")) {
    std::ofstream out(ge::test::root_path(kGolden));
    out << "# games/shooters_edge/bots/fight_run.hpp: four fighters (expert, novice, expert, "
           "novice) on the baseline mod, seed 3, 3000 frames: the last frame's digest "
           "(tests/shooters_edge_fight_test.cpp)\n";
    char line[32];
    std::snprintf(line, sizeof line, "%016llx\n", static_cast<unsigned long long>(a));
    out << line;
    std::printf("wrote %s\n", kGolden);
    return;
  }
  const auto rows = ge::test::golden_rows(kGolden);
  CHECK_MSG(rows.size() == 1 && rows[0].size() == 1,
            std::string(kGolden) + " is missing or malformed: write it with GE_GOLDEN_WRITE=1");
  if (rows.size() == 1 && rows[0].size() == 1)
    CHECK_MSG(rows[0][0] == a,
              "the fighters' match moved off its golden bits: if the fighter, the bot, the mod or "
              "the move changed on purpose, rewrite with GE_GOLDEN_WRITE=1 and say why in the PR");
}
