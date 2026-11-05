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

## Ranked candidates (best value for the effort first)

| # | Classic setting | What it adds | Home | Spec |
| --- | --- | --- | --- | --- |
| 1 | `maxspectators`, `sv_forcespec_onfull` | Spectator slots counted apart from players (today a spectator takes a player slot and `sv_maxspectators` is not read), and a full server seating a joiner as a spectator. Changes the mod's slot count (players + spectators), so digests move | engine setting exists; endpoint | §5 |
| 2 | `k_minrate` | `sv_minrate`: the floor beside `sv_maxrate`, the client told and its `rate` stuffed | game (endpoint) | §8 |
| 3 | `sv_spectalk`, `sv_sayteam_to_spec` (`k_spectalk`, `k_sayteam_to_spec`) | Who hears spectators and who hears team chat, by match phase | engine routing rule + game | §6 |
| 4 | `k_overtime`, `k_exttime` | Extra time, sudden death, tie-break, golden frag when tied at the time limit | game (mod) with an engine hook on the match phase | §14 |
| 5 | `k_idletime` | Kick players not ready in warmup after N seconds once half are ready | engine lobby (`engine/server/match`) | §13 |
| 6 | `k_defmap` (reset on empty) | Back to the home map 10 s after the last player leaves | engine (`sv_homemap`) + game map change | §16 |
| 7 | `k_membercount`, `k_lockmin`, `k_lockmax`, `k_exclusive` | Team start conditions and locking the roster at the match's start | engine lobby | §15 |
| 8 | `k_prewar`, `k_freeze`, `k_sready` | What warmup allows: firing, moving doors, a glow on the unready | engine lobby phase + game | §17 |
| 9 | `k_timetop` | A cap on the time limit a player may vote | engine (votes, M53f) | §18 |
| 10 | `sv_reconnectlimit` | Throttle a still-connected client's reconnect storm (no reply) | endpoint | §4 |
| 11 | `dq`, `dr`, `dp`, `k_dis`, `k_pow`, `k_fallbunny`, `k_spw` | Drop quad/ring/pack, discharge, powerups, fall damage, spawn choice | game (baseline mod, QC) | §19 |
| 12 | `sv_kicktop`, `sv_unfake` | Kick colour spammers; stop faked chat lines | endpoint | §11 |
| 13 | `sv_minping` | A ping floor by delaying a client's packets | endpoint | §10 |
| 14 | `allow_download_pakmaps`, `allow_download_demos`, `sv_maxdownloadrate` | Only once the dedicated server serves files from the game folder | endpoint | §7 |
| 15 | `k_short_gib`, `k_bzk`, `k_frp`, `k_highspeed` | Novelty rules | game (mod) | §20 |

Larger features the configs switch on, each its own row in `docs/goals/M53.md` rather than a
setting: rcon (`rcon_password`, `sv_crypt_rcon`, `sv_rconlim`, M53f), ban and VIP lists
(`ban_ip.cfg`, `vip_ip.cfg`, M53f), server-side demo recording (`sv_demodir`, `sv_demofps`,
`demo_tmp_record`, `sv_demoMaxDirSize`, M53g), logs (`sv_logdir`, `logplayers`, `logrcon`), votes
and admins (`k_vp_*`, `k_admins`, `k_master`, M53f/h), map entity overrides (`sv_loadentfiles`),
practice mode, race mode, bots (`k_fb_*`: ours are `sv_bots`).

Not worth building: `sv_getrealip`, `sv_use_dns`, `sys_select_timeout`, `sys_restart_on_error`,
`sv_progtype`, `fs_cache`, `sv_demoUseCache`, `fpd` (a classic client proxy's switches),
`check_maps`, `k_666` (not read by the match mod any more), `k_autoreset` (likewise).
