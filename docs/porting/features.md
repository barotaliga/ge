# Small features: a catalogue

The small things a player notices: an aid, a filter, an effect that tells them something, a check
that keeps a match fair. Each is one row: what it is, where its shared part lives, what a game adds,
which games have it, and its default. A NEW game goes down this list and, for each row, either fills
the game's hook (the feature then works in it) or names the row a gap in its row file. A feature
one game had and another did not is on in its own game and off in the others
(`docs/porting-games.md` § Improvements one game had).

"Both" is the first classic game and Quake II. "Hook" is what a new game supplies.

## Play

| Feature | Shared | Hook | Games | Default |
| --- | --- | --- | --- | --- |
| The easy jump (`cl_speedjump`) | `plugins/shooter/speedjump_assist.hpp` | its move's air law; the share by `speedjump-share` | both | off |
| The blast-jump helper (`sv_rjump`) | `plugins/shooter/rjump_assist.hpp`, `server/rjump_rules.hpp` | its rocket's numbers | first | off |
| Aim help (`sv_aimhelp`) | `plugins/shooter/aim_help.hpp`, `aim_turn.hpp`, `server/shot_help.hpp` | its hitscan weapons; `aim-bend` | first | off |
| Player aids granted by vote or admin | `engine/server/aids.hpp` | which aids it has | both | none granted |
| The quick weapon (`cl_quickweapon`) | `plugins/shooter/quick_weapon.hpp`, `client/quick_fire.hpp` | its weapon keys (`hud::KeyedWeapons`) | both | off |
| Weapon groups (`cl_weapongroups`) | `plugins/shooter/weapon_groups.hpp`, `client/weapon_groups.hpp` | its groups in the game row | both | off |
| Weapon preselection, hiding no-ammo lines | `client/weapon_choice.hpp`, `plugins/shooter/weapon_lines.hpp` | its weapon commands | both | as the game |
| Midair scoring | `plugins/shooter/midair.hpp` | its blast weapons | first | a mode |
| Lag compensation (`sv_antilag` 1 or 2, `sv_antilag_projectiles`, `sv_antilag_max`, `cl_antilag`) | `net/qw_antilag.hpp` | its hitscan and, with 2 and projectiles, its missiles (`lag_place`, the rewound box); its server's frame times, `sv_antilag_interp` | both | off |
| Other players run forward (`cl_predict_players`) | `plugins/shooter/predict.hpp`, `other-moves` | its move | both | on |
| Co-op companions | `bots/companion.hpp` | its rules' buttons and exits | both | in co-op |

## Seeing

| Feature | Shared | Hook | Games | Default |
| --- | --- | --- | --- | --- |
| Weapons in other players' hands (`r_drawvweps`, `cl_novweps`) | `plugins/morph/carry.hpp`, `plugins/classic/vweps.hpp`, `client/world_models.cpp` | its model list and weapon index; stand-ins from its pack | both | on |
| Arrivals: spawn, respawn, teleport effects | `client/warps.hpp`, `app/effects.hpp`, `render/morph.hpp`'s `MorphWarp` | its own arrival effect (Quake II's lattice of specks, never another game's fog); `ce_warps` adds dust | both | the game's own |
| An item's return | `app/effects.hpp` | its item-return event | Quake II (a burst of greens) | the game's own |
| A power's shell, an item's pulse (`r_powershells`, `r_itempulse`) | `client/q2_look.hpp` | its powers' bits | both | on in Quake II, off in the first |
| The landing dip (`cl_landdip`) | `engine/camera/first_person.hpp` | its fall kick | both | 0 in Quake |
| Gib, body and flame filters (`cl_gibfilter`, `cl_deadbodyfilter`) | `client/model_filters.hpp`, `plugins/classic/app/look.hpp` | its gib bit and death frames | both | off |
| Bodies burning in lava and slime (`ce_burning`) | `client/burning.hpp`, `render/body_burn.hpp` | its contents codes through `brush::cell_at` | both | the switch |
| Bullet holes, scorch, blood (`ce_decals`, `ce_hits`, `ce_blood`) | `engine/detail/decals.hpp`, `hits.hpp` | its shots' events | both | the switches |
| Crosshair marks, colour, size | `engine/hud/crosshair.hpp`, `hud/crosshair.hpp` | its own crosshair meaning (`own` in Quake II) | both | the game's own |
| Speed, clock, frame rate, net graph (`show_speed`, `scr_clock`, `scr_showfps`, `r_netgraph`) | `engine/hud/corner_readouts.hpp`, `frame_graph.hpp`, `net_graph.hpp` | nothing | both | off |
| The HUD's layout and looks (`hud_style`, `hud_` cvars) | `engine/hud/layout.hpp`, `hud/layout.hpp` | its status snapshot | both | its classic bar |
| Screenshots and the gallery | `engine/capture/screenshot.hpp`, `gallery.hpp` | a key and a folder | both | on |

## Talking

| Feature | Shared | Hook | Games | Default |
| --- | --- | --- | --- | --- |
| Place names (`loc_dist`) | `plugins/classic/locs.hpp`, `client/place_names.hpp` | its loc files, else named off its items | both | the game row's reach |
| Team chat's codes and canned lines (`%w`, `%b`, `tp_name_*`, `f_` triggers) | `client/team_chat.hpp`, `team_point.hpp` | its weapon names by its keys | both | on |
| What team messages mean, for bots and people | `plugins/shooter/team_talk.hpp` | its wording (`say_text`, `parse_say`) | both | on |
| Chat ignore lists | `engine/console/chat_ignore.hpp`, `client/chat_ignore.hpp` | its chat lines, hooked into its remote path | both | empty |
| Voice chat and `say_near` (`sv_voip`) | `engine/net/voice.hpp`, `net/qw_talk.hpp` | its protocol ids, what a team is | both on our servers | off |

## Fair play

| Feature | Shared | Hook | Games | Default |
| --- | --- | --- | --- | --- |
| The speed check and its score | `plugins/shooter/client.hpp`, `net/qw_cheats.hpp`, `engine/net/guard.hpp` | its command timing | first | on |
| Fast clocks warned and kicked (`sv_speedwarn`, `sv_speedkick`) | `engine/server/skew.hpp` | its command clock | both | the game's |
| Inputs off range, off grid, floods, future views | `engine/net/guard.hpp` | its input fields' ranges | engine protocol | on |
| The map's checksums | `net/qw_mapsum.hpp`, `server/md4.hpp` | its checksum rule | first | on |
| A flagged player named in the match report, banned by account | `engine/account/matches.hpp` | its report | both | on trusted servers |
| Chat floods, rename limits, idle drops, IP limits | `engine/server/guards.hpp`, `names.hpp`, `idle.hpp`, `admission.hpp` | nothing | both | the game's |

## Watching

| Feature | Shared | Hook | Games | Default |
| --- | --- | --- | --- | --- |
| The live map (`demo_map_timers`) | `engine/overview/`, `livemap/live_map.hpp`, `plugins/brush/app/overview_floors.hpp`, `plugins/morph/overview_mesh.hpp` | items and respawn times, players' colours, a demo's entities | both | on TV and demos |
| Kill highlights on the seek bar | `engine/replay/highlights.hpp`, `client/demo_highlights.hpp`, `demo_scan.hpp` | every player's stats in its multiview | both | on |
| Autotrack | `client/demo_autotrack.hpp` | the same stats | both | on in multiview |
| The hub's radar | `engine/replay/radar.hpp`, `client/demo_radar.hpp` | its multiview read | both | on |
| Following the top player as a spectator | `net/qw_follow.hpp` | its spectator commands | first | a command |
| A demo as a video, captions | `engine/capture/`, `client/demo_video.hpp`, `demo_caption.hpp` | its recording | both | a command |
| The demo benchmark | `client/timedemo.hpp` | its recording | both | a command |

## Gaps (rows a game lacks today)

- Quake II: the blast-jump helper, aim help and midair were built on Quake's endpoint;
  each is a lift to the shared layer with the default off in Quake II (the M18v13 census in
  `docs/goals/M18v.md` is the current list).
- Quake: footsteps (it ships no step sounds; remix carries them).
- Both: an item's return as an event in Quake II's recordings.
