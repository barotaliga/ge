# Lag compensation by rewind: clean-room facts

A WORKING note of clean-room facts about lag compensation by rewind ("antilag") in the
QuakeWorld server (mvdsv, with the ktx mod's controls), its client (ezQuake) and the Quake II
modern servers, kept for Shooter's Edge's rewind of other players under a shooter's traces. A
reader session read QW-Group/mvdsv (with QW-Group/qwprot, its shared protocol header),
QW-Group/ktx, QW-Group/ezquake-source and actionquake/q2pro (AQtion's fork) at their default
branches (2026-11-08 to 2026-11-18) and wrote these facts in its own words with no code
(`clean-room` skill). Upstream Q2PRO (skullernet/q2pro) could not be fetched in that session:
every Quake II fact below is the AQtion fork's, where the feature lives in the Action game
module, not the engine. Each fact is marked **read** (the source does it), **derived** (follows
from read facts, working shown) or **measured** (none yet: nothing was run).

## 1. QuakeWorld server: the switch and what is rewound

1. **read** `sv_antilag` (serverinfo, default empty = off): 0 off; 1 rewinds only traces the
   mod marks; 2 also marks every `traceline` call automatically. There is no `sv_antilagexact`.
2. **read** A mod marks a trace by adding **64** to the "nomonsters" argument of `traceline` or
   `tracebox` (builtin 90), or by setting the bit **65536** in an entity's `flags`, which makes
   every push move of that entity (toss, bounce, fly, fly-missile physics) a rewound trace.
   Under 2 only `traceline` is marked automatically, not `tracebox`.
3. **read** Whose view a marked trace uses: its ignore-entity's, when that is a non-bot player,
   else its ignore-entity's `owner`'s, when that is a non-bot player; else the trace is not
   rewound. Bots are rewound as targets (sampled every server frame) but their shots never are.
4. **read** Rewound: player slots only, alive (a non-spectator with health above 0), spawned,
   with at least one sample. Monsters, doors, platforms and items never. Dead players are traced
   where they are. Spectators and the shooter are in the set but skipped (not solid; ignore-entity).
5. **read** Only the origin is rewound; the box is the player's current `mins`/`maxs` at the
   rewound origin. The blend from current to rewound is always full (no setting).
6. **read** The rewound set is computed once per incoming packet of the shooter, before that
   packet's commands run, and used by every marked trace for that shooter until the next packet.
7. **read** No enemy-only or team-mate rule; no statistic or command reports rewinds.

## 2. QuakeWorld server: the time and the history

1. **read** History: a ring of **128** origins per player, one per incoming move packet that
   advanced server time, stamped with the server time of that packet (bots: one per server
   frame). Emptied by the `setorigin` builtin and on every packet while `sv_antilag` is 0.
2. **derived** At 77 packets/s the ring covers 128 / 77 = 1.66 s. There is no ping cap, time cap
   or per-client minimum.
3. **read** Base time: the server time at which the newest frame this packet acknowledges was
   built; the ping is now minus that frame's real send time.
4. **read** `sv_antilag_no_pred` (serverinfo, default empty) non-zero: target = base. Otherwise
   target = base + lead, capped at the current server time; lead = 1 / `maxfps` (serverinfo,
   default 77; values under 20 or over 1000 read as 77) when the ping is under 0.02 s, else
   0.02 s. **derived** At 77 the short lead is 12.99 ms.
5. **read** Nothing from the client enters the target time (no interpolation delay, no frame time,
   no userinfo). **derived** `sv_minping`'s delay is not added.
6. **read** Target later than the target player's newest sample: current origin + current
   velocity × min(target − newest sample time, 0.02 s). Otherwise a linear blend by time between
   the bracketing samples. One sample only: that sample. No teleport detection beyond the
   `setorigin` reset.
7. **derived** Past the oldest sample: with a wrapped ring, a straight-line extrapolation beyond
   the oldest sample along newest-to-oldest (not clamped); with an unfilled ring, the lookup
   reads outside the ring and the result is undefined.

## 3. QuakeWorld server: projectiles

1. **read** `sv_antilag_projectiles` (serverinfo, default empty) acts only with `sv_antilag` 2.
2. **derived** It covers every toss, bounce, fly and fly-missile entity (rockets, grenades,
   nails, and also tossed items, backpacks, gibs) whose `owner` is a non-bot player.
3. **read** Every physics frame of the flight is rewound, not only the first; no age rule.
   **derived** The time is the owner's latest rewound set (their view at their last packet).

## 4. QuakeWorld: what players see and the debug tools

1. **read** The ezQuake client sends nothing that changes the rewind (no userinfo key, no
   opt-out). The server prints nothing about antilag on connect.
2. **read** ktx: the player command `antilag` votes to toggle `sv_antilag` between 0 and 2
   (outside a match; 2 players needed unless an admin; passes on an admin's vote or a majority of
   `k_vp_antilag` percent, bounded 51 to 100, at least 2 votes); broadcasts "Antilag mode On/Off
   by majority vote" or "by admin veto"; during a match it only reports the mode. "On" means 2.
   Every ktx user mode sets `sv_antilag 2`; ktx never marks traces itself, so 1 is unsupported.
   The match countdown shows "Antilag N" when non-zero.
3. **read** ezQuake `cl_fakeshaft 2` aims the player's own drawn lightning beam from the command
   sent two packets earlier; cosmetic; announced in chat.
4. **read** `sv_debug_antilag` (default 0, checked at map start): offers the extension
   `MVD_PEXT1_DEBUG_ANTILAG` (MVD1 bit 16) and writes an MVD hidden block of type 0 after each
   packet that had rewound players: 4-byte length, 2-byte type, then shooter slot (1), record
   count (1), incoming sequence (4), server time (float), target time (float); per record 27
   bytes: slot (1, +128 when a client position is included), rewound position (3 floats), msec
   (1), model (1), client position (3 floats). Little-endian.
5. **read** The client position comes from client message byte 201, sub-type 1; it is kept for
   the demo only. **derived** ezQuake at this release never sends it: `cl_debug_antilag_send`
   (default 0) only requests the extension and warns when the server refuses.
6. **read** ezQuake demo viewers, all default 0, acting for the tracked player's blocks:
   `cl_debug_antilag_view` (draw others at 0 normal, 1 rewound, 2 client position),
   `cl_debug_antilag_ghost` (a 0.2-alpha second copy at 0/1/2 when it differs from `_view`),
   `cl_debug_antilag_lines` (2-pixel lines, red rewound to current, green client to current, blue
   rewound to client, pairs chosen by `_view`; needs `scr_autoid`), `cl_debug_antilag_self`
   (move the tracked player too).
7. **read** Related: `MVD_PEXT1_HIGHLAGTELEPORT` (MVD1 bit 2; ezQuake `cl_pext_lagteleport`,
   default 1) turns a high-ping player's not-yet-acknowledged moves through a teleport's yaw
   change. ezQuake also carries an "antilag 1 prototype" (client weapon prediction, client-flown
   projectiles, accurate timings: `cl_pext_weaponprediction`, `cl_pext_simpleprojectiles`,
   `cl_pext_accuratetimings`, `cl_nopred_weapon`, `cl_predict_beam`, `cl_predict_buffer`) that
   mvdsv at this release does not offer.

## 5. Quake II (AQtion's q2pro fork)

1. **read** `sv_antilag` (serverinfo, default 1), `sv_antilag_interp` (serverinfo, default 0),
   no max-ping or max-rewind setting. The `gamesettings` command prints both.
2. **read** History: 64 samples per player (origin, `mins`, `maxs`, stamp), one per server
   frame for every in-use client. **derived** 6.4 s at `sv_fps` 10, 1.6 s at 40; never cleared.
3. **read** Target = the shooter's sub-frame clock (level time at frame start + the `msec` of the
   commands run since) − ping (engine's, ms / 1000), clamped to no earlier than level time −
   **0.25 s** (a constant). With `sv_antilag_interp` 1 every stamp is recorded one server frame
   late, so rewinds land one frame further back; server extrapolation (`cl_xerp`) is then off.
4. **read** Rewound: other living players, origin blended linearly by time, box from the earlier
   sample (crouch is rewound); relinked for the shot, restored after. Inside the current frame
   the origin blends from the newest sample to the current origin by the target's own command
   time. **derived** The highest client slot is never rewound (loop bound); a target older than
   the history places the player at history slot 1; teleports and respawns are blended across.
5. **read** Hitscan only: MK23, dual pistols, MP5, M4, M3 shotgun (one rewind for all pellets),
   handcannon, sniper rifle. Grenades, knives, kicks and projectiles are not rewound.
6. **read** Opt-out: userinfo `cl_antilag` (empty or above 0 = in; 0 or below = out), also via the
   fork's cvar sync (default 1; **derived** empty counts as out there). It opts out the player's
   own shots only. On a change while `sv_antilag` is on: "YOUR CL_ANTILAG IS NOW SET TO 0/1".
7. **read** The fork's server browser colours a server cyan for `sv_antilag` above 0 (after the
   ping colours).
