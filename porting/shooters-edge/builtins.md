# The modern server's builtins and hooks, specified

What a multiplayer `qwprogs.dat` may call and what calls it, at the modern server's behaviour
(the baseline M18o locked), in our words (README.md, the rules). Written by the research session
that wrote `gameloop.md` and `progs.md`, from the server's source; the building session takes it
from here and does not look further. The VM itself (memory, opcodes, calls, the entity text) is
`progs.md`; the frame and physics that call the hooks are `gameloop.md`; the messages a builtin
writes are `protocol.md`'s.

Conventions: arguments are the parameter slots (`p0` = global 4, `p1` = 7, …); the result goes to
the return slot. "The argument count" is the CALL's count. A **temporary string** result is
written into a ring of 64 buffers of 2048 bytes (truncated to fit) and stays valid until 64 more
are made; strings a hook is handed by the engine use the separate ring of 8 (`progs.md`).
"Error" is a program error with the stack trace (`progs.md` § Calling). An entity argument is
decoded to its number; **Ours** checks the number is in range and errors when it is not (the
server does not check most of them).

## The standard numbers (1–82)

| # | Name | Arguments → result | Does |
| --- | --- | --- | --- |
| 0 | — | | always an error ("unimplemented builtin") |
| 1 | makevectors | v angles | sets `v_forward`, `v_right`, `v_up` from pitch, yaw, roll as the move's angle vectors (`movement.md` § units: the same function) |
| 2 | setorigin | e, v | copies the origin, resets the entity's lag-compensation history, relinks it in the area tree (no touching) |
| 3 | setmodel | e, string m | the name must be in the model precache list (else error "no precache: m"); sets `model` to the argument's string value and `modelindex` to its list index. A name starting with `*` is a map's inline brush model: `mins`, `maxs` and `size` come from that model's bounds and the entity is relinked. Other models leave the size alone |
| 4 | setsize | e, v mins, v maxs | sets `mins`, `maxs`, `size` = maxs − mins; relinks |
| 5 | — | | error (an old unused slot) |
| 6 | break | | prints "break statement"; the server deliberately crashes to a debugger. **Ours**: an error |
| 7 | random | → float | k / 32767 where k is a uniform integer 0..32767, so 0 and 1 are both possible. **Ours**: k from the game's seeded generator (`core::Rng`), never a platform random |
| 8 | sound | e, float channel, string sample, float volume, float attenuation | volume × 255 truncated must be 0..255, attenuation 0..4, channel 0..15, else a server error. The sample must be precached (else a console message and nothing). Channel bit 8 means "not limited to who can hear it, and reliable"; channel & 7 is the channel. The sound's position is the entity's origin, or the centre of its box for a brush model or a model-less trigger. Sent to clients who can hear it from there (by the map's hearing sets, or anyone within 1024 units) unless bit 8 or the server's PHS switch is off, in which case to everyone |
| 9 | normalize | v → v | v / |v| (length in float, square root in double rounded to float, then 1/length multiplied), zero for a zero vector |
| 10 | error | strings… | joins all arguments, prints them with the function's name and `self`, and stops the map |
| 11 | objerror | strings… | the same, frees `self` first, and stops the map too (the old comment says the level continues; it does not) |
| 12 | vlen | v → float | the length as in normalize |
| 13 | vectoyaw | v → float | 0 if x and y are both 0; else atan2(y, x) in degrees, in double, plus 360 when negative, rounded to float |
| 14 | spawn | → e | allocates an entity (`progs.md` § Allocating) |
| 15 | remove | e | frees it (`progs.md` § Allocating) |
| 16 | traceline | v start, v end, float nomonsters, e ignore | a line trace through the world (`gameloop.md` § world traces) with move type nomonsters (0 normal, 1 no monsters, 2 missile; truncated); with the server's lag compensation at 2 the trace is lag-compensated. Writes the `trace_*` globals: allsolid, startsolid, fraction, inwater, inopen, endpos, plane normal and dist, and `trace_ent` (the world when nothing was hit) |
| 17 | checkclient | → e | a client that `self` might see, for monster AI: every 0.1 s of server time the candidate moves on to the next client slot (wrapping 1..32) that is in use, alive (health > 0) and not flagged no-target, and the visibility set from its eye (origin + view_ofs) is taken. Returns the world if the candidate is free or dead, or if `self`'s eye is not in that set; else the candidate |
| 18 | find | e start, field f, string s → e | the next entity after `start` (in number order, up to the highest in use) that is not free and whose string field `f` equals `s` by content; the world if none. A null `s` is an error |
| 19 | precache_sound | string s → s | only while the map loads (spawn functions), else error. The name must not be empty or start with a character ≤ space (error "Bad string"). Adds it to the sound list if new (overflow is an error); returns its argument |
| 20 | precache_model | string s → s | the same for the model list |
| 21 | stuffcmd | e client, string s | client numbers 1..32 only (else error "Parm 0 not a client"). If `s` is exactly "disconnect\n" the client is dropped. Otherwise appends to that client's pending command text (256 bytes; overflow is an error); when the pending text contains a newline, it is sent reliably as one command message and cleared |
| 22 | findradius | v org, float rad → e | every linked entity (solid and trigger lists of the area tree) inside the box org ± (rad + 1), skipping non-solid ones, whose box centre (origin + (mins + maxs)/2) is within `rad` of org (squared distance compared in float), chained through the `chain` field; returns the head of the chain (the world if none). The chain's order is the reverse of the area tree's visiting order |
| 23 | bprint | float level, strings… | joins the strings from p1 on and prints to every client at that level (0 low, 1 medium, 2 high, 3 chat) |
| 24 | sprint | e client, float level, strings… | joins from p2 on and prints to one client; a non-client prints a console message. Spectators tracking that client get a copy when the server's spectator-print switch has bit 2 and their own setting asks for it |
| 25 | dprint | strings… | prints to the server console |
| 26 | ftos | float → temp string | a whole number (equal to its truncation) prints as an integer; otherwise as `%5.1f` (one decimal, right-aligned in 5 characters, e.g. "  1.5") |
| 27 | vtos | v → temp string | `'x y z'` with each component as `%5.1f`, in single quotes |
| 28 | coredump | | prints every entity |
| 29 | traceon | | turns on statement tracing (print each statement) |
| 30 | traceoff | | turns it off |
| 31 | eprint | e | prints one entity's fields |
| 32 | walkmove | float yaw, float dist → float | for `self`: 0 unless it is on the ground, flying or swimming; else the step move below with the move (cos yaw × dist, sin yaw × dist, 0) (yaw in degrees, trig in double), relinking. `self` and the running function are restored after (the move may run touch functions) |
| 33 | — | | error |
| 34 | droptofloor | → float | traces `self`'s box from its origin 256 units down; if it hit nothing or started all solid, 0; else moves it to the end, relinks, sets the on-ground flag and `groundentity` to what was hit, and returns 1 |
| 35 | lightstyle | float style, string value | sets the light style's pattern; while the map runs, sends it reliably to every spawned client |
| 36 | rint | float → float | round half away from zero, through a 32-bit integer |
| 37 | floor | float → float | |
| 38 | ceil | float → float | |
| 39 | — | | error |
| 40 | checkbottom | e → float | 1 if the entity's box stands on ground: if the four points 1 unit below its bottom corners are all in solid, yes; otherwise trace a line from the bottom centre down 36 units (must hit), and from each bottom corner down to the same depth: every corner must hit and lie within 18 units below the centre's hit |
| 41 | pointcontents | v → float | the contents at the point (`gameloop.md` § world traces) |
| 42 | — | | error |
| 43 | fabs | float → float | |
| 44 | aim | e, float speed → v | returns `v_forward` (no auto-aim on this server) |
| 45 | cvar | string → float | the setting's value as a number; the name `pr_checkextension` (any case) returns 1 |
| 46 | localcmd | string | appends to the server's own command buffer |
| 47 | nextent | e → e | the next entity after it that is not free, up to the highest in use; the world after the last |
| 48 | particle | v org, v dir, float colour, float count [, float effect, float effect count] | a particle burst for clients; sent as the effect given in p4 (and p5, default 1). Without p4 the effect is guessed: count 255 → an explosion, colour 73 → blood (count 1), colour 225 → lightning blood, otherwise nothing is sent |
| 49 | changeyaw | | turns `self`'s yaw toward `ideal_yaw` by at most `yaw_speed`: current = the yaw wrapped into 0..360 (the angle wrap of `movement.md`); no change if equal; the difference is taken the short way round (±180), clamped to ±yaw_speed, and the result wrapped again |
| 50 | — | | error |
| 51 | vectoangles | v → v | pitch, yaw, 0 in degrees: x and y zero gives yaw 0 and pitch 90 (z > 0) or 270; else yaw as vectoyaw and pitch = atan2(z, the horizontal length) in degrees, plus 360 when negative |
| 52–59 | WriteByte, WriteChar, WriteShort, WriteLong, WriteCoord, WriteAngle, WriteString, WriteEntity | float dest, value | append one value to a message: dest 0 the unreliable broadcast, 1 reliably to the client in `msg_entity` (1..32, else error), 2 the reliable broadcast, 3 the signon (only while loading, else error), 4 the multicast buffer (sent by #82). Other values: error. Numbers are truncated from the float; an entity is written as its number (a short); the encodings are `protocol.md`'s |
| 60–62 | sin, cos, sqrt | float → float | (extensions, below) |
| 63–66 | — | | error |
| 67 | movetogoal | float dist | monster chase step (below) |
| 68 | precache_file | string → string | returns its argument; does nothing |
| 69 | makestatic | e | copies the entity into the static list (model index from its model's name, frame, colormap, skin, origin, angles, and its alpha and colour extension values) and frees it. A full list (2048) or a model not in the list just frees it |
| 70 | changelevel | string map | queues "map <name>" once per map (a second call in the same map does nothing) |
| 71 | — | | error |
| 72 | cvar_set | string name, string value | sets a setting; an unknown name prints a message |
| 73 | centerprint | e client, strings… | joins from p1 on and sends it reliably as a centred message to one client (copies to tracking spectators as sprint does, under bit 1) |
| 74 | ambientsound | v pos, string sample, float volume, float attenuation | adds a looping sound to the signon: the sample must be precached (else a message), volume × 255 and attenuation × 64 truncated |
| 75–77 | precache_model2, precache_sound2, precache_file2 | | the same as 20, 19, 68 |
| 78 | setspawnparms | e client | copies that client's 16 saved spawn values into `parm1..parm16`; a non-client is an error |
| 79 | logfrag | e killer, e killed | both must be clients (else nothing); appends a frag line to the server's frag log: `\<killer name>\<killed name>\` and a newline, or with the server's long frag log on `\frag\<killer>\<killed>\<killer team>\<killed team>\<date and time>\`. **Ours**: the date and time come from the game's clock source, so a replay writes the same line |
| 80 | infokey | e, string key → temp string | for the world: the server info's value, or the local info's when that is empty. For a client: `ip` and `realip` its addresses, `download` its download percentage (−1 when none), `ping` its ping, `*userid` its user id, `login` its login name, otherwise its user info value. Anything else: empty |
| 81 | stof | string → float | the number in the string, read like this: skip leading characters ≤ space; an optional sign; `0x` starts hexadecimal digits; a `'` takes the next character's code; otherwise decimal digits with one optional point, accumulated in double, the fractional digits divided out by repeated ÷10, rounded to float. Reading stops at the first other character |
| 82 | multicast | v where, float to | sends the multicast buffer to: 0 everyone, 1 those who can hear `where` (its hearing set, or anyone within 1024 of their eye), 2 those who can see it, 3–5 the same three reliably; clears the buffer. A spectator tracking a player (`trackent`) is judged from that player's eye. Other values are an error |

## The extension builtins

| # | Name | Arguments → result | Does | Extension |
| --- | --- | --- | --- | --- |
| 60 | sin | float → float | in double, rounded to float (**Ours**: `core/libm`) | DP_QC_SINCOSSQRTPOW |
| 61 | cos | float → float | the same | DP_QC_SINCOSSQRTPOW |
| 62 | sqrt | float → float | the same | DP_QC_SINCOSSQRTPOW |
| 84 | tokenize | string → float | splits the string into words (below) into the program's own word list (separate from the console's); returns the count | ZQ_QC_TOKENIZE |
| 85 | argc | → float | the count from the last tokenize | ZQ_QC_TOKENIZE |
| 86 | argv | float n → temp string | word n (truncated to an integer), empty when out of range | ZQ_QC_TOKENIZE |
| 90 | tracebox | v start, v mins, v maxs, v end, float nomonsters, e ignore | as traceline with a box (against the world a box uses the clip hull the map has for that size, `movement.md`; entities use the exact box). No lag compensation | DP_QC_TRACEBOX |
| 91 | randomvec | → v | three values k × (2/32767) − 1 (k as in random), repeated until the squared length is below 1 | DP_QC_RANDOMVEC |
| 93 | registercvar | string name, string value → float | 0 if the setting exists; else creates it and returns 1. The server creates it with the **name** as its value (it reads the name twice). **Ours**: the same, so a mod sees what it sees there | DP_REGISTERCVAR |
| 94 | min | floats… → float | the smallest of 2 or more arguments (fewer is a server crash; **Ours** an error) | DP_QC_MINMAXBOUND |
| 95 | max | floats… → float | the largest | DP_QC_MINMAXBOUND |
| 96 | bound | float lo, float v, float hi → float | max(lo, min(v, hi)) | DP_QC_MINMAXBOUND |
| 97 | pow | float x, float y → float | in double, rounded | DP_QC_SINCOSSQRTPOW |
| 99 | checkextension | string → float | 1 if the name (any case) is in the list below, else 0 | |
| 103, 448 | cvar_string | string → temp string | the setting's text; string 0 if it does not exist | QSG_CVARSTRING, DP_QC_CVAR_STRING |
| 114 | strlen | string → float | its length in bytes | ZQ_QC_STRINGS |
| 115 | strcat | strings… → temp string | all arguments joined (2047 bytes at most) | ZQ_QC_STRINGS |
| 116 | substr | string s, float start, float len → temp string | start and len truncated; empty when start ≥ the length, len is 0 or s is empty; else from `start` at most `len` characters (a negative start or len is undefined in the server: **Ours** clamps start to 0 and treats a negative len as "to the end") | ZQ_QC_STRINGS |
| 118 | strzone | string s [, float size] → string | a copy kept until strunzone or the map's end, in one of 1024 slots (none free: error "out of string memory"); the optional size (a larger buffer) is ignored by us beyond the copy | ZQ_QC_STRINGS |
| 119 | strunzone | string | frees a strzone string; a string that is not one is an error; freeing twice is allowed | ZQ_QC_STRINGS |
| 231 | calltimeofday | | if the program has a function `timeofday`, calls it with the local time: seconds, minutes, hours, day of month, month, year as floats in p0–p5 and the date as text ("Mon Jan 02, 15:04:05 2006" layout) in p6 (an engine string). **Ours**: from the game's clock source, recorded in a replay | FTE_CALLTIMEOFDAY |
| 531 | setpause | float | pauses (non-zero) or unpauses the server if it is not already in that state | ZQ_PAUSE |
| 532 | precache_vwep_model | string → float | only while loading; the name may not be empty, start with a character ≤ space, or contain `"`, `;`, a newline, a tab or a space (error); appends to the 32-entry visible-weapon list (overflow is an error) and returns its index | ZQ_VWEP |

**The extension list** `checkextension` answers yes to: DP_CON_SET, DP_QC_CVAR_STRING,
DP_QC_MINMAXBOUND, DP_QC_RANDOMVEC, DP_QC_SINCOSSQRTPOW, DP_QC_TRACEBOX, DP_REGISTERCVAR,
FTE_CALLTIMEOFDAY, QSG_CVARSTRING, ZQ_CLIENTCOMMAND, ZQ_ITEMS2, ZQ_MOVETYPE_NOCLIP,
ZQ_MOVETYPE_FLY, ZQ_MOVETYPE_NONE, ZQ_PAUSE, ZQ_QC_STRINGS, ZQ_QC_TOKENIZE, ZQ_VWEP. (The three
move types are `gameloop.md`'s; DP_CON_SET is a console command, `configs.md`.)

**Words** (tokenize): skip spaces, tabs and carriage returns; a newline or the end stops; a
`//` runs to the end of the line and is skipped; a `"` starts a quoted word that runs to the next
`"` or the end (the quotes are dropped); otherwise a word runs to the next space, tab, newline or
carriage return. At most 80 words and 1024 bytes of words in all (stop when either is reached);
a word is at most 1023 bytes. The text after the first word (from where the second word starts)
is kept as the "arguments" line for hooks that pass it.

## The monster moves (walkmove, movetogoal, checkbottom)

The **step move** of entity E by vector m:

- **Flying or swimming** (flag fly or swim): try twice. The first try, if E has an enemy, adds a
  height correction to the target: E higher than the enemy by more than 40 → 8 down; by less than
  30 → 8 up. Trace E's box from its origin to origin + m (+ correction). A full move (fraction 1)
  succeeds: a swimmer whose end is in empty contents fails instead (left the water); else E moves
  to the end (relinked when asked). Otherwise, with no enemy stop trying; the second try has no
  correction. Two failures: fail.
- **Walking**: from origin + m raised 18, trace E's box down 36. All solid: fail. Started in
  solid: retry from 18 lower (the plain target); solid again: fail. Hit nothing: if E has the
  partial-ground flag, move it by m, relink, clear on-ground and succeed; else fail (walked off
  an edge). Otherwise move E to the end; if `checkbottom` fails, succeed anyway when E has the
  partial-ground flag (relink), else move it back and fail. On success clear partial-ground, set
  `groundentity` to what was hit, relink.

**movetogoal** (dist): for `self`, with goal = its `goalentity`. Not on ground, flying or
swimming: return 0. If it has an enemy and its box grown by `dist` touches the goal's box: stop.
Else, one time in four (a draw of 0..3 from the game's generator equal to 1) or if stepping
toward `ideal_yaw` fails, pick a new chase direction:

1. Old direction = `ideal_yaw` truncated to a multiple of 45, wrapped; turnaround = old − 180,
   wrapped.
2. dx, dy = goal − self. Horizontal choice: 0 if dx > 10, 180 if dx < −10, else none. Vertical:
   270 if dy < −10, 90 if dy > 10, else none.
3. Both chosen: the diagonal (45, 315, 135 or 215 — note 215, not 225, as the server has it);
   if it is not the turnaround, step that way and stop on success.
4. If a coin draw is heads or |dy| > |dx|, swap the two choices. Try the first, then the second
   (each if chosen and not the turnaround).
5. Try the old direction. Then all eight directions from 0 up to 315 or from 315 down to 0 (a
   coin draw decides), skipping the turnaround. Then the turnaround.
6. Nothing worked: `ideal_yaw` = old direction; if `checkbottom` fails, set partial-ground.

**Stepping a direction** (yaw, dist): set `ideal_yaw` = yaw, turn as changeyaw does (on `self`),
then a step move by (cos, sin) × dist without relinking. On success, if the yaw now differs from
the ideal by more than 45 and less than 315 (current minus ideal), put the origin back (turned
but not moved). Relink either way; return whether the move succeeded.

The server's draws are C `rand()` (the coin is the low bit; "one in four" is the low two bits
equal to 1). **Ours**: draws from the game's generator in that order; a replay holds.

## The hooks

When the engine calls a hook it sets `time` to the server's time and `self` as listed. A hook
that is optional is called only if the program has it (`progs.md` § Optional hooks). Strings
handed in use the ring of 8.

| Hook | When | self | Arguments → result |
| --- | --- | --- | --- |
| main | not called by this server | | |
| StartFrame | each physics frame, first (`gameloop.md`) | world | |
| PlayerPreThink, PlayerPostThink | around each player's command (`gameloop.md`) | the player | |
| SpectatorThink | in place of PlayerPostThink for a spectator | the spectator | |
| ClientConnect, SpectatorConnect | when a client (or spectator) enters the game | it | |
| PutClientInServer | after ClientConnect for a player | it | |
| ClientDisconnect, SpectatorDisconnect | when it leaves | it | |
| SetNewParms, SetChangeParms | to fill `parm1..16` for a new client, and to save them across a map change | the client | |
| ClientKill | the `kill` command | the player | |
| UserCmd | a client command the mod may take (below) | the client | p0 the command word → float: non-zero = handled |
| GE_ClientCommand | the same, preferred when present | the client | p0 the word, p1 the arguments line → float handled |
| ChatMessage | `say` / `say_team`, before the server prints it | the client | p0 the text (surrounding quotes removed), p1 1 for team → float: non-zero = the mod handled it |
| UserInfo_Changed | after a client's info key changes | the client | p0 key, p1 old value, p2 new value |
| localinfoChanged | after the server's local info changes | world (0) | p0 key, p1 old value, p2 new value |
| ConsoleCmd | a server console command no command, setting or alias matched | world (0) unless the console is a mod's redirect | → float handled; the words are the console's |
| GE_ShouldPause | a client asks to pause or unpause (spectators cannot), or the server unpauses because no players are left | the client, or world | p0 the new state → float: 0 = refuse |
| GE_PausedTic | every server frame while paused | | p0 seconds paused so far |
| timeofday | by `calltimeofday` | | as above |

**Client commands**: a command a client sends is matched against the server's own commands.
One the server marks as not overridable runs at once. Otherwise the mod gets it first (through
GE_ClientCommand if present, else UserCmd); if the mod says handled, done; else the server's own
command runs, or "Bad user command" is printed. The overridable ones: rate, kill, pause, say,
say_team, minping, airstep, maps, ban, banip, banrem, join, observe, qtvusers, god, give, noclip,
fly. `say`/`say_team` then also offer ChatMessage.

## The extension fields

Looked up by name after loading; the engine reads or writes them as follows.

| Field | Type | The engine |
| --- | --- | --- |
| items2 | float | its integer bits, shifted left 23, are added to the items the player's status sends |
| maxspeed | float | the player's move's maximum speed; a change is sent to the client |
| gravity | float | the player's move's gravity scale (1 when the field is absent); a change is sent to the client |
| movement | vector | written before each command with the command's forward, side and up moves |
| vw_index | float | the visible weapon index sent with the player for clients that take visible weapons |
| hideentity | entity | for this client, that entity is not sent |
| trackent | entity (a client number 1..32 read raw) | the client sees from that player's view (for spectators): visibility, sound and the view follow the tracked player |
| visclients | float as bits | the engine sets bit n when client n+1 can see this entity this frame, and clears it otherwise; cleared for everyone each frame |
| hideplayers | float | when set, other players are not sent to this client |
| teleported | float | read with a fixed angle: non-zero tells capable clients the jump was a teleport (else a respawn), then cleared |

## Map entity keys handled by the engine

`alpha` (clamped 0..1) and `colormod` (three numbers, each > 0, else ignored) are kept per
entity outside its fields and are sent to clients that support them; `makestatic` carries them.
