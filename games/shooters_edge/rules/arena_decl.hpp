// The arena's declaration and the rules' shared names: its globals, fields, strings and functions
// as X-lists (the program's declaration is built from them, and each is an enum the rules index
// by), the constants the rules use, the words-level helpers (a condition's test, the bit
// operations, a vector's equality, as the bytecode does them), and every rule one file calls in
// another. Internal to games/shooters_edge/rules/arena_*.cpp.
//
// ge:summary The arena's declaration lists, constants and shared rule names (internal)
// ge:keywords arena native-arena declaration x-list globals fields strings functions
// ge:skill progs shooter
// ge:test tests/shooters_edge_arena_rules_test.cpp
#pragma once

#include <bit>
#include <cstdint>
#include <initializer_list>

#include "progs/file.hpp"
#include "progs/native.hpp"
#include "shooters_edge/rules/calls.hpp"

namespace shooters_edge::server::arena {

namespace pg = ge::progs;

// THE GLOBALS, in the order the arena's QC declared them (the server and the bots find some by
// name: `deathmatch`, `teamplay`, `match_over`).
#define SE_ARENA_GLOBALS(X)        \
  X(deathmatch, kTypeFloat)        \
  X(teamplay, kTypeFloat)          \
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
  X(arsenal_missile8, kTypeString) \
  X(clock_said, kTypeFloat)        \
  X(team_a, kTypeString)           \
  X(team_b, kTypeString)           \
  X(frags_a, kTypeFloat)           \
  X(frags_b, kTypeFloat)

// THE FIELDS after the form's, in the QC's order (the server finds `maxspeed`, `gravity` and
// `attack_finished` by name; a map's "light" key fills `light_lev`).
#define SE_ARENA_FIELDS(X)             \
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

// THE STRINGS the rules store or pass.
#define SE_ARENA_STRINGS(X)                                                                        \
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
  X(you_receive, "You receive ")

// THE FUNCTIONS by their QC names: the hooks, the spawn functions by class name, and every
// think, touch, use and blocked a field holds. Each is `void name(A&)` below.
#define SE_ARENA_FUNCTIONS(X)                                       \
  X(main_hook, "main")                                              \
  X(StartFrame, "StartFrame")                                       \
  X(PlayerPreThink, "PlayerPreThink")                               \
  X(PlayerPostThink, "PlayerPostThink")                             \
  X(ClientKill, "ClientKill")                                       \
  X(ClientConnect, "ClientConnect")                                 \
  X(PutClientInServer, "PutClientInServer")                         \
  X(ClientDisconnect, "ClientDisconnect")                           \
  X(SetNewParms, "SetNewParms")                                     \
  X(SetChangeParms, "SetChangeParms")                               \
  X(worldspawn, "worldspawn")                                       \
  X(info_player_deathmatch, "info_player_deathmatch")               \
  X(info_player_start, "info_player_start")                         \
  X(info_intermission, "info_intermission")                         \
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

#define SE_X(name, ...) g_##name,
enum Gl : int { SE_ARENA_GLOBALS(SE_X) kGlobalCount };
#undef SE_X
#define SE_X(name, ...) f_##name,
enum Fd : int { SE_ARENA_FIELDS(SE_X) kFieldCount };
#undef SE_X
#define SE_X(name, ...) s_##name,
enum St : int { SE_ARENA_STRINGS(SE_X) kStringCount };
#undef SE_X
#define SE_X(name, ...) fn_##name,
enum Fn : int { SE_ARENA_FUNCTIONS(SE_X) kFunctionCount };
#undef SE_X

// THE RULES' VIEW of the server's calls: Q's, plus the arena's own globals, fields, strings and
// functions by their enums.
struct A : Q {
  explicit A(pg::Vm& v) : Q(v) {}
  using Q::f;
  using Q::i;
  using Q::set_f;
  using Q::set_i;
  using Q::set_v;
  using Q::v;
  float f(int32_t e, Fd k) { return Q::f(e, own_field(k)); }
  int32_t i(int32_t e, Fd k) { return Q::i(e, own_field(k)); }
  Vec3 v(int32_t e, Fd k) { return Q::v(e, own_field(k)); }
  void set_f(int32_t e, Fd k, float x) { Q::set_f(e, own_field(k), x); }
  void set_i(int32_t e, Fd k, int32_t x) { Q::set_i(e, own_field(k), x); }
  void set_v(int32_t e, Fd k, Vec3 x) { Q::set_v(e, own_field(k), x); }
  float g(Gl k) const { return gf(own_global(k)); }
  int32_t ge(Gl k) const { return gi(own_global(k)); }
  void set_g(Gl k, float x) { set_gf(own_global(k), x); }
  void set_ge(Gl k, int32_t x) { set_gi(own_global(k), x); }
  int32_t s(St k) const { return str(k); }
  int32_t fn(Fn k) const { return Q::fn(k); }
};

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
constexpr float kMsgEffect = 4;  // the multicast buffer
constexpr float kSvcTempentity = 23;
constexpr float kTeSpike = 0, kTeGunshot = 2, kTeExplosion = 3, kTeLightning2 = 6, kTeBlood = 12;
constexpr int kEfMuzzleflash = 2, kEfDimlight = 8, kEfBlue = 64, kEfRed = 128;

// THE RULES with a field's or the server's name.
#define SE_X(name, ...) void name(A& q);
SE_ARENA_FUNCTIONS(SE_X)
#undef SE_X

// THE RULES one file calls in another.
// arena_util
void set_movedir(A& q);
void calc_move(A& q, Vec3 dest, float spd, int32_t done);
void use_targets(A& q);
void effect_head(A& q, float kind);
void write_vec(A& q, Vec3 v);
void effect_explosion(A& q, Vec3 org);
void effect_beam(A& q, Vec3 from, Vec3 to);
float crandom(A& q);
float min_of(float a, float b);
float number_of(A& q, int32_t s);
// arena_combat
void score_death(A& q, int32_t targ, int32_t attacker);
void damage(A& q, int32_t targ, int32_t inflictor, int32_t attacker, float points);
void telefrag(A& q, int32_t who);
void radius_damage(A& q, int32_t inflictor, int32_t attacker, float points, int32_t ignore,
                   int32_t how);
// arena_weapons
int32_t weapon_word(A& q, float w, float i);
float weapon_number(A& q, float w, float i);
float weapon_rank(float w);
float ammo_for(A& q, float w);
void weapon_show(A& q);
void weapon_set(A& q, float w);
void weapon_select_best(A& q);
void weapon_impulse(A& q);
void weapon_frame(A& q);
void weapon_precache(A& q);
// arena_items
void backpack_drop(A& q);
void power_frame(A& q);
void item_precache(A& q);
// arena_dmm4
void dmm4_give(A& q);
bool dmm4_shielded(A& q, int32_t p);
void dmm4_frame(A& q);
void dmm4_soaked(A& q, int32_t targ);
void dmm4_octa_taken(A& q);
void dmm4_octa_off(A& q);
void dmm4_pack_take(A& q);
void dmm4_precache(A& q);
// arena_player
void player_pain(A& q);
void player_die(A& q);
void jump_sound(A& q);
void swim_sound(A& q);
void player_precache(A& q);
// arena_glue
void say_all(A& q, float level, std::initializer_list<int32_t> text);
void say_to(A& q, int32_t to, float level, std::initializer_list<int32_t> text);
int32_t name_of(A& q, int32_t class_name, int32_t fallback);
void effect_send(A& q, Vec3 org, float set);
void effect_impact(A& q, Vec3 org, float count, float bleeds);
bool team_shoves(A& q, int32_t a, int32_t b);
bool same_team(A& q, int32_t a, int32_t b);
int32_t game_words(A& q, int32_t key, int32_t a, int32_t b, int32_t c, int32_t d);
int32_t game_variant(A& q, int32_t stem, int32_t a, int32_t b, int32_t c, int32_t d);
bool pellets_as_one_blow();
bool game_quiet(A& q);
int32_t death_words(A& q, int32_t targ, int32_t attacker);
void drop_on_death(A& q);
bool items_come_back();
void player_jump(A& q);
int32_t spawn_point(A& q);
void give_inventory(A& q);
void player_respawn(A& q);
// arena_world
bool teleport_takes(A& q, int32_t who);
void teleport_arrived(A& q, int32_t who);

}  // namespace shooters_edge::server::arena
