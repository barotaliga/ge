// A CLASSIC GAME'S WORDS AS DATA: what its server prints, as templates (server/words.hpp's marks:
// %1 the first name or number, %g1 its digits gold, ^red^, %[ %] the gold brackets, %. the dot,
// %l a rule, %n a newline),
// one table a game names by its profile row (game_profile.hpp). The window and the dedicated
// server put each in the server's local info (`word_lines`), where the baseline mod's `words`
// builtin and the lobby's announcer (server/announce.hpp) read them. A game with no table, or
// Shooter's Edge's own content, speaks the mod's and the lobby's own words.
//
// THE KEYS, and the arguments each is given:
// - obituaries `obit_*` (killed by another player), `tk_*` (by a teammate), `self_*` (by oneself),
//   `world_*` (by the world), `pent_*` (a telefrag against the shield): %1 the victim, %2 the
//   killer, %3 the victim's pronoun (his or her), %4 the killer's. A key with a number on the end
//   is one of several variants, `_1` to `_n`, one drawn at random; one without has a single form.
// - `left`: %1 the name, %2 the frags (said only in the match: `quiet_warmup`).
// - `quiet_warmup` (any text): no obituary and no leaving line before the match starts, nothing
//   scored there either.
// - the clock: `clock_minutes` / `clock_minute` (%1 the minutes left), `clock_seconds` /
//   `clock_second` (%1 the seconds left, at 30, 15 and 10 to 1), `clock_lead` (%1 the team ahead,
//   %2 its lead, `clock_lead_one` a lead of 1), `clock_tie`; `match_over`; `team_scores_head`,
//   `team_scores_row` (%1 the team, %2 its frags, %3 its share), `team_scores_tail`.
// - powers wearing off, 3 s before the end: `power_quad_off`, `power_pent_off`.
// - the lobby (server/announce.hpp): `ready` (%1 the name), `ready_team` (%1 the name, %2 the
//   team), `unready`, `stops`, `all_ready`, `timer`, `aborting`, `begun`, `ready_again` (to a
//   ready player saying ready), `get_ready` (to one not ready in the last 5 s); the countdown's
//   centre print, a line each: `count_head` (%1 the seconds), `count_dm`, `count_mode` (%1 the
//   mode's word, `mode_duel`, `mode_team` or `mode_ffa`), `count_antilag`, `count_teamplay`,
//   `count_timelimit`, `count_fraglimit`, `count_powerups` (%1 `on`), `count_tail`; the sounds
//   `sound_timer` (the countdown begins) and `sound_tick` (each of its last 5 s).
//
// ge:summary A classic game's server words as templates: obituaries, the clock, the lobby, the count
// ge:keywords words classic-words obituary obituaries kill-message death-message countdown
// ge:keywords centerprint ready break match-over minutes-remaining word_lines kclassicserverwords
// ge:skill shooter
// ge:test tests/shooters_edge_words_test.cpp
#pragma once

#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace shooters_edge::profiles {

struct WordRow {
  std::string_view key;
  std::string_view text;  // a template (server/words.hpp)
};

inline constexpr WordRow kClassicServerWords[] = {
    // --- killed by another player ---
    {"obit_axe", "%1 was ax-murdered by %2%n"},
    {"obit_sg", "%1 chewed on %2's boomstick%n"},
    {"obit_sg_gib", "%1 was lead poisoned by %2%n"},
    {"obit_ssg", "%1 ate 2 loads of %2's buckshot%n"},
    {"obit_ssg_quad", "%1 ate 8 loads of %2's buckshot%n"},
    {"obit_ng_1", "%1 was body pierced by %2%n"},
    {"obit_ng_2", "%1 was nailed by %2%n"},
    {"obit_sng_1", "%1 was punctured by %2%n"},
    {"obit_sng_2", "%1 was perforated by %2%n"},
    {"obit_sng_3", "%1 was ventilated by %2%n"},
    {"obit_sng_gib", "%1 was straw-cuttered by %2%n"},
    {"obit_gl", "%1 eats %2's pineapple%n"},
    {"obit_gl_gib", "%1 was gibbed by %2's grenade%n"},
    {"obit_rl", "%1 rides %2's rocket%n"},
    {"obit_rl_gib", "%1 was gibbed by %2's rocket%n"},
    {"obit_rl_quad_1", "%1 was brutalized by %2's quad rocket%n"},
    {"obit_rl_quad_2", "%1 was smeared by %2's quad rocket%n"},
    {"obit_rl_quad_3", "%2 rips %1 a new one%n"},
    {"obit_lg", "%1 accepts %2's shaft%n"},
    {"obit_lg_gib", "%1 gets a natural disaster from %2%n"},
    {"obit_discharge_1", "%1 drains %2's batteries%n"},
    {"obit_discharge_2", "%1 accepts %2's discharge%n"},
    {"obit_tele", "%1 was telefragged by %2%n"},
    {"obit_squish", "%2 squishes %1%n"},
    {"obit_other", "%1 killed by %2 ?%n"},
    // --- killed by a teammate ---
    {"tk_tele", "%1 was telefragged by %3 teammate%n"},
    {"tk_squish", "%2 squished a teammate%n"},
    {"tk_1", "%2 checks %4 glasses%n"},
    {"tk_2", "%2 loses another friend%n"},
    {"tk_3", "%2 gets a frag for the other team%n"},
    {"tk_4", "%2 mows down a teammate%n"},
    // --- killed by oneself ---
    {"self_gl", "%1 tries to put the pin back in%n"},
    {"self_rl_1", "%1 discovers blast radius%n"},
    {"self_rl_2", "%1 becomes bored with life%n"},
    {"self_discharge_slime", "%1 discharges into the slime%n"},
    {"self_discharge_lava", "%1 discharges into the lava%n"},
    {"self_discharge_water_1", "%1 heats up the water%n"},
    {"self_discharge_water_2", "%1 discharges into the water%n"},
    {"self_squish", "%1 was squished%n"},
    {"self_kill", "%1 suicides%n"},
    {"self_other", "%1 somehow becomes bored with life%n"},
    // --- killed by the world ---
    {"world_fall_1", "%1 cratered%n"},
    {"world_fall_2", "%1 fell to %3 death%n"},
    {"world_drown_1", "%1 sleeps with the fishes%n"},
    {"world_drown_2", "%1 sucks it down%n"},
    {"world_slime_1", "%1 gulped a load of slime%n"},
    {"world_slime_2", "%1 can't exist on slime alone%n"},
    {"world_lava_1", "%1 turned into hot slag%n"},
    {"world_lava_2", "%1 visits the Volcano God%n"},
    {"world_lava_gib", "%1 burst into flames%n"},
    {"world_squish", "%1 was squished%n"},
    {"world_died", "%1 died%n"},
    // --- telefrags against the shield ---
    {"pent_deflect", "Satan's power deflects %1's telefrag%n"},
    {"pent_both", "%1 was telefragged by %2's Satan's power%n"},
    // --- arriving and leaving ---
    {"left", "%1 left the game with %2 frags%n"},
    {"quiet_warmup", "1"},
    // --- the clock and the end ---
    {"clock_minutes", "%[%g1%] minutes remaining%n"},
    {"clock_minute", "%[%g1%] minute remaining%n"},
    {"clock_seconds", "%[%g1%] seconds%n"},
    {"clock_second", "%[%g1%] second%n"},
    {"clock_lead", "^Team^ %[%1%] leads by %g2 frags%n"},
    {"clock_lead_one", "^Team^ %[%1%] leads by %g2 frag%n"},
    {"clock_tie", "The game is currently a tie%n"},
    {"match_over", "The match is over%n"},
    {"team_scores_head", "%n^Team scores^: ^frags %. percentage^%n%l%n"},
    {"team_scores_row", "%[%1%]: %2 %. %3%%%n"},
    {"team_scores_tail", "%l%n"},
    // --- powers wearing off ---
    {"power_quad_off", "Quad Damage is wearing off%n"},
    {"power_pent_off", "Protection is almost burned out%n"},
    // --- the lobby ---
    {"ready", "%1 ^is ready^ - no matchtag set%n"},
    {"ready_team", "%1 ^is ready^ %[%2%] - no matchtag set%n"},
    {"ready_again", "Type break to unready yourself%n"},
    {"unready", "%1 ^is not ready^%n"},
    {"stops", "%1 ^stops the countdown^%n"},
    {"all_ready", "All players ready%n"},
    {"timer", "Timer started%n"},
    {"aborting", "Aborting...%n"},
    {"get_ready", "^Get ready^!%n"},
    {"begun", "^The match has begun!^%n"},
    {"count_head", "^Countdown^: %>2g1%n%n%n"},
    {"count_dm", "Deathmatch %>2g1%n"},
    {"count_mode", "Mode %>8r1%n"},
    {"count_antilag", "Antilag %>5g1%n"},
    {"count_teamplay", "Teamplay %>4g1%n"},
    {"count_timelimit", "Timelimit %>3g1%n"},
    {"count_fraglimit", "Fraglimit %>3g1%n"},
    {"count_powerups", "Powerups %>4r1%n"},
    {"count_tail", "%nno matchtag%n%n%n"},
    {"mode_duel", "D u e l"},
    {"mode_team", "T e a m"},
    {"mode_ffa", "F F A"},
    {"sound_timer", "items/protect2.wav"},
    {"sound_tick", "buttons/switch04.wav"},
};

// The server's console lines that give the mod and the lobby `words`: `localinfo <key>
// "<template>"` each.
inline std::vector<std::string> word_lines(std::span<const WordRow> words) {
  std::vector<std::string> lines;
  lines.reserve(words.size());
  for (const WordRow& w : words)
    lines.push_back("localinfo " + std::string(w.key) + " \"" + std::string(w.text) + "\"");
  return lines;
}

}  // namespace shooters_edge::profiles
