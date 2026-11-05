# Server settings: what the classic server and match mod do

WORKING AREA: deleted when porting ends. A clean-room description, in our words, of what the classic server program (MVDSV) and the classic match mod (KTX) do for each setting below, written from their source by a spec session; the building session reads only this. No code, no internal identifiers.

Read from MVDSV at commit `dcc8355` and KTX at commit `f316310` (both from github.com/QW-Group). "nQuake" means the values in nQuake's shipped server configs (`mvdsv.cfg`, `ktx.cfg`, `matchless.cfg`, `configs/usermodes/*`).

Terms used throughout:
- **Server program** is the network server. **Match mod** is the game code it loads. When both handle something, the mod usually goes first and the server's own version only runs if the mod declines. Each section says which one wins.
- **Match phase**: *prewar* (no match yet, players warm up and type `ready`), *countdown*, *in game*, *intermission/over*. **Matchless** (`k_matchless 1`) is a plain FFA server with no ready step. In matchless mode the clock starts as soon as the first player joins.
- **Broadcast** means every client sees it. **Private** means only the named client sees it. Message texts are quoted exactly, except that `\n` is a newline and `N`/`<name>` are filled in. Some words are drawn in the classic "red text" (high-bit) font. The text is the same; only the glyphs differ.
- "Program default" is the value when no config sets it. A match-mod setting with no registered default reads as 0 (or as empty).

---

## 1. `floodprot` and `floodprotmsg` (chat flood protection)

**Where.** Server program. The match mod replaces it with its own (see "With the match mod" below).
**Default.** Program: `floodprot 4 4 10`, with an empty `floodprotmsg`. nQuake: `floodprot 10 1 1`, `floodprotmsg "quiet!"`.
**Values.** `floodprot <N messages> <per S seconds> <silence T seconds>`. All three must be positive integers and N is at most 10. With no arguments it prints the current settings. `floodprotmsg "<text>"` sets the text sent to a flooder.

**Behaviour (server program).**
- Only `say` and `say_team` count, from players and spectators alike. Nothing else is counted: not commands, not name changes, not voice.
- Each client keeps a ring of the times of its last 10 accepted messages, in real (wall-clock) time, unaffected by pause. A new message is checked against the time of the N-th most recent accepted message. If that time is less than S seconds ago, the new message is the (N+1)-th inside the window. It is then dropped and the client is silenced until now + T.
- Ring slots that were never filled count as "never", so a fresh client cannot flood until it has sent N messages.
- On the message that triggers the silence, the flooder privately sees `FloodProt: <floodprotmsg>` if a message is set, otherwise `FloodProt: You can't talk for T seconds.` Nobody else is told.
- While silenced, every say or say_team is dropped and the sender sees `You can't talk for N more seconds` (the remaining time, rounded down). Talking while silenced does not extend the silence. Blocked messages are not added to the ring.
- The silence uses the same per-client "muted until" time as the admin `mute` command. So a silenced player is also refused name changes (`You can't change your name while you're muted`) and voice.
- If the client disconnects while silenced, the remaining silence is stored against its IP address and put back when it reconnects. This is shared with mute, and the server keeps at most 512 such entries.

**With the match mod.** The mod handles all say and say_team traffic, so the server-side check never runs. The mod has its own flood check with the same ring logic, configured by levels:
- `k_fp` sets the level for players (program default 1) and `k_fp_spec` sets it for spectators (program default 3). Level 1 is 9 messages per 1 s with a 1 s silence. Level 2 is 4 per 1 s with a 5 s silence. Level 3 is 5 per 3 s with a 7 s silence.
- The mod's ring holds at most 9 messages, and it uses game time.
- The mod skips the check while the game is paused, but it still records message times.
- Its texts are `FloodProt: You can't talk for T seconds.`, which ignores `floodprotmsg`, and `You can't talk for N more seconds`, where N is the remaining time rounded down plus 1.
- When `k_fp` changes, the mod also runs `floodprot` with the matching three numbers, for servers that do not pass chat to it.
- Each game-mode preset runs `floodprot 10 1 1`, `k_fp 1` and `k_fp_spec 1`.
- Admins cycle the levels with the `fp` and `fp_spec` commands. Each change is broadcast as `floodprot level <lvl> (<N> <S> <T>) <Low|Medium|High>`.

**Questions an implementer would ask.**
- *Are spectators included?* Yes. In the mod they have their own level.
- *Is it a sliding window?* Yes. It always compares against the N-th most recent accepted message, not against fixed buckets.
- *Does the silence survive a reconnect?* In the server program, yes, by IP. In the mod, the per-player state is apparently cleared when the slot is reused.
- *Can a config turn it off?* Not with the server command, which rejects zeros. The only route is the mod's own handling.

## 2. `timeout` and `zombietime` (dropping dead connections)

**Where.** Server program.
**Default.** `timeout`: 65 in the program and in nQuake. `zombietime`: 2, which nQuake does not set.
**Values.** Both are in seconds.

**Behaviour.**
- Once per server frame, the server looks at every client slot that has started connecting. If the last *valid packet* from that client is more than `timeout` seconds old, the client is dropped.
- "Heard" means any well-formed sequenced packet from that address and port. A live client sends movement packets every frame, even when idle, standing still or paused. So this only catches dead links. It is not an idle kick.
- Everyone sees `<name> timed out` (high priority). Then the mod's disconnect hook runs. With the match mod it broadcasts `<name> left the game with N frags`, or for a spectator `Spectator <name> left the game`.
- A timed-out slot is freed immediately, with no zombie period.
- A client dropped any other way (it quit, was kicked, or the server dropped it) is sent a disconnect message, and its slot becomes a **zombie** for `zombietime` seconds. During that time the slot is still counted as taken for capacity, and any final reliable data can be resent. After that the slot is free.
- Bots are never timed out.
- Side effect: if the server is paused and no non-spectator slots are left, it unpauses and broadcasts `Pause released since no players are left.` The mod can veto this.

**Questions.**
- *Does a long map load count as silence?* Yes. A load longer than `timeout` drops the client.
- *Is a timed-out client throttled on reconnect?* No. Its slot is freed at once (see 4).

## 3. `pausable` (who may pause)

**Where.** Server program, which the match mod overrides because the mod owns the `pause` command.
**Default.** 0 in the program and in nQuake.

**Behaviour (server program alone).**
- A client's `pause` toggles the pause.
- If `pausable` is 0, the client sees `Pause not allowed.`
- A spectator sees `Spectators can not pause.`
- Otherwise everyone sees `<name> paused the game` or `<name> unpaused the game`, and every client is told the new pause state.
- There is no rate limit and no countdown.

**Behaviour (with the match mod).**
- `pause` is available to players, and to spectators who are admins.
- Outside matchless mode, `pause` does nothing unless a match is *in game*. During prewar, countdown and intermission it is silently ignored.
- Pausing:
  - If a pause is already scheduled, the requester sees `Pause already in progress.`
  - If 3 seconds or less of the match remain, the requester sees `Too late to pause. Please wait for match to finish.`
  - If `pausable` is 0, a non-admin may still pause if the server has a `matchtag` (or `k_pause_without_matchtag` is 1) and the player has pause requests left. Each player gets 3 per match, reset when the match starts, and this use spends one. Otherwise the requester sees `Pause is not allowed`.
  - Once accepted, the pause is scheduled 3 s ahead. Every client sees a centre-print `pausing` with a seconds count and hears a click each second. Then everyone sees `<name> paused the game. He has N remaining request(s).` and the game freezes.
- Unpausing:
  - Anyone allowed to use the command may unpause. `pausable` is not checked.
  - Everyone sees `<name> unpaused the game (will resume in 3 seconds)`, then a centre-print `unpausing` counting down in real time, with clicks.
  - Then everyone sees `game unpaused`.
  - A second unpause during the countdown shows the sender only `Unpause is pending, N second(s)`.
- If the match ends while paused, the game unpauses at once with `game unpaused`.

**Questions.**
- *Rate limit?* Only the 3-request budget when `pausable` is 0, plus the 3 s lead-in.
- *Does the flood check affect pausing?* No. The mod's command flood check is skipped while paused, so the pauser can always unpause.

## 4. `sv_reconnectlimit` (reconnect throttle)

**Where.** Server program.
**Default.** Program 0, which is off. nQuake 5.
**Values.** Seconds.

**Behaviour.**
- A new connection request is matched against existing slots. A slot matches when it has the same IP and either the same client port tag (qport) or the same UDP source port. Free slots are skipped.
- If a matching slot exists, the server looks at how long ago that slot's "connection start" was. If that is less than `sv_reconnectlimit` seconds, the request is refused.
- The "connection start" is reset each time the client:
  - first loads a level after connecting, and again on every map change;
  - switches between player and spectator;
  - is dropped, which makes the slot a zombie.
- A slot that has connected but not yet asked for the level has no start time. It reads as 0 seconds, so it always counts as "too soon" while the limit is on.
- A refused client gets **no reply at all**. Its own retry loop simply resends. The server console logs `<address>:reconnect rejected: too soon`.
- If the request is not refused, the old slot is dropped silently (no disconnect message is sent to the old connection) and freed, and the connection goes ahead as normal.

**Questions.**
- *Does it throttle a normal quit-and-rejoin?* Only within `zombietime` (2 s) of the quit. After that the zombie slot is free and no longer matches. A timed-out slot is freed at once and never matches. In practice it throttles reconnect storms from a client that is still connected.

## 5. `maxspectators` and `sv_forcespec_onfull` (slots, and full-server fallback)

**Where.** Server program.
**Default.** `maxclients`: 24 in the program, 32 in nQuake. Each mode preset in the match mod sets 8, and the mode configs adjust it from there. `maxspectators`: 8 in the program, 12 in nQuake. `maxvip_spectators`: nQuake 4. `sv_forcespec_onfull`: 2 in the program, 1 in nQuake.

**Behaviour.**
- There are 32 slots in all. The values are clamped so that `maxclients` + `maxspectators` (+ VIP spectators) is at most 32. Values above that are cut back when checked.
- Slots are counted by role. Every non-free slot counts, including zombies. Non-spectators count as players, spectators count against `maxspectators`, and VIP spectators count separately.
- A player is admitted while players < `maxclients`.
- A spectator is admitted while spectators < `maxspectators`, and only with the correct spectator password if one is set.
- When a non-spectator connects and players are full, but a spectator slot is free:
  - mode 1: the client becomes a spectator unless its userinfo `svf` has bit value 2 ("never force me");
  - mode 2: the client becomes a spectator only if its `svf` has bit value 1 ("force me on full");
  - mode 0: never.
- A forced client receives the connectionless print `server is full: connecting as spectator`, gets `*spectator 1` set, and is admitted. This skips the spectator password check.
- Otherwise the client receives `server is full`.
- If no slot is free at all, the client receives `server is full` regardless.

**Questions.**
- *Is the forced spectator promoted later?* No. It stays a spectator until it rejoins.

## 6. `sv_sayteam_to_spec`, `k_sayteam_to_spec`, `k_spectalk` (who hears whom)

**Where.** `sv_spectalk` and `sv_sayteam_to_spec` belong to the server program. The match mod's chat handler reads them, and the mod's `k_` settings overwrite them.
**Default.**
- `sv_spectalk`: 1 in the program.
- `sv_sayteam_to_spec`: 1 in the program, 0 in nQuake. Under the match mod it is overwritten every frame from `k_sayteam_to_spec`.
- `k_sayteam_to_spec`: 0 in the program, 1 in nQuake.
- `k_spectalk`: 0 in the program and in nQuake. The mode presets also set 0; some CTF and RA mode configs set 1.

**Routing.**
- **Spectator `say`**:
  - If `sv_spectalk` is 1, everyone hears it, prefixed `<name>:`.
  - If `sv_spectalk` is 0, only spectators hear it, prefixed `[SPEC] <name>:`.
- **Spectator `say_team`**: only spectators hear it, prefixed `[SPEC] <name>:`.
- **Player `say_team`**:
  - The sender always sees its own message.
  - In a team mode (or CTF), team-mates hear it. In coop, everyone does. In other modes nobody else does.
  - Prefix: `(<name>):`.
  - A spectator also hears it only if all of these hold:
    - `sv_sayteam_to_spec` is 1;
    - the message contains byte 13 (carriage return). Clients insert this byte when expanding their formatted team-message macros, so plain typed team chat is private;
    - the spectator is tracking a player on the sender's team, or, if tracking nobody, the spectator's own team key matches the sender's.
- **Player `say`**: everyone hears it, prefixed `<name>:`.

**What the match mod sets.**
- At match start, `sv_spectalk` is set to `k_spectalk` (forced to 1 in coop). When `k_spectalk` is 0, the mod also sets bit 64 in serverinfo `fpd`, which tells proxies to mute spectators to players.
- At match end, `sv_spectalk` goes back to 1 and the bit is cleared. So **outside a match, spectators can always talk to players**.
- `k_sayteam_to_spec` sets `sv_sayteam_to_spec` as follows:
  - 0: never;
  - 1: only while a match is counting down or in game;
  - 2: only outside a match;
  - 3: always.
- The `silence` command toggles `k_spectalk`.
  - Outside a match, anyone may use it, and everyone sees `Spectalk on: players can hear spectators during game` or `Spectalk off: players cannot hear spectators during game`.
  - During a match only admins may use it, and it takes effect at once: `Spectalk on: players can now hear spectators` or `Spectalk off: players can no longer hear spectators`.
- Demos: spectator-only chat goes only into the live stream, unless `k_keepspectalkindemos` is set. A player's private team chat (no byte 13, or not in game) is left out of demos.

## 7. Downloads: `allow_download*`, `sv_downloadchunksperframe`, `sv_maxdownloadrate`

**Where.** Server program.
**Default.**

| Setting | Program | nQuake |
| --- | --- | --- |
| `allow_download` | 1 | 1 |
| `_skins` | 1 | 1 |
| `_models` | 1 | 1 |
| `_sounds` | 1 | 1 |
| `_maps` | 1 | 1 |
| `_pakmaps` | 1 | 0 |
| `_demos` | 1 | 1 |
| `_other` | 0 | 1 |
| `sv_downloadchunksperframe` | 30 | 30 |
| `sv_maxdownloadrate` | 0 | 100000 |

**Classification.** Backslashes in the path are first turned into forward slashes. The first matching rule applies:

1. **Refused outright:** a path that is absolute, starts with `.` or `../`, contains `/../`, ends in `/..`, or ends in `.log` (any case). On Windows, a path that starts with a drive letter.
2. If `allow_download` is 0, everything is refused.
3. `csprogs.dat` is always allowed.
4. A path with no `/` (a file at the root of the game directory) is refused.
5. `skins/…` falls under `_skins`.
6. `progs/…` falls under `_models`.
7. `sound/…` falls under `_sounds`.
8. `maps/…` falls under `_maps`. In addition, if the file is found inside a pak and `_pakmaps` is 0, it is refused.
9. `demos/…` and `demonum/<n>` fall under `_demos`. `demonum/<n>` picks a recorded demo by number, where a negative number or a string of dots counts back from the newest. The server then tells the client to download that demo by name.
10. Everything else falls under `_other`.

An admin-authenticated client (logged in with `techlogin`) bypasses steps 2 to 10.

**Refusal.**
- The client receives the protocol's "file not found" download reply: size −1, or for chunked clients a −1 size with the file name. Its own UI then reports that the file was not found or not available.
- No text is printed by the server, except for an unknown demo number: `Demo number N not found.`
- Refusals and missing files look identical to the client.

**On success.** The client is told the size: `Map <name> is <K>KB (<M>MB)` (followed, for maps, by `Download this map faster:` and the URL in `download_map_url` if that is set), or `File <name> is <K>KB (<M>MB)`.

**Rate.**
- During a download, the client's `drate` userinfo is used, defaulting to 99999. It is capped by `sv_maxdownloadrate`, or by `sv_maxrate` when `sv_maxdownloadrate` is 0. The floor is 500.
- Duplicate packets are off while downloading.
- Chunked clients ask for 1024-byte chunks. The server answers at most `sv_downloadchunksperframe` of them per client per server frame (clamped to 1..30). The first chunk of a frame is sent only if the packet has room.
- One chunk per frame at 77 frames per second is about 77 KB/s.
- Older non-chunked clients get one block per request, sized by their rate.

## 8. `sv_maxrate` and `k_minrate` (the client's rate)

**Where.** `sv_maxrate` belongs to the server program. `k_minrate` belongs to the match mod, which also enforces `sv_maxrate` by talking to the client.
**Default.** `sv_maxrate`: 0 (no cap) in the program, 50000 in nQuake. `k_minrate`: 0 in the program, 2500 in nQuake.

**Server program.**
- The send rate (bytes per second) comes from the client's `rate` userinfo (or `drate` while downloading).
  - An empty value means 99999.
  - An explicit 0 means 2500.
- The value is capped at `sv_maxrate` if that is non-zero, then clamped to 500..300000.
- It is applied:
  - at connect;
  - whenever the userinfo changes;
  - to every client when `sv_maxrate` or `sv_maxdownloadrate` changes (checked once per frame);
  - by the client command `rate <n>`, which replies `Net rate set to N`. With no argument it replies `Current rate is N`.
- Otherwise the client is not told about the clamp.

**Match mod.**
- It keeps its settings sane:
  - `k_minrate` is bounded to 0..100000; 0 becomes 500;
  - `sv_maxrate` is bounded to 0..500000; 0 becomes 30000;
  - if the minimum is above the maximum, the minimum is lowered to the maximum.
- So under the mod, `sv_maxrate` is never 0.
- It checks a client's `rate` userinfo:
  - when the client connects as a player or spectator;
  - when the client changes `rate`, in which case an out-of-range value is rejected;
  - for every client every 20 seconds.
- A rate above the maximum: the client privately sees `\nYour rate setting is too high for this server.\nRate set to N`, and the mod stuffs `rate N` into the client, so the client sets it itself.
- A rate below `k_minrate`: the client sees `...is too low for this server.\nRate set to N`, with the same stuffing.
- An empty `rate` is accepted, since it means "the maximum allowed".

## 9. `sv_mintic`, `sv_maxtic`, `sv_maxvelocity`, `sv_speedcheck` (brief)

**Where.** Server program.

- **`sv_mintic` / `sv_maxtic`.** Program 0.013 / 0.1; nQuake 0.01 / 0.03. In seconds.
  - They bound the step of the *world* simulation: game-code thinking, projectiles, doors, items. Players are not covered; they are moved per input packet.
  - If less than `sv_mintic` has passed since the last world step, no step is run this frame.
  - A longer gap is clamped to `sv_maxtic`, so after a hitch the world runs slow rather than jumping.
  - So the highest world rate is 1/`sv_mintic`.
- **`sv_maxvelocity`.** Program 2000 (units per second); nQuake does not set it.
  - After a NaN check, any velocity whose **length** is above this is scaled down to it. The whole vector is scaled, not each axis.
  - It applies to free-moving entities (tossed, bouncing and flying projectiles, gibs, step-moving monsters).
  - It does not apply to player movement.
- **`sv_speedcheck`.** Program 1, nQuake 1. A guard against speed cheats, per human client (bots are exempt).
  - The server measures the real time since that client's last command batch and compares it with the milliseconds the command claims.
  - Unused real time goes into a bank, capped at 500 ms and floored at 0.
  - A command that claims more than the elapsed time plus the bank is cut down to that amount, and the bank is emptied.
  - Nobody is told.
  - Independent of this, any command longer than 50 ms is split into halves (the impulse only on the first half).

## 10. `sv_minping` and `sv_enable_cmd_minping` (brief)

**Where.** Server program.
**Default.** `sv_minping`: 0 (off) in the program and in nQuake. `sv_enable_cmd_minping`: 1 in the program, 0 in nQuake.

**Behaviour.**
- Each non-spectator client has an artificial delay applied to its incoming packets, from 0 to 1 s.
- On each packet, the delay moves by 1 ms: up if the measured ping is below `sv_minping` ms, down if it is above `sv_minping` + 1. So every player's ping converges on the floor.
- Spectators are exempt.
- The delay is frozen while paused.
- A client can set the floor itself with the command `minping <0..300>`, only if `sv_enable_cmd_minping` is 1 and no game is under way. "Under way" means a demo is recording (not just a live stream) or serverinfo `status` is not `Standby`.
  - Refusals: `Can't change sv_minping: demo recording or match in progress.`, `Can't change sv_minping: sv_enable_cmd_minping == 0.`, `Value must be >= 0 and <= 300.`
  - It then prints `sv_minping = N`.
  - Nobody else is told.

## 11. `sv_kicktop` and `sv_unfake` (brief)

**Where.** Server program. The match mod also unfakes in its own chat handler.
**Default.** Both 1 in the program and in nQuake.

- **`sv_kicktop`** counts changes to the `topcolor` userinfo. Only changes to a different value count.
  - A change more than 8 s after the window start opens a new window, with the count at 0.
  - Inside the window, a change is allowed while the count before it is at most 5, then the count goes up. So the 8th change inside one 8-second window kicks.
  - The window does not slide.
  - Everyone else sees `<name> was kicked for topcolor spam`. The player sees `You were kicked from the game for topcolor spamming` and is dropped at the end of the frame.
- **`sv_unfake`**: in public `say` only (not `say_team`), every carriage return (byte 13) is replaced with `#`. This stops a message that "fakes" another player's chat line by returning to the start of the line. No one is told.

---

## 12. `k_count` and `k_matchless_countdown` (the countdown)

**Where.** Match mod.
**Default.** `k_count`: 10 in the program and in nQuake. `k_matchless_countdown`: 0 in the program and in nQuake.
**Values.** `k_count` is in seconds.

**Behaviour.**
- The countdown begins when everyone is ready and the start conditions hold (see 15), or when an admin forces the start or the idle bot does (13).
- Broadcasts: `All players ready` (when every player is ready) and `Timer started`.
- Serverinfo `status` becomes `Countdown`.
- Countdown length:
  - the larger of `k_count` and 3;
  - in matchless mode, 0 unless `k_matchless_countdown` is 1;
  - 0 when deathmatch is off (coop).
- Every second:
  - Everyone gets a centre-print `Countdown: N`, followed by the match settings (mode, deathmatch, teamplay, time and frag limits, overtime, powerups, and so on).
  - In the last 5 seconds, every unready player privately sees `Get ready!` and every client hears a click.
- Each second the start conditions are re-checked. If they fail, everyone sees `Aborting...` and the match returns to prewar.
- During the countdown nobody can fire, and doors and lifts are frozen.
- At 0 the match starts. Everyone sees `The match has begun!` (in matchless mode only when `k_matchless_countdown` is 1), and serverinfo `status` shows the minutes left.
- In matchless mode the timer starts as soon as at least one player is present. With a countdown of 0 the match simply begins.

## 13. `k_idletime` (the idle bot: kicking unready players)

**Where.** Match mod.
**Default.** Program 0 (off); nQuake 0. The duel mode config sets 60 and the FFA mode config sets 180, but the idle bot only applies where players type `ready`.
**Values.** Seconds, minimum 3.

**When it activates.**
- It is checked when a player types `ready` (and not everyone is ready yet) and when a client disconnects.
- It activates only if all of these hold:
  - `k_idletime` > 0;
  - no bots are present;
  - at least 2 players are present;
  - at least half of them are ready;
  - no countdown, match or intermission is running;
  - no admin forced start is pending;
  - practice mode is off;
  - no idle bot is already running;
  - the start conditions hold.
- If only the start conditions fail, the player who just readied privately sees `Can't issue idle bot!`
- On activation everyone sees `\nserver activates the idle bot`.

**Each second.**
- The counter drops by one. The first tick comes almost immediately, so the first value shown is `k_idletime` − 1.
- If fewer than half are ready, or fewer than 2 players are present, everyone sees `console: bah! chickening out?\nserver disables the idle bot` and it stops.
- If the start conditions fail, everyone sees `idle bot removed` and it stops.
- Otherwise, when the counter is below 5 or a multiple of 5, each unready player privately sees `console: N second(s) to go ready`.

**At zero.**
- Everyone sees `server is tired of waiting\nmatch WILL commence!`
- Each unready player is announced to all as `<name> was kicked by IDLE BOT`, privately sees `Bye bye! Pay attention next time.`, and is disconnected.
- If more than one ready player remains, the countdown starts. Otherwise everyone sees `Can't start! More players needed.`
- If everyone readies first, the countdown removes the idle bot silently.

**Matchless counterpart.** `k_matchless_max_idle_time`: a player who has not fired for that many seconds is warned once, `WARNING: You will be forced to spectate if you do not fire within N seconds!`. The warning comes 30 s before the limit, or at half the limit if the limit is 60 s or less. At the limit the player sees `You were forced to reconnect as spectator by exceeding the maximum idle time of N seconds.` and is made to reconnect as a spectator.

## 14. `k_overtime` and `k_exttime` (tied at the time limit)

**Where.** Match mod.
**Default.** `k_overtime`: 0 in the program; 1 in nQuake (the FFA and most dmm4 mode configs set 2). `k_exttime`: 0 in the program, read as at least 1 and at most 999; 3 in nQuake.
**Values.** `k_overtime`: 0 off, 1 extra time, 2 sudden death, 3 tie-break, 4 golden frag. `k_exttime`: minutes.

**When the clock reaches 0**, the mod decides whether to go to overtime.
- The two scores compared:
  - Duel and FFA: the frags of the top two players. If fewer than two players are present, there is no overtime.
  - Team and CTF: the summed frags of the two teams. This includes players who have left (their kept scores). There must be exactly 2 teams and more than 1 player, otherwise there is no overtime.
- Whether the gap allows overtime:
  - Modes 1 and 2 need a gap of 0, a true tie.
  - Mode 3 allows a gap of at most 1.
  - Mode 4 ignores the gap and always goes to overtime.
- There is no overtime in a "lightning gun challenge" mode or in round-based modes.
- No overtime means the match ends normally.
- With overtime, everyone sees `time over, the game is a draw`, and the current armour, health and items are saved for the stats. Then, by mode:
  - **1 (extra time):** everyone sees `N minute(s) overtime follows`, `k_exttime` minutes go back on the clock and serverinfo `status` updates. When that runs out, the same decision is made again, so overtime can **repeat without limit** while the score is still tied.
  - **2 (sudden death):** the clock is removed and everyone sees `Sudden death overtime begins`. The match ends at the **next death of any player**, whoever it is and whatever the cause, including suicide or a fall.
  - **3 (tie-break):** everyone sees `tie-break overtime begins`. On each player death the gap is checked, and the match ends once it reaches 3 (or 2 in deathmatch 4).
  - **4 (golden frag):** everyone sees `Golden Frag overtime begins`. All through the match the absolute gap between the top two is noted at every death. In overtime, the match ends at the first death after which the gap is larger than the last noted gap. Otherwise the noted gap is updated.
- The fraglimit still ends the match in any phase.
- The `overtime` command cycles 0→1→2→3→4→0 outside a match. Its broadcasts are `Overtime: time based` with `Overtime length: N minute(s)`, `Overtime: sudden death`, `Overtime: tie-break`, `Overtime: golden frag`, and `Overtime: off`. Going from 0 to 1 also raises `k_exttime` to 1 if it is 0.
- The `overtimeup` command cycles `k_exttime` from 1 to 10 and wraps. It broadcasts `Overtime length set to N minute(s)`.

## 15. `k_membercount`, `k_lockmin`, `k_lockmax`, `k_exclusive` (start conditions and the lock)

**Where.** Match mod.
**Default.** All four are 0 in the program. nQuake: `k_membercount` 1, `k_lockmin` 0, `k_lockmax` 32, `k_exclusive` 0. The team mode presets set membercount 1–3, lockmin 1 and lockmax 2 or 3.

**Start conditions.** These are checked when a player readies, every countdown second, and every idle-bot second. When they fail, the message goes privately to whoever asked, or is broadcast during the countdown.
- In rocket-arena mode there are no limits.
- **Duel:** more than 2 players gives `Get rid of N player(s)!`
- **Other non-team modes:** no further checks.
- **Team and CTF:**
  - A player with an empty team gives `Get rid of players with empty team`.
  - "Ready teams" are the distinct non-empty teams that have at least one *ready* player.
  - Fewer ready teams than `k_lockmin` gives `N more team(s) required!` More than `k_lockmax` gives `Get rid of N team(s)!` (Clan arena and race fix both limits at 2.)
  - Every team, counting *all* its present players whether ready or not, must have at least `k_membercount` members. Otherwise: `Server wants at least N players in each team\nWaiting...`. When it is only a readiness check and not everyone is ready yet, this failure is silent.
  - CTF also needs the map's flags.
- Starting also needs every present player to be ready and at least 2 ready players.

**The lock while in game** (not in matchless mode). It is applied when someone joins as a player mid-match:
- `k_lockmode` 2: always refused, with `Match in progress, server locked\nPlease reconnect as spectator`.
- `k_lockmode` 1: duel and FFA admit only a name that matches a player present or kept from the match. Team modes admit only a team that already exists, and only if that team is below the largest team's size.
- Joining with an empty team, or in CTF without red or blue, is always refused.
- **`k_exclusive` 1:** the player count is taken when the countdown finishes. A newcomer is refused with `Sorry, server is full\nPlease reconnect as spectator` while the present player count is at that number or above. Someone leaving lets one person in.
- A refused joiner sees the message and is disconnected.
- An admitted joiner is announced as `<name> entered the game`. A player who left gets back their kept score.

## 16. `k_defmap` and `k_autoreset` (when the server empties)

**Where.** Match mod.
**Default.** `k_defmap`: empty in the program; not set in nQuake's main config (the matchless CTF config sets `e2m2`). `k_autoreset`: nQuake sets 1, but **the current mod never reads it**. The reset below always happens.

**When the last human player leaves** (bots do not count):
1. Any running match or countdown is ended without the usual results, and race mode is turned off.
2. The **reset** runs:
   - serverinfo `matchtag` is cleared and the team overlay is turned off;
   - `configs/reset.cfg` is executed if it exists;
   - the default game mode's preset is executed. That is the one named by `k_defmode`, or `ffa` on a matchless server.
3. The map is chosen:
   - If leaving switched the server between match and matchless, or practice mode is unlocked, or bots were toggled, the current map reloads at once.
   - Otherwise a check is scheduled for **10 s** later. If at that point there are still no human players and `k_lockmap` is 0:
     - if `k_defmap` is set and is not the current map, the server changes to it;
     - otherwise, if an intermission is running or only bots are present, the current map reloads;
     - otherwise nothing happens.
- The same check also runs 60–90 s (randomised) after every map load, or 0.5 s after the first map since the server started. So an empty server drifts to `k_defmap`.
- On a matchless server (other than CTF) none of the `k_defmap` checks run.
- Separately, an idle map is reloaded after 12 hours of uptime when no match is running.

## 17. `k_prewar`, `k_freeze`, `k_sready` (warm-up)

**Where.** Match mod. **Default.** `k_prewar`: 1 in the program and in nQuake. `k_freeze`: 0 in both. `k_sready`: 0 in the program, 1 in nQuake.

- **`k_prewar`** applies only before the match:
  - **0:** no firing. Jumping is allowed. A player who tries to fire sees `can't fire, prewar is disabled`.
  - **1:** free firing and jumping.
  - **2:** players who are not ready can neither fire (`type ready to enable fire`) nor jump (`type ready to enable jumps`). A blocked jump is replaced by a downward push of 270 units/s.
  - Each refusal message is shown at most once per second per player, with a brief screen flash.
  - In captains mode nobody may fire (`can't fire until in captains mode`).
  - During the countdown nobody may fire, whatever the value.
  - The admin command `prewar` cycles 0→1→2→0 and broadcasts `Players may fire before match`, `Players may fire and jump when ready`, or `Players may not fire before match`. Switching to 0 or 2 also stops everyone's current firing.
- **`k_freeze` 1:** before the match, doors, buttons and lifts ignore touches and trains do not move. Practice mode is exempt. (During the countdown these are always frozen.) Toggled with the `freeze` command (announced as "map freeze").
- **`k_sready` 1:** before the match, on a non-matchless server, every player who is not ready glows blue.

## 18. `k_timetop` (the cap on the time limit)

**Where.** Match mod. **Default.** Program 0, which is read as 30 (and written back as 30). nQuake 30. **Values.** Minutes, bounded to 0..600.

- The player commands `timeup`, `timedown` and `timeset` work only outside a match. They clamp the new time limit to 0..`k_timetop`.
  - The 5-minute steps snap through 0→1→3→5 going up and 5→3→1 going down.
  - A change is broadcast as `Match length set to N minute(s)`.
  - If the value does not change, the sender privately sees `timelimit still N`.
  - Lowering the time limit to 0 while the fraglimit is also 0 is refused with `You need some timelimit or fraglimit at least`.
- The mod also checks continuously. If in deathmatch the time limit is above `k_timetop`, below 0, or 0 together with a fraglimit of 0, it is set to `k_timetop`. Hoonymode duel, race and clan arena are excepted. An admin config cannot exceed the cap either.

## 19. `dq`, `dr`, `dp`, `k_dis`, `k_pow`, `k_fallbunny`, `k_spw` (brief)

All of these are match-mod settings. Program defaults are 0. The mode presets set `dp 1`, `dq 0`, `dr 0`, `k_dis 1`, `k_spw 4`.

- **`dq`** (nQuake 0): when powerups and quad are on, a player who dies with quad drops it as a pickup carrying the remaining quad time. This never happens during berzerk (see 20).
- **`dr`** (nQuake 0): the same for the ring of invisibility.
- **`dp`** (nQuake 1): dying drops a backpack, but only *in game*. There is no pack on suicide and no pack if it would be empty. Its contents follow `k_frp` (see 20).
- **`k_dis`** (nQuake 1): discharging the lightning gun under water.
  - 0: the cells are lost and nothing explodes.
  - 1: radius damage of 35 per cell.
  - 2: the same, but anyone not in water takes no damage from it.
- **`k_pow`** (nQuake 0; the 2on2 and 4on4 mode configs set 1): powerups on or off. The individual powerups are controlled by `k_pow_q`, `k_pow_p`, `k_pow_r` and `k_pow_s`. On a matchless server, `k_pow_min_players` keeps powerups off until that many players are present, re-checked every `k_pow_check_time` seconds (default 10).
- **`k_fallbunny`** (nQuake 1, program 0): with 0, a hard landing (one that hurts, not in water) "breaks the ankle", so the player cannot jump on the frame of landing. That rules out bunny-hopping out of a long fall. With 1 the standard behaviour applies. Yawn mode and race always behave as 1.
- **`k_spw`** (nQuake 3): which spawn point a player gets. A spot counts as blocked when a live player other than the spawner is within 84 units.
  - −1: the spots in order.
  - 0: random among unblocked spots.
  - 1: also never the same spot twice in a row. If every spot is blocked, players near the chosen spot are pushed away.
  - 2: in game, a nearby player blocks a spot only within 2.6 s of their own spawn or teleport. Otherwise as 1.
  - 3: as 2.
  - 4: as 3, plus per-player spot weights, and a second draw if the same spot comes up twice in a row.
  - If every spot is blocked, a random spot is used anyway.

## 20. `k_short_gib`, `k_666`, `k_bzk`/`k_btime`, `k_frp`, `k_highspeed`

- **`k_short_gib`** (default 0): gibs disappear after 2 s instead of 10–20 s.
- **`k_666`**: **not read by the current mod.** Spawn invulnerability is `dmm4_invinc_time` instead, which applies in deathmatch 4 (and bloodfest), in game.
  - Its default is 2 s and its maximum 30. A negative value turns it off, and midair mode turns it off.
  - The command `spawn666time <s>` sets it, only in deathmatch 4 and outside a match, and broadcasts `<name> set spawn invincibility time to N.Ns`.
- **`k_bzk`/`k_btime`** (nQuake 0 / 20): berzerk.
  - When the match clock reaches `k_btime` seconds remaining, everyone sees `BERZERK!!!!`, the lights flicker, and every living player gets quad for the rest of the match plus 2 s of invulnerability.
  - Players who join after that get quad too.
  - Toggled with `berzerk`, outside a match only.
- **`k_frp`** (nQuake 0): what weapon a dropped backpack holds.
  - 0: the weapon held.
  - 1: the best weapon that still has ammo, in this order from best: rocket launcher, lightning gun, grenade launcher, super nailgun, super shotgun, nailgun.
  - 2: the last weapon fired, even with no ammo.
  - Yawn mode forces 2.
- **`k_highspeed`** (nQuake 320): the `speed` command, outside a match only, switches `sv_maxspeed` between 320 and `k_highspeed` (bounded to 0..9999). It broadcasts `Maxspeed set to N`.

---

## Rough cost to build (opinion)

Ranked by value to a deathmatch server divided by the effort to build it, best first:

1. **`timeout` / `zombietime`.** Essential, tiny, and a clear rule.
2. **`maxspectators` + `sv_forcespec_onfull`.** Slot counting plus one fallback rule. A full server is common.
3. **Chat flood (`floodprot`, or the mod's `k_fp` levels).** A ring of 10 times per client and two messages. High value on public servers.
4. **`k_count` + `k_matchless_countdown`.** A per-second ticker. Every match needs it.
5. **`sv_maxrate` + `k_minrate`.** A clamp plus a 20-second re-check. It prevents both starved and flooding clients.
6. **Spectator chat routing (`sv_spectalk`, `k_spectalk`, `sv_sayteam_to_spec`, `k_sayteam_to_spec`).** A routing table plus phase switches. Important for fair matches.
7. **`k_timetop`.** A one-line clamp on a vote.
8. **`k_overtime` / `k_exttime`.** Moderate: two scores to compare, four modes, and repeat logic. Highly visible in duels.
9. **`k_prewar` / `k_freeze` / `k_sready`.** Gates on fire and jump plus a glow. Cheap once the match phases exist.
10. **`sv_reconnectlimit`.** Small, but its value is modest because of the zombie window.
11. **`pausable`, with the mod's 3-request budget and 3 s lead-in.** Medium. It needs a frozen-clock path and a real-time countdown while paused.
12. **`k_membercount` / `k_lockmin` / `k_lockmax` / `k_exclusive`.** Needed for team play. The join-lock rules are fiddly.
13. **`k_defmap` / reset on empty.** Needs a config-exec path and a delayed map change. Keeps public servers tidy.
14. **`k_idletime`.** A state machine with five messages. Useful for duels and pickups.
15. **`dq`/`dr`/`dp`/`k_frp`/`k_dis`/`k_pow`.** Each is small game-rule code, but there are many of them.
16. **`sv_kicktop` + `sv_unfake`.** Trivial, but they matter only for classic clients.
17. **`sv_mintic`/`sv_maxtic`/`sv_maxvelocity`.** Mostly decided by our own fixed-step simulation already. Copy only the clamps.
18. **`k_spw`.** Spawn selection matters a lot, but the faithful weighted variant (4) is real work. Start with 0 and 1.
19. **`sv_speedcheck`.** Only matters if clients send their own frame times. Our deterministic input log may make it moot.
20. **Downloads (`allow_download*`, chunking, rates).** Path classification is easy. The transfer protocol (chunked and legacy, rate pacing) is the biggest piece here, with low value if content ships as packs.

Also low priority: **`sv_minping`/`minping`** (a delay queue per client), **`k_short_gib`, `k_bzk`, `k_highspeed`** (cosmetic or novelty), and **`k_666`** (dead; build `dmm4_invinc_time` instead if deathmatch 4 is wanted).
