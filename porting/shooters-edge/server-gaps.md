# Server settings: the gap list against a classic server (working note)

WORKING AREA: deleted when porting ends. Which settings of a stock classic server install
(nQuake's server package: MVDSV's `mvdsv.cfg`, KTX's `ktx.cfg`, `matchless.cfg` and the mode
configs) our servers have, which a round added, and the ranked candidates for the next rounds.
The behaviour of each is in `server-settings.md` (the clean-room spec, written from the GPL
source by a spec session); a building session reads only that, never the source.

Homes, in order of preference: **engine** (`engine/server`, every game's server), **plugin**
(`plugins/shooter`, any first-person shooter), **game** (Shooter's Edge: its classic endpoint
`games/shooters_edge/net`, its mod server `games/shooters_edge/server`, its lobby).

How to run a round: take the top of "Ranked candidates", check `server-settings.md` has the
section (if not, have a spec session add it from the source), build it in the highest home it
fits, move its row to "Done", and re-rank. Stop when nothing left is worth its cost.

## Done

| Classic setting | Ours | Home | Round |
| --- | --- | --- | --- |
| `hostname` | `sv_hostname` | engine | before |
| `maxclients` | `sv_maxclients` | engine | before |
| `fraglimit`, `timelimit`, `teamplay`, `deathmatch`, `samelevel` | the same (game rules) | game | before |
| `sv_gravity`, `sv_maxspeed` | the same | plugin (round 1 moved them into its table) | before |
| `password`, `spectator_password` | `sv_password`, `sv_spectatorpassword` | engine | before |
| the master list (`setmaster`) | `sv_rendezvous`, `sv_visibility` | engine | before |
| `k_ml_*` map rotation | `sv_maprotation` | engine | before |
| `k_count` | `sv_countdown`, `sv_minplayers` on the listen server's lobby (not yet on the dedicated server's: the lobby thread owns that) | game (lobby) | before |
| `sv_mintic`, `sv_maxtic`, `sv_maxvelocity`, `sv_speedcheck`, `sv_minpitch`, `sv_maxpitch` | the same | plugin (`plugins/shooter/settings.hpp`) | 1 |
| `sv_stopspeed`, `sv_spectatormaxspeed`, `sv_accelerate`, `sv_airaccelerate`, `sv_wateraccelerate`, `sv_friction`, `sv_waterfriction`, `edgefriction` | the same | plugin | 1 |
| `serverinfo pm_ktjump`, `pm_bunnyspeedcap`, `pm_slidefix`, `pm_airstep`, `pm_pground` | the same; written into the server info when not at the default | plugin | 1 |
| `serverinfo maxfps` | `maxfps` | game (endpoint) | 1 |
| `floodprot`, `floodprotmsg` | `sv_floodprot`, `sv_floodprotmsg` (default `10 1 1`) | engine (`engine/server/guards.hpp`), applied by the endpoint | 1 |
| `timeout` | `sv_timeout` (65 s; was a fixed 30 s), everyone told `<name> timed out` | engine setting, endpoint | 1 |
| `pausable` | `pausable` (default 1 so a listen server pauses; a public server sets 0) | game (mod server) | 1 |
| `sv_maxrate` | `sv_maxrate` | game (endpoint) | 1 |
| `allow_download`, `_skins`, `_models`, `_sounds`, `_maps`, `_other` | the same | game (endpoint; the dedicated server offers no files yet) | 1 |
| `sv_downloadchunksperframe` | the same | game (endpoint) | 1 |
| `sv_antilag` (server info) | `sv_antilag` | game (endpoint) | 1 |
| voice relay | `sv_voip` | game (endpoint) | 1 |
| `maxspectators`, `sv_forcespec_onfull`, `sv_reconnectlimit`, `sv_kicktop`, `sv_unfake`, `sv_minping` | `sv_maxspectators`, `sv_fullspectate`, `sv_reconnectwait`, `sv_colorkick`, `sv_unfake`, `sv_minping` | endpoint | M58 rounds 1-2 |
| `sv_spectalk` | `sv_spectalk` (mutes spectators only in a match since round 7) | engine (`server/chat.hpp`) | M58 2, 7 |
| `k_minrate` | `sv_minrate` (no alias: the match mod reads its own) | engine (`server/rate.hpp`) | M58 7 |
| `sv_sayteam_to_spec`, `k_sayteam_to_spec` | `sv_sayteam_to_spec` 0..3 by phase | engine (`server/chat.hpp`) | M58 7 |
| `k_overtime`, `k_exttime` | `sv_overtime`, `sv_overtime_minutes` | engine (`overtime.hpp`), endpoint over the mod's clock | M58 7 |
| `k_idletime` | `sv_idlekick` | engine lobby | M58 7 |
| `k_membercount`, `k_lockmode`/`k_exclusive` | `sv_teamsize_min`, `sv_lockjoin` (the lobby has two teams, so no `k_lockmin`/`k_lockmax`) | engine lobby | M58 7 |
| `k_prewar` | `sv_prewar` | engine lobby, endpoint strips the buttons | M58 7 |
| `k_defmap` | `sv_homemap` | engine (`homemap.hpp`), endpoint | M58 7 |
| `k_timetop` | `sv_timelimit_max` (a cap on votes) | engine (votes) | M58 7 |

## Ranked candidates (best value for the effort first)

| # | Classic setting | What it adds | Home | Spec |
| --- | --- | --- | --- | --- |
| 1 | `k_freeze`, `k_sready` | Doors and lifts frozen before the match; a glow on the unready | engine lobby phase + game | §17 |
| 2 | `dq`, `dr`, `dp`, `k_dis`, `k_pow`, `k_fallbunny`, `k_spw` | Drop quad/ring/pack, discharge, powerups, fall damage, spawn choice | game (the arena's C++ rules) | §19 |
| 3 | `allow_download_pakmaps`, `allow_download_demos`, `sv_maxdownloadrate` | Only once the dedicated server serves files from the game folder | endpoint | §7 |
| 4 | `k_short_gib`, `k_bzk`, `k_frp`, `k_highspeed` | Novelty rules | game (mod) | §20 |
| 5 | the `overtime`, `overtimeup`, `silence`, `prewar` commands | Players changing the settings above outside a match (votes do it today) | engine lobby | §6, §14, §17 |

Larger features the configs switch on, each its own row in `docs/goals/M53.md` rather than a
setting: rcon (`rcon_password`, `sv_crypt_rcon`, `sv_rconlim`, M53f), ban and VIP lists
(`ban_ip.cfg`, `vip_ip.cfg`, M53f), server-side demo recording (`sv_demodir`, `sv_demofps`,
`demo_tmp_record`, `sv_demoMaxDirSize`, M53g), logs (`sv_logdir`, `logplayers`, `logrcon`), votes
and admins (`k_vp_*`, `k_admins`, `k_master`, M53f/h), map entity overrides (`sv_loadentfiles`),
practice mode, race mode, bots (`k_fb_*`: ours are `sv_bots`).

Not worth building: `sv_getrealip`, `sv_use_dns`, `sys_select_timeout`, `sys_restart_on_error`,
`sv_progtype`, `fs_cache`, `sv_demoUseCache`, `fpd` (a classic client proxy's switches),
`check_maps`, `k_666` (not read by the match mod any more), `k_autoreset` (likewise).
