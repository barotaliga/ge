// THE RULES' SHARED NAMES: what the arena's and the campaign's rules have in common, as the QC
// variants shared their common sources. The X-lists of the globals, fields and functions both
// programs declare (each variant adds its own after them), the strings every rule stores or
// passes (one list for both: a string is a value, so the one a program never uses costs nothing),
// the constants and the words-level helpers (a condition's test, the bit operations, a vector's
// equality, as the bytecode does them). The rules in common_*.cpp are compiled once a variant
// (variant.hpp), each against its own declaration. Internal to games/shooters_edge/rules.
//
// ge:summary The rules' shared declaration lists, strings, constants and helpers (internal)
// ge:keywords rules-common native-arena native-campaign declaration x-list globals fields strings
// ge:keywords functions constants
// ge:skill progs shooter
// ge:test tests/shooters_edge_arena_rules_test.cpp
#pragma once

#include <bit>
#include <cstdint>
#include <initializer_list>

#include "progs/file.hpp"
#include "progs/native.hpp"
#include "shooters_edge/rules/calls.hpp"

namespace shooters_edge::server::rules {

namespace pg = ge::progs;

// THE GLOBALS both programs declare, after their form's (the server and the bots find some by
// name: `match_over`).
#define SE_COMMON_GLOBALS(X)       \
  X(fraglimit, kTypeFloat)         \
  X(timelimit, kTypeFloat)         \
  X(match_over, kTypeFloat)        \
  X(telefragging, kTypeFloat)      \
  X(intermission_time, kTypeFloat) \
  X(last_spawn, kTypeEntity)       \
  X(activator, kTypeEntity)        \
  X(damage_attacker, kTypeEntity)  \
  X(arsenal_row1, kTypeString)     \
  X(arsenal_row2, kTypeString)     \
  X(arsenal_row3, kTypeString)     \
  X(arsenal_row4, kTypeString)     \
  X(arsenal_row5, kTypeString)     \
  X(arsenal_row6, kTypeString)     \
  X(arsenal_row7, kTypeString)     \
  X(arsenal_row8, kTypeString)     \
  X(arsenal_view1, kTypeString)    \
  X(arsenal_view2, kTypeString)    \
  X(arsenal_view3, kTypeString)    \
  X(arsenal_view4, kTypeString)    \
  X(arsenal_view5, kTypeString)    \
  X(arsenal_view6, kTypeString)    \
  X(arsenal_view7, kTypeString)    \
  X(arsenal_view8, kTypeString)    \
  X(arsenal_missile1, kTypeString) \
  X(arsenal_missile2, kTypeString) \
  X(arsenal_missile3, kTypeString) \
  X(arsenal_missile4, kTypeString) \
  X(arsenal_missile5, kTypeString) \
  X(arsenal_missile6, kTypeString) \
  X(arsenal_missile7, kTypeString) \
  X(arsenal_missile8, kTypeString)

// THE FIELDS after the form's, in the QC's order (the server finds `maxspeed`, `gravity` and
// `attack_finished` by name; a map's "light" key fills `light_lev`).
#define SE_COMMON_FIELDS(X)            \
  X(maxspeed, kTypeFloat)              \
  X(gravity, kTypeFloat)               \
  X(attack_finished, kTypeFloat)       \
  X(pain_finished, kTypeFloat)         \
  X(air_finished, kTypeFloat)          \
  X(respawn_ready, kTypeFloat)         \
  X(spawn_time, kTypeFloat)            \
  X(delay, kTypeFloat)                 \
  X(wait, kTypeFloat)                  \
  X(speed, kTypeFloat)                 \
  X(lip, kTypeFloat)                   \
  X(worldtype, kTypeFloat)             \
  X(height, kTypeFloat)                \
  X(dmg, kTypeFloat)                   \
  X(state, kTypeFloat)                 \
  X(pos1, kTypeVector)                 \
  X(pos2, kTypeVector)                 \
  X(finaldest, kTypeVector)            \
  X(move_done, kTypeFunction)          \
  X(killtarget, kTypeString)           \
  X(deathtype, kTypeString)            \
  X(respawn_time, kTypeFloat)          \
  X(hazard_finished, kTypeFloat)       \
  X(super_damage_finished, kTypeFloat) \
  X(power_said, kTypeFloat)            \
  X(invincible_finished, kTypeFloat)   \
  X(shield_finished, kTypeFloat)       \
  X(shield_said, kTypeFloat)           \
  X(shield_flash, kTypeFloat)          \
  X(shield_heard, kTypeFloat)          \
  X(light_lev, kTypeFloat)             \
  X(style, kTypeFloat)                 \
  X(th_die, kTypeFunction)             \
  X(th_hurt, kTypeFunction)            \
  X(show_hostile, kTypeFloat)          \
  X(blast_share, kTypeFloat)           \
  X(body_next, kTypeFloat)             \
  X(body_first, kTypeFloat)            \
  X(body_step, kTypeFloat)             \
  X(splash_next, kTypeFloat)

// THE STRINGS the rules store or pass, both variants'.
#define SE_RULES_STRINGS(X)                                                                        \
  X(additional_health, "You get 10 additional health\n")                                           \
  X(airbut1_wav, "buttons/airbut1.wav")                                                            \
  X(arc_gun, "Arc Gun")                                                                            \
  X(armor1_wav, "items/armor1.wav")                                                                \
  X(armour_row1, "0.3 100 0 progs/armor.mdl items/armor1.wav")                                     \
  X(armour_row2, "0.6 150 1 progs/armor.mdl items/armor1.wav")                                     \
  X(armour_row3, "0.8 200 2 progs/armor.mdl items/armor1.wav")                                     \
  X(arsenal_armour1, "arsenal_armour1")                                                            \
  X(arsenal_armour2, "arsenal_armour2")                                                            \
  X(arsenal_armour3, "arsenal_armour3")                                                            \
  X(arsenal_weapon1, "arsenal_weapon1")                                                            \
  X(arsenal_weapon2, "arsenal_weapon2")                                                            \
  X(arsenal_weapon3, "arsenal_weapon3")                                                            \
  X(arsenal_weapon4, "arsenal_weapon4")                                                            \
  X(arsenal_weapon5, "arsenal_weapon5")                                                            \
  X(arsenal_weapon6, "arsenal_weapon6")                                                            \
  X(arsenal_weapon7, "arsenal_weapon7")                                                            \
  X(arsenal_weapon8, "arsenal_weapon8")                                                            \
  X(b_batt0, "maps/b_batt0.bsp")                                                                   \
  X(b_bh10, "maps/b_bh10.bsp")                                                                     \
  X(b_bh100, "maps/b_bh100.bsp")                                                                   \
  X(b_bh25, "maps/b_bh25.bsp")                                                                     \
  X(b_nail0, "maps/b_nail0.bsp")                                                                   \
  X(b_rock0, "maps/b_rock0.bsp")                                                                   \
  X(b_shell0, "maps/b_shell0.bsp")                                                                 \
  X(backpack, "backpack")                                                                          \
  X(backpack_mdl, "progs/backpack.mdl")                                                            \
  X(basetry_wav, "doors/basetry.wav")                                                              \
  X(baseuse_wav, "doors/baseuse.wav")                                                              \
  X(bf, "bf\n")                                                                                    \
  X(blown_apart, "blown apart")                                                                    \
  X(burned, "burned")                                                                              \
  X(by, " by ")                                                                                    \
  X(calc_move_no_speed, "calc_move: no speed for ")                                                \
  X(cells, "cells")                                                                                \
  X(clock_lead, "clock_lead")                                                                      \
  X(clock_lead_one, "clock_lead_one")                                                              \
  X(clock_minute, "clock_minute")                                                                  \
  X(clock_minutes, "clock_minutes")                                                                \
  X(clock_second, "clock_second")                                                                  \
  X(clock_seconds, "clock_seconds")                                                                \
  X(clock_tie, "clock_tie")                                                                        \
  X(colon, ": ")                                                                                   \
  X(comma, ", ")                                                                                   \
  X(cut_down, "cut down")                                                                          \
  X(damage2_wav, "items/damage2.wav")                                                              \
  X(damage_wav, "items/damage.wav")                                                                \
  X(ddoor1_wav, "doors/ddoor1.wav")                                                                \
  X(ddoor2_wav, "doors/ddoor2.wav")                                                                \
  X(death1_wav, "player/death1.wav")                                                               \
  X(deathmatch, "deathmatch")                                                                      \
  X(delayed_use, "delayed_use")                                                                    \
  X(dest_needs_targetname, "a teleport destination needs a targetname")                            \
  X(died, "died")                                                                                  \
  X(discharged, "discharged")                                                                      \
  X(disconnected, "disconnected")                                                                  \
  X(dissolved, "dissolved")                                                                        \
  X(dmm4_invinc_time, "dmm4_invinc_time")                                                          \
  X(door_trigger, "door_trigger")                                                                  \
  X(doormv1_wav, "doors/doormv1.wav")                                                              \
  X(double_scattergun, "Double Scattergun")                                                        \
  X(drclos4_wav, "doors/drclos4.wav")                                                              \
  X(drown1_wav, "player/drown1.wav")                                                               \
  X(drowned, "drowned")                                                                            \
  X(electrocuted, "electrocuted")                                                                  \
  X(entered_the_game, " entered the game\n")                                                       \
  X(fell_out, "an item fell out of the map at ")                                                   \
  X(flame2_mdl, "progs/flame2.mdl")                                                                \
  X(flame_mdl, "progs/flame.mdl")                                                                  \
  X(fragged, "fragged")                                                                            \
  X(fragged_themselves, " fragged themselves\n")                                                   \
  X(fraglimit, "fraglimit")                                                                        \
  X(fraglimit_hit, "Fraglimit hit.")                                                               \
  X(frags_nl, " frags\n")                                                                          \
  X(func_door, "func_door")                                                                        \
  X(g_light, "progs/g_light.mdl")                                                                  \
  X(g_nail, "progs/g_nail.mdl")                                                                    \
  X(g_nail2, "progs/g_nail2.mdl")                                                                  \
  X(g_rock, "progs/g_rock.mdl")                                                                    \
  X(g_rock2, "progs/g_rock2.mdl")                                                                  \
  X(g_shot, "progs/g_shot.mdl")                                                                    \
  X(gained_bonus_powers, " gained bonus powers!!!\n")                                              \
  X(ge_phase, "ge_phase")                                                                          \
  X(gender, "gender")                                                                              \
  X(gibbed_by_a_grenade, "gibbed by a grenade")                                                    \
  X(got_backpack, "You got a backpack\n")                                                          \
  X(got_backpack_with, "You got a backpack with the ")                                             \
  X(got_octapower, "You got the OctaPower\n")                                                      \
  X(grenade, "grenade")                                                                            \
  X(grenade_launcher, "Grenade Launcher")                                                          \
  X(health1_wav, "items/health1.wav")                                                              \
  X(health_nl, " health\n")                                                                        \
  X(heavy_spiker, "Heavy Spiker")                                                                  \
  X(her, "her")                                                                                    \
  X(his, "his")                                                                                    \
  X(hurt, "hurt")                                                                                  \
  X(hydro1_wav, "doors/hydro1.wav")                                                                \
  X(hydro2_wav, "doors/hydro2.wav")                                                                \
  X(info_player_deathmatch, "info_player_deathmatch")                                              \
  X(info_player_start, "info_player_start")                                                        \
  X(invulner_mdl, "progs/invulner.mdl")                                                            \
  X(item_cells, "item_cells")                                                                      \
  X(item_rockets, "item_rockets")                                                                  \
  X(item_shells, "item_shells")                                                                    \
  X(item_spikes, "item_spikes")                                                                    \
  X(itembk2_wav, "items/itembk2.wav")                                                              \
  X(k_disallow_weapons, "k_disallow_weapons")                                                      \
  X(left, "left")                                                                                  \
  X(left_the_game_with, " left the game with ")                                                    \
  X(lock4_wav, "weapons/lock4.wav")                                                                \
  X(match_over, "match_over")                                                                      \
  X(medplat1_wav, "plats/medplat1.wav")                                                            \
  X(medplat2_wav, "plats/medplat2.wav")                                                            \
  X(medtry_wav, "doors/medtry.wav")                                                                \
  X(meduse_wav, "doors/meduse.wav")                                                                \
  X(nailed, "nailed")                                                                              \
  X(nails, "nails")                                                                                \
  X(needs_gold_key, "This door needs the gold key.")                                               \
  X(needs_silver_key, "This door needs the silver key.")                                           \
  X(nl, "\n")                                                                                      \
  X(no_spawn_point, "the map has no spawn point for players")                                      \
  X(no_weapon, "no weapon\n")                                                                      \
  X(not_enough_ammo, "not enough ammo\n")                                                          \
  X(null_wav, "misc/null.wav")                                                                     \
  X(obit_axe, "obit_axe")                                                                          \
  X(obit_discharge, "obit_discharge")                                                              \
  X(obit_gl, "obit_gl")                                                                            \
  X(obit_gl_gib, "obit_gl_gib")                                                                    \
  X(obit_lg, "obit_lg")                                                                            \
  X(obit_lg_gib, "obit_lg_gib")                                                                    \
  X(obit_ng, "obit_ng")                                                                            \
  X(obit_other, "obit_other")                                                                      \
  X(obit_rl, "obit_rl")                                                                            \
  X(obit_rl_gib, "obit_rl_gib")                                                                    \
  X(obit_rl_quad, "obit_rl_quad")                                                                  \
  X(obit_sg, "obit_sg")                                                                            \
  X(obit_sg_gib, "obit_sg_gib")                                                                    \
  X(obit_sng, "obit_sng")                                                                          \
  X(obit_sng_gib, "obit_sng_gib")                                                                  \
  X(obit_squish, "obit_squish")                                                                    \
  X(obit_ssg, "obit_ssg")                                                                          \
  X(obit_ssg_quad, "obit_ssg_quad")                                                                \
  X(obit_tele, "obit_tele")                                                                        \
  X(one, "1")                                                                                      \
  X(pain1_wav, "player/pain1.wav")                                                                 \
  X(pkup_wav, "weapons/pkup.wav")                                                                  \
  X(plat1_wav, "plats/plat1.wav")                                                                  \
  X(plat2_wav, "plats/plat2.wav")                                                                  \
  X(plat_trigger, "plat_trigger")                                                                  \
  X(player, "player")                                                                              \
  X(player_mdl, "progs/player.mdl")                                                                \
  X(plyrjmp8_wav, "player/plyrjmp8.wav")                                                           \
  X(point, ".")                                                                                    \
  X(power_pent_off, "power_pent_off")                                                              \
  X(power_quad_off, "power_quad_off")                                                              \
  X(protect2_wav, "items/protect2.wav")                                                            \
  X(protect3_wav, "items/protect3.wav")                                                            \
  X(protect_wav, "items/protect.wav")                                                              \
  X(protection_burned_out, "Protection is almost burned out\n")                                    \
  X(quad_damage, "Quad Damage")                                                                    \
  X(quaddama_mdl, "progs/quaddama.mdl")                                                            \
  X(quiet_warmup, "quiet_warmup")                                                                  \
  X(r_item1_wav, "items/r_item1.wav")                                                              \
  X(r_item2_wav, "items/r_item2.wav")                                                              \
  X(r_tele1_wav, "misc/r_tele1.wav")                                                               \
  X(rocket, "rocket")                                                                              \
  X(rocket_launcher, "Rocket Launcher")                                                            \
  X(rockets, "rockets")                                                                            \
  X(row1,                                                                                          \
    "melee none 0 0.5 1 20 0 64 0 0 progs/v_axe.mdl weapons/ax1.wav - player/axhit2.wav - - 0")    \
  X(row2, "pellets shells 1 0.5 6 4 0.04 2048 0 0 progs/v_shot.mdl weapons/guncock.wav - - - - 0") \
  X(row3,                                                                                          \
    "pellets shells 2 0.7 14 4 0.07 2048 0 0 progs/v_shot2.mdl weapons/shotgn2.wav - - - "         \
    "progs/g_shot.mdl 5")                                                                          \
  X(row4,                                                                                          \
    "spike nails 1 0.1 1 9 0 1000 6 0 progs/v_nail.mdl weapons/rocket1i.wav progs/spike.mdl "      \
    "- - progs/g_nail.mdl 30")                                                                     \
  X(row5,                                                                                          \
    "spike nails 2 0.1 1 18 0 1000 6 0 progs/v_nail2.mdl weapons/spike2.wav "                      \
    "progs/s_spike.mdl - - progs/g_nail2.mdl 30")                                                  \
  X(row6,                                                                                          \
    "grenade rockets 1 0.6 1 0 0 600 2.5 120 progs/v_rock.mdl weapons/grenade.wav "                \
    "progs/grenade.mdl weapons/bounce.wav weapons/r_exp3.wav progs/g_rock.mdl 5")                  \
  X(row7,                                                                                          \
    "rocket rockets 1 0.8 1 100 0 1000 5 120 progs/v_rock2.mdl weapons/sgun1.wav "                 \
    "progs/missile.mdl - weapons/r_exp3.wav progs/g_rock2.mdl 5")                                  \
  X(row8,                                                                                          \
    "beam cells 1 0.1 1 30 0 600 0 35 progs/v_light.mdl weapons/lhit.wav - - - "                   \
    "progs/g_light.mdl 15")                                                                        \
  X(runetry_wav, "doors/runetry.wav")                                                              \
  X(runeuse_wav, "doors/runeuse.wav")                                                              \
  X(s_light_spr, "progs/s_light.spr")                                                              \
  X(self_discharge_lava, "self_discharge_lava")                                                    \
  X(self_discharge_slime, "self_discharge_slime")                                                  \
  X(self_discharge_water, "self_discharge_water")                                                  \
  X(self_gl, "self_gl")                                                                            \
  X(self_kill, "self_kill")                                                                        \
  X(self_other, "self_other")                                                                      \
  X(self_rl, "self_rl")                                                                            \
  X(self_squish, "self_squish")                                                                    \
  X(shafted, "shafted")                                                                            \
  X(shells, "shells")                                                                              \
  X(shield, "Shield")                                                                              \
  X(shot, "shot")                                                                                  \
  X(space, " ")                                                                                    \
  X(spike, "spike")                                                                                \
  X(spiker, "Spiker")                                                                              \
  X(squished, "squished")                                                                          \
  X(stndr1_wav, "doors/stndr1.wav")                                                                \
  X(stndr2_wav, "doors/stndr2.wav")                                                                \
  X(style0, "m")                                                                                   \
  X(style1, "mmnmmommommnonmmonqnmmo")                                                             \
  X(style2, "abcdefghijklmnopqrstuvwxyzyxwvutsrqponmlkjihgfedcba")                                 \
  X(suicides, "suicides")                                                                          \
  X(switch02_wav, "buttons/switch02.wav")                                                          \
  X(switch04_wav, "buttons/switch04.wav")                                                          \
  X(switch21_wav, "buttons/switch21.wav")                                                          \
  X(team, "team")                                                                                  \
  X(team_scores_head, "team_scores_head")                                                          \
  X(team_scores_row, "team_scores_row")                                                            \
  X(team_scores_tail, "team_scores_tail")                                                          \
  X(teamplay, "teamplay")                                                                          \
  X(telefragged, "telefragged")                                                                    \
  X(teleporter_needs_target, "a teleporter needs a target")                                        \
  X(teleporter_no_destination, "a teleporter cannot find its destination ")                        \
  X(the, "the ")                                                                                   \
  X(timelimit, "timelimit")                                                                        \
  X(timelimit_hit, "Timelimit hit.")                                                               \
  X(tk, "tk")                                                                                      \
  X(tk_squish, "tk_squish")                                                                        \
  X(tk_tele, "tk_tele")                                                                            \
  X(tornoff2_wav, "player/tornoff2.wav")                                                           \
  X(udeath_wav, "player/udeath.wav")                                                               \
  X(underscore, "_")                                                                               \
  X(was, " was ")                                                                                  \
  X(water1_wav, "misc/water1.wav")                                                                 \
  X(water2_wav, "misc/water2.wav")                                                                 \
  X(weapon_grenadelauncher, "weapon_grenadelauncher")                                              \
  X(weapon_lightning, "weapon_lightning")                                                          \
  X(weapon_nailgun, "weapon_nailgun")                                                              \
  X(weapon_rocketlauncher, "weapon_rocketlauncher")                                                \
  X(weapon_supernailgun, "weapon_supernailgun")                                                    \
  X(weapon_supershotgun, "weapon_supershotgun")                                                    \
  X(world_died, "world_died")                                                                      \
  X(world_drown, "world_drown")                                                                    \
  X(world_lava, "world_lava")                                                                      \
  X(world_lava_gib, "world_lava_gib")                                                              \
  X(world_slime, "world_slime")                                                                    \
  X(world_squish, "world_squish")                                                                  \
  X(you_get, "You get ")                                                                           \
  X(you_got_ammunition, "You got ammunition\n")                                                    \
  X(you_got_armor, "You got armor\n")                                                              \
  X(you_got_the, "You got the ")                                                                   \
  X(you_receive, "You receive ")                                                                   \
  X(empty, "")                                                                                     \
  X(misc_secret_wav, "misc/secret.wav")                                                            \
  X(misc_talk_wav, "misc/talk.wav")                                                                \
  X(info_intermission, "info_intermission")                                                        \
  X(e1m1, "e1m1")                                                                                  \
  X(e2m1, "e2m1")                                                                                  \
  X(e3m1, "e3m1")                                                                                  \
  X(e4m1, "e4m1")                                                                                  \
  X(start, "start")                                                                                \
  X(sv_episodes_done, "sv_episodes_done")                                                          \
  X(this_episode_opens_once, "This episode opens once the one before it is done.")                 \
  X(a_changelevel_trigger_without, "a changelevel trigger without a map")                          \
  X(a_secret, "A secret!")                                                                         \
  X(more_to_go, " more to go")                                                                     \
  X(done, "Done.")                                                                                 \
  X(skill, "skill")                                                                                \
  X(registered, "registered")                                                                      \
  X(the_first_rune_hums,                                                                           \
    "The first rune hums in your hand, and the halls behind you fall silent.\n\nThree more lie "   \
    "in deeper places. The way back to the hall of gates is open.")                                \
  X(the_second_rune_is,                                                                            \
    "The second rune is yours. The dead in the old keeps lie still at last.\n\nTwo more remain, "  \
    "further down.")                                                                               \
  X(three_runes_now_the,                                                                           \
    "Three runes now. The pits below have stopped their noise.\n\nOne more waits in the oldest "   \
    "place of all.")                                                                               \
  X(the_last_rune_is,                                                                              \
    "The last rune is yours, and something at the bottom of the world has felt it.\n\nThe four "   \
    "gates stand open. Go down and finish it.")                                                    \
  X(the_episode_s_rune_is_yours, "The episode's rune is yours.")                                   \
  X(end1_mdl, "progs/end1.mdl")                                                                    \
  X(end2_mdl, "progs/end2.mdl")                                                                    \
  X(end3_mdl, "progs/end3.mdl")                                                                    \
  X(end4_mdl, "progs/end4.mdl")                                                                    \
  X(you_have_a_key, "You have a key.")                                                             \
  X(w_s_key_mdl, "progs/w_s_key.mdl")                                                              \
  X(w_g_key_mdl, "progs/w_g_key.mdl")                                                              \
  X(sv_friendlyfire, "sv_friendlyfire")                                                            \
  X(sv_friendlypush, "sv_friendlypush")                                                            \
  X(info_player_coop, "info_player_coop")                                                          \
  X(the_map_has_no, "the map has no info_player_start")                                            \
  X(restart_n, "restart\n")                                                                        \
  X(mauled, "mauled")                                                                              \
  X(monster_army, "monster_army")                                                                  \
  X(gib, "gib")                                                                                    \
  X(gib1_mdl, "progs/gib1.mdl")                                                                    \
  X(gib2_mdl, "progs/gib2.mdl")                                                                    \
  X(gib3_mdl, "progs/gib3.mdl")                                                                    \
  X(path_corner, "path_corner")                                                                    \
  X(in_a_wall_at, " in a wall at ")                                                                \
  X(monster_missile, "monster_missile")                                                            \
  X(hit, "hit")                                                                                    \
  X(soldier_sattck1_wav, "soldier/sattck1.wav")                                                    \
  X(soldier_pain1_wav, "soldier/pain1.wav")                                                        \
  X(soldier_pain2_wav, "soldier/pain2.wav")                                                        \
  X(soldier_mdl, "progs/soldier.mdl")                                                              \
  X(h_guard_mdl, "progs/h_guard.mdl")                                                              \
  X(soldier_death1_wav, "soldier/death1.wav")                                                      \
  X(soldier_idle_wav, "soldier/idle.wav")                                                          \
  X(soldier_sight1_wav, "soldier/sight1.wav")                                                      \
  X(dog_dattack1_wav, "dog/dattack1.wav")                                                          \
  X(dog_dpain1_wav, "dog/dpain1.wav")                                                              \
  X(dog_mdl, "progs/dog.mdl")                                                                      \
  X(h_dog_mdl, "progs/h_dog.mdl")                                                                  \
  X(dog_ddeath_wav, "dog/ddeath.wav")                                                              \
  X(dog_dsight_wav, "dog/dsight.wav")                                                              \
  X(dog_idle_wav, "dog/idle.wav")                                                                  \
  X(knight_sword1_wav, "knight/sword1.wav")                                                        \
  X(knight_sword2_wav, "knight/sword2.wav")                                                        \
  X(knight_khurt_wav, "knight/khurt.wav")                                                          \
  X(knight_mdl, "progs/knight.mdl")                                                                \
  X(h_knight_mdl, "progs/h_knight.mdl")                                                            \
  X(knight_kdeath_wav, "knight/kdeath.wav")                                                        \
  X(knight_ksight_wav, "knight/ksight.wav")                                                        \
  X(knight_idle_wav, "knight/idle.wav")                                                            \
  X(blown_up_by_a_grenade, "blown up by a grenade")                                                \
  X(weapons_r_exp3_wav, "weapons/r_exp3.wav")                                                      \
  X(weapons_bounce_wav, "weapons/bounce.wav")                                                      \
  X(weapons_grenade_wav, "weapons/grenade.wav")                                                    \
  X(grenade_mdl, "progs/grenade.mdl")                                                              \
  X(ogre_ogsawatk_wav, "ogre/ogsawatk.wav")                                                        \
  X(ogre_ogpain1_wav, "ogre/ogpain1.wav")                                                          \
  X(ogre_mdl, "progs/ogre.mdl")                                                                    \
  X(h_ogre_mdl, "progs/h_ogre.mdl")                                                                \
  X(ogre_ogdrag_wav, "ogre/ogdrag.wav")                                                            \
  X(ogre_ogdth_wav, "ogre/ogdth.wav")                                                              \
  X(ogre_ogidle_wav, "ogre/ogidle.wav")                                                            \
  X(ogre_ogwake_wav, "ogre/ogwake.wav")                                                            \
  X(zombie_z_shot1_wav, "zombie/z_shot1.wav")                                                      \
  X(zom_gib_mdl, "progs/zom_gib.mdl")                                                              \
  X(hit_by_flesh, "hit by flesh")                                                                  \
  X(zombie_z_idle_wav, "zombie/z_idle.wav")                                                        \
  X(zombie_z_fall_wav, "zombie/z_fall.wav")                                                        \
  X(zombie_z_pain_wav, "zombie/z_pain.wav")                                                        \
  X(zombie_mdl, "progs/zombie.mdl")                                                                \
  X(h_zombie_mdl, "progs/h_zombie.mdl")                                                            \
  X(zombie_z_idle1_wav, "zombie/z_idle1.wav")                                                      \
  X(zombie_z_gib_wav, "zombie/z_gib.wav")                                                          \
  X(zombie_z_pain1_wav, "zombie/z_pain1.wav")                                                      \
  X(zombie_z_miss_wav, "zombie/z_miss.wav")                                                        \
  X(zombie_z_hit_wav, "zombie/z_hit.wav")                                                          \
  X(zombie_idle_w2_wav, "zombie/idle_w2.wav")                                                      \
  X(wizard_wattack_wav, "wizard/wattack.wav")                                                      \
  X(w_spike_mdl, "progs/w_spike.mdl")                                                              \
  X(spat_on, "spat on")                                                                            \
  X(wizard_wpain_wav, "wizard/wpain.wav")                                                          \
  X(wizard_mdl, "progs/wizard.mdl")                                                                \
  X(h_wizard_mdl, "progs/h_wizard.mdl")                                                            \
  X(wizard_hit_wav, "wizard/hit.wav")                                                              \
  X(wizard_wdeath_wav, "wizard/wdeath.wav")                                                        \
  X(wizard_widle1_wav, "wizard/widle1.wav")                                                        \
  X(wizard_widle2_wav, "wizard/widle2.wav")                                                        \
  X(wizard_wsight_wav, "wizard/wsight.wav")                                                        \
  X(fish_bite_wav, "fish/bite.wav")                                                                \
  X(fish_mdl, "progs/fish.mdl")                                                                    \
  X(fish_death_wav, "fish/death.wav")                                                              \
  X(fish_idle_wav, "fish/idle.wav")                                                                \
  X(demon_dhit2_wav, "demon/dhit2.wav")                                                            \
  X(eviscerated, "eviscerated")                                                                    \
  X(demon_dland2_wav, "demon/dland2.wav")                                                          \
  X(demon_djump_wav, "demon/djump.wav")                                                            \
  X(demon_dpain1_wav, "demon/dpain1.wav")                                                          \
  X(demon_mdl, "progs/demon.mdl")                                                                  \
  X(h_demon_mdl, "progs/h_demon.mdl")                                                              \
  X(demon_ddeath_wav, "demon/ddeath.wav")                                                          \
  X(demon_idle1_wav, "demon/idle1.wav")                                                            \
  X(demon_sight2_wav, "demon/sight2.wav")                                                          \
  X(shambler_melee2_wav, "shambler/melee2.wav")                                                    \
  X(shambler_melee1_wav, "shambler/melee1.wav")                                                    \
  X(shambler_smack_wav, "shambler/smack.wav")                                                      \
  X(shambler_sattck1_wav, "shambler/sattck1.wav")                                                  \
  X(struck_by_lightning, "struck by lightning")                                                    \
  X(shambler_shurt2_wav, "shambler/shurt2.wav")                                                    \
  X(shambler_mdl, "progs/shambler.mdl")                                                            \
  X(h_shams_mdl, "progs/h_shams.mdl")                                                              \
  X(shambler_sboom_wav, "shambler/sboom.wav")                                                      \
  X(shambler_sdeath_wav, "shambler/sdeath.wav")                                                    \
  X(shambler_sidle_wav, "shambler/sidle.wav")                                                      \
  X(shambler_ssight_wav, "shambler/ssight.wav")                                                    \
  X(exploded_by_a_homing_ball, "exploded by a homing ball")                                        \
  X(caught_by_a_homing, "caught by a homing ball's blast")                                         \
  X(shalrath_attack_wav, "shalrath/attack.wav")                                                    \
  X(shalrath_attack2_wav, "shalrath/attack2.wav")                                                  \
  X(v_spike_mdl, "progs/v_spike.mdl")                                                              \
  X(shalrath_pain_wav, "shalrath/pain.wav")                                                        \
  X(shalrath_mdl, "progs/shalrath.mdl")                                                            \
  X(h_shal_mdl, "progs/h_shal.mdl")                                                                \
  X(shalrath_death_wav, "shalrath/death.wav")                                                      \
  X(shalrath_idle_wav, "shalrath/idle.wav")                                                        \
  X(shalrath_sight_wav, "shalrath/sight.wav")                                                      \
  X(enforcer_enfire_wav, "enforcer/enfire.wav")                                                    \
  X(laser_mdl, "progs/laser.mdl")                                                                  \
  X(lasered, "lasered")                                                                            \
  X(enforcer_pain1_wav, "enforcer/pain1.wav")                                                      \
  X(enforcer_pain2_wav, "enforcer/pain2.wav")                                                      \
  X(enforcer_mdl, "progs/enforcer.mdl")                                                            \
  X(h_mega_mdl, "progs/h_mega.mdl")                                                                \
  X(enforcer_death1_wav, "enforcer/death1.wav")                                                    \
  X(enforcer_enfstop_wav, "enforcer/enfstop.wav")                                                  \
  X(enforcer_idle1_wav, "enforcer/idle1.wav")                                                      \
  X(enforcer_sight1_wav, "enforcer/sight1.wav")                                                    \
  X(enforcer_sight2_wav, "enforcer/sight2.wav")                                                    \
  X(enforcer_sight3_wav, "enforcer/sight3.wav")                                                    \
  X(enforcer_sight4_wav, "enforcer/sight4.wav")                                                    \
  X(slimed, "slimed")                                                                              \
  X(blob_hit1_wav, "blob/hit1.wav")                                                                \
  X(blob_land1_wav, "blob/land1.wav")                                                              \
  X(blob_death1_wav, "blob/death1.wav")                                                            \
  X(caught_by_a_spawn_s_burst, "caught by a spawn's burst")                                        \
  X(tarbaby_mdl, "progs/tarbaby.mdl")                                                              \
  X(blob_sight1_wav, "blob/sight1.wav")                                                            \
  X(hknight_slash1_wav, "hknight/slash1.wav")                                                      \
  X(hknight_attack1_wav, "hknight/attack1.wav")                                                    \
  X(k_spike_mdl, "progs/k_spike.mdl")                                                              \
  X(burned_by_magic, "burned by magic")                                                            \
  X(hknight_pain1_wav, "hknight/pain1.wav")                                                        \
  X(hknight_mdl, "progs/hknight.mdl")                                                              \
  X(h_hellkn_mdl, "progs/h_hellkn.mdl")                                                            \
  X(hknight_death1_wav, "hknight/death1.wav")                                                      \
  X(hknight_sight1_wav, "hknight/sight1.wav")                                                      \
  X(hknight_hit_wav, "hknight/hit.wav")                                                            \
  X(hknight_idle_wav, "hknight/idle.wav")                                                          \
  X(hknight_grunt_wav, "hknight/grunt.wav")                                                        \
  X(a_teleport_train_needs, "a teleport train needs a path")                                       \
  X(teleport_mdl, "progs/teleport.mdl")                                                            \
  X(burned_by_the_lava_boss, "burned by the lava boss")                                            \
  X(caught_by_the_lava, "caught by the lava boss's fire")                                          \
  X(boss1_throw_wav, "boss1/throw.wav")                                                            \
  X(lavaball_mdl, "progs/lavaball.mdl")                                                            \
  X(boss_mdl, "progs/boss.mdl")                                                                    \
  X(boss1_sight1_wav, "boss1/sight1.wav")                                                          \
  X(boss1_pain_wav, "boss1/pain.wav")                                                              \
  X(boss1_death_wav, "boss1/death.wav")                                                            \
  X(boss1_out1_wav, "boss1/out1.wav")                                                              \
  X(lightning, "lightning")                                                                        \
  X(misc_power_wav, "misc/power.wav")                                                              \
  X(the_thing_at_the,                                                                              \
    "The thing at the bottom of the world is gone, torn apart from the inside.\n\nThe four runes " \
    "go quiet. The way home is open.")                                                             \
  X(boss2_pop2_wav, "boss2/pop2.wav")                                                              \
  X(boss2_death_wav, "boss2/death.wav")                                                            \
  X(oldone_mdl, "progs/oldone.mdl")                                                                \
  X(boss2_idle_wav, "boss2/idle.wav")                                                              \
  X(boss2_sight_wav, "boss2/sight.wav")                                                            \
  X(crushed, "crushed")                                                                            \
  X(a_train_s_path, "a train's path ends at nothing named ")                                       \
  X(a_train_cannot_find, "a train cannot find its first corner ")                                  \
  X(a_train_needs_a_path, "a train needs a path")                                                  \
  X(plats_train2_wav, "plats/train2.wav")                                                          \
  X(plats_train1_wav, "plats/train1.wav")                                                          \
  X(doors_latch2_wav, "doors/latch2.wav")                                                          \
  X(doors_winch2_wav, "doors/winch2.wav")                                                          \
  X(doors_airdoor2_wav, "doors/airdoor2.wav")                                                      \
  X(doors_airdoor1_wav, "doors/airdoor1.wav")                                                      \
  X(doors_basesec2_wav, "doors/basesec2.wav")                                                      \
  X(doors_basesec1_wav, "doors/basesec1.wav")                                                      \
  X(explo_box, "explo_box")                                                                        \
  X(blew_up, "blew up")                                                                            \
  X(b_explob_bsp, "maps/b_explob.bsp")                                                             \
  X(b_exbox2_bsp, "maps/b_exbox2.bsp")                                                             \
  X(fireball, "fireball")                                                                          \
  X(weapons_spike2_wav, "weapons/spike2.wav")                                                      \
  X(s_spike_mdl, "progs/s_spike.mdl")                                                              \
  X(spike_mdl, "progs/spike.mdl")                                                                  \
  X(bubble, "bubble")                                                                              \
  X(s_bubble_spr, "progs/s_bubble.spr")                                                            \
  X(ambience_comp1_wav, "ambience/comp1.wav")                                                      \
  X(ambience_drip1_wav, "ambience/drip1.wav")                                                      \
  X(ambience_drone6_wav, "ambience/drone6.wav")                                                    \
  X(ambience_buzz1_wav, "ambience/buzz1.wav")                                                      \
  X(ambience_fl_hum1_wav, "ambience/fl_hum1.wav")                                                  \
  X(ambience_suck1_wav, "ambience/suck1.wav")                                                      \
  X(ambience_swamp1_wav, "ambience/swamp1.wav")                                                    \
  X(ambience_swamp2_wav, "ambience/swamp2.wav")                                                    \
  X(ambience_thunder1_wav, "ambience/thunder1.wav")

// THE FUNCTIONS both programs hold by name: the hooks, the map's things, the thinks and touches.
#define SE_COMMON_FUNCTIONS(X)                                      \
  X(PlayerPreThink, "PlayerPreThink")                               \
  X(PlayerPostThink, "PlayerPostThink")                             \
  X(ClientKill, "ClientKill")                                       \
  X(ClientConnect, "ClientConnect")                                 \
  X(PutClientInServer, "PutClientInServer")                         \
  X(ClientDisconnect, "ClientDisconnect")                           \
  X(info_teleport_destination, "info_teleport_destination")         \
  X(light, "light")                                                 \
  X(light_fluoro, "light_fluoro")                                   \
  X(light_torch_small_walltorch, "light_torch_small_walltorch")     \
  X(light_flame_large_yellow, "light_flame_large_yellow")           \
  X(light_flame_small_yellow, "light_flame_small_yellow")           \
  X(light_flame_small_white, "light_flame_small_white")             \
  X(light_globe, "light_globe")                                     \
  X(func_wall, "func_wall")                                         \
  X(func_illusionary, "func_illusionary")                           \
  X(func_door, "func_door")                                         \
  X(func_plat, "func_plat")                                         \
  X(func_button, "func_button")                                     \
  X(trigger_multiple, "trigger_multiple")                           \
  X(trigger_once, "trigger_once")                                   \
  X(trigger_teleport, "trigger_teleport")                           \
  X(trigger_push, "trigger_push")                                   \
  X(trigger_hurt, "trigger_hurt")                                   \
  X(item_health, "item_health")                                     \
  X(item_armor1, "item_armor1")                                     \
  X(item_armor2, "item_armor2")                                     \
  X(item_armorInv, "item_armorInv")                                 \
  X(item_shells, "item_shells")                                     \
  X(item_spikes, "item_spikes")                                     \
  X(item_rockets, "item_rockets")                                   \
  X(item_cells, "item_cells")                                       \
  X(weapon_supershotgun, "weapon_supershotgun")                     \
  X(weapon_nailgun, "weapon_nailgun")                               \
  X(weapon_supernailgun, "weapon_supernailgun")                     \
  X(weapon_grenadelauncher, "weapon_grenadelauncher")               \
  X(weapon_rocketlauncher, "weapon_rocketlauncher")                 \
  X(weapon_lightning, "weapon_lightning")                           \
  X(item_artifact_super_damage, "item_artifact_super_damage")       \
  X(item_artifact_invulnerability, "item_artifact_invulnerability") \
  X(noop, "noop")                                                   \
  X(remove_self, "remove_self")                                     \
  X(calc_move_arrive, "calc_move_arrive")                           \
  X(delayed_use, "delayed_use")                                     \
  X(item_return, "item_return")                                     \
  X(health_decay, "health_decay")                                   \
  X(health_touch, "health_touch")                                   \
  X(armour_touch, "armour_touch")                                   \
  X(ammo_touch, "ammo_touch")                                       \
  X(weapon_touch, "weapon_touch")                                   \
  X(backpack_touch, "backpack_touch")                               \
  X(backpack_gone, "backpack_gone")                                 \
  X(power_touch, "power_touch")                                     \
  X(spike_touch, "spike_touch")                                     \
  X(blast, "blast")                                                 \
  X(rocket_touch, "rocket_touch")                                   \
  X(grenade_touch, "grenade_touch")                                 \
  X(door_hit_bottom, "door_hit_bottom")                             \
  X(door_hit_top, "door_hit_top")                                   \
  X(door_go_down, "door_go_down")                                   \
  X(door_blocked, "door_blocked")                                   \
  X(door_use, "door_use")                                           \
  X(door_killed, "door_killed")                                     \
  X(door_touch, "door_touch")                                       \
  X(door_trigger_touch, "door_trigger_touch")                       \
  X(door_link, "door_link")                                         \
  X(plat_hit_top, "plat_hit_top")                                   \
  X(plat_hit_bottom, "plat_hit_bottom")                             \
  X(plat_go_down, "plat_go_down")                                   \
  X(plat_blocked, "plat_blocked")                                   \
  X(plat_field_touch, "plat_field_touch")                           \
  X(plat_use, "plat_use")                                           \
  X(button_return, "button_return")                                 \
  X(button_pressed, "button_pressed")                               \
  X(button_touch, "button_touch")                                   \
  X(button_use, "button_use")                                       \
  X(button_killed, "button_killed")                                 \
  X(trigger_ready, "trigger_ready")                                 \
  X(trigger_touch, "trigger_touch")                                 \
  X(trigger_use, "trigger_use")                                     \
  X(teleport_touch, "teleport_touch")                               \
  X(push_touch, "push_touch")                                       \
  X(hurt_ready, "hurt_ready")                                       \
  X(hurt_touch, "hurt_touch")

// WORDS AS THE BYTECODE TESTS THEM. A condition on a float (IF / IFNOT) tests the word, so -0 is
// true; `!x`, `&&` and `||` compare as floats (and evaluate both sides: the rules ask both
// before combining, with `&` and `|` on bools). `&` and `|` on floats work on their integer parts.
inline bool on(float x) { return std::bit_cast<uint32_t>(x) != 0; }
inline float band(float a, float b) { return pg::bit_and(a, b); }
inline float bor(float a, float b) { return pg::bit_or(a, b); }
inline bool same_vec(Vec3 a, Vec3 b) { return a.x == b.x && a.y == b.y && a.z == b.z; }

// THE CONSTANTS.
constexpr float kMovetypeNone = 0, kMovetypeWalk = 3, kMovetypeToss = 6, kMovetypePush = 7,
                kMovetypeFlymissile = 9, kMovetypeBounce = 10;
constexpr float kSolidNot = 0, kSolidTrigger = 1, kSolidBbox = 2, kSolidSlidebox = 3, kSolidBsp = 4;
constexpr int kFlClient = 8, kFlItem = 256, kFlOnground = 512, kFlJumpreleased = 4096;
constexpr float kDamageNo = 0, kDamageYes = 1, kDamageAim = 2;
constexpr float kDeadNo = 0, kDeadDead = 2, kDeadRespawnable = 3;
constexpr float kContentSlime = -4, kContentLava = -5, kContentSky = -6;
constexpr int kWScatter = 1, kWDouble = 2, kWSpiker = 4, kWHeavySpiker = 8, kWGrenades = 16,
              kWRockets = 32, kWArc = 64, kWBlade = 4096;
constexpr int kItShells = 256, kItNails = 512, kItRockets = 1024, kItCells = 2048, kItArmor1 = 8192,
              kItArmor2 = 16384, kItArmor3 = 32768, kItArmorAll = 57344, kItKey1 = 131072,
              kItKey2 = 262144, kItInvulnerability = 1048576, kItQuad = 4194304;
constexpr float kChanWeapon = 1, kChanVoice = 2, kChanItem = 3, kChanBody = 4;
constexpr float kAttnNone = 0, kAttnNorm = 1;
constexpr float kPrintLow = 0, kPrintMedium = 1, kPrintHigh = 2;
constexpr float kMulticastPhs = 1, kMulticastPvs = 2;
constexpr float kSvcTempentity = 23;
constexpr float kTeSpike = 0, kTeGunshot = 2, kTeExplosion = 3, kTeLightning2 = 6, kTeBlood = 12;
constexpr float kStateTop = 0, kStateBottom = 1, kStateUp = 2, kStateDown = 3,
                kStateWaiting = 4;  // a named plat at its top, until it is used
constexpr int kEfMuzzleflash = 2, kEfDimlight = 8, kEfBlue = 64, kEfRed = 128;

}  // namespace shooters_edge::server::rules
