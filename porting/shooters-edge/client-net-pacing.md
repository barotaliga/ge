# The clients' physics and packet pacing, and the server's view kicks (clean-room note)

WORKING AREA: deleted when porting ends. M18k4 round 12, client settings: how the classic clients
pace their physics frames, their moves and their packets (ezQuake 3.6.9 for QuakeWorld; Q2PRO's
master and its actionquake fork for Quake II), the settings a player sees for it with their
defaults, ranges and locks, and when the QuakeWorld server (mvdsv's master) sends the view kicks.
Written by a clean-room reader in its own words, with no code; each fact is marked read (the
source says it) or derived (worked out from read facts, how stated). Nothing was measured by
running a binary.

## QuakeWorld client (ezQuake 3.6.9)

**Two clocks or one: `cl_independentPhysics`** (default 1). Changeable only while disconnected
("... can be changed only when disconnected"); `-noindphys` on the command line sets it 0 and
`cl_nolerp` 1. At 0 every drawn frame is a physics frame (input, read packets, one move, one
packet). At 1 a physics clock picks physics frames among drawn frames; only they gather input,
run commands, make and send a move; the frames between only draw and keep reading the mouse, and
the client's own predicted position and angles are smoothed between physics frames for drawing
unless `cl_nolerp` 1 (default 0) or `cl_nolerp_on_entity` 1 (default 0, while on a mover). read

**The physics rate (independent physics on).** Cap = the server-info key `maxfps` (72 when absent
or 0; mvdsv's default is 77). A non-zero `cl_physfps` (default 0) lowers it and can never raise it
above the server's value; the result is raised to at least 10; no other upper bound. While
spectating (not in a demo) `cl_physfps_spectator` (default 77) replaces `cl_physfps` in the same
rule. Demo playback: every drawn frame is a physics frame. The game's help text says `cl_physfps`
0 means "use cl_maxfps"; the code uses the server's cap. read

**The physics clock.** Each drawn frame adds its time (capped at 0.2 s) to an accumulator. Below
one period (one over the cap) no physics frame; at or above it a physics frame of exactly one
period, the rest carried; above two periods the whole accumulated time is one long physics frame
and the accumulator empties. At most one physics frame per drawn frame, so with drawn frames
slower than the cap physics runs at the drawn rate with an occasional long frame. read, derived

**A move's msec.** Whole ms with the fraction carried in a double: the frame's ms (the physics
frame's with independent physics on, the uncapped drawn frame's with it off) is added to the
carry, the whole part is the msec, the fraction stays. Above 250 ms a move is set to 100 ms. At 77
a second: the first move 12 ms, then 13 ms, one 12 in every 77, each 77 moves summing to 1000 ms.
At 72 a second: 64 moves of 14 ms and 8 of 13 ms in every 72. read; the patterns derived by
running the rule for one second.

**Packets.** One move per physics frame, each with its own sequence number. Every packet carries
the current move and the two before it (three moves, fixed; delta-coded against the previous, the
oldest against an empty move); moves are never merged. Layout: move opcode, a sequence-keyed CRC
byte over the move bytes, a byte of measured loss in percent, the three moves, then (unless
`cl_nodelta` 1) the delta opcode and one byte of the frame to delta from. `cl_c2sdupe` (default 0,
0..3) sends each packet that many extra times. read

**`cl_c2spps`** (default 0 = every move sent). Non-zero is clamped to 10..72 packets a second; a
balance gains the drawn frame's time per move and loses one over the rate per send, bounded to
plus or minus 0.1 s; a move with no positive balance is held back (sequence number consumed, not
counted as loss) and rides in the next packet as a backup. Never more than 2 held back in a row,
never held back when a carried move has an impulse, per `cl_c2sImpulseBackup` (default 3: 1 checks
the current move, 2 also the previous, 3 also the one before). With independent physics on the
balance gains only the current drawn frame's time, so at high drawn rates the real packet rate
falls well below the setting. read, the last derived

**Independent physics off: `cl_maxfps`** (default 0) is the draw, physics and packet rate at
once, held to 30..cap where cap is the server's `maxfps` (raised to at least 30) or, when absent,
the ruleset's figure: 72 for `default` and `mtfl`, 77 for `smackdown`, `qcon`, `thunderdome`,
`smackdrive`. 0 means the cap on a local server, else `rate` (default 25000) over 80, held to
30..cap. With independent physics on `cl_maxfps` limits drawing only: 0 unlimited, otherwise at
least 30, no upper bound. Demo playback: 0 unlimited, else at least 30. Disconnected or minimized:
`cl_maxfps_menu` (default 0) when at least 30, else the display's refresh, else 30. read

**`cl_earlypackets`** (default 1, menu "Early Packets"). Only with independent physics on and not
in a demo: at 1 the drawn frames between physics frames also read packets, and on the frame a
packet arrives the other players' predictions and the client's own are rebuilt at once instead of
on the next physics frame; also the view angles are not interpolated across a teleport (a jump
over 100 units). At 0 packets are read only on physics frames. It never changes when moves are
made or sent. read

**Locks.** `cl_independentPhysics` and `ruleset` (default `default`) only while disconnected.
`smackdown`, `qcon`, `thunderdome`, `smackdrive`: with independent physics on, `cl_c2spps` forced 0
and read-only (writable again on leaving); their 77 figure matters only with independent physics
off. `mtfl`: `cl_c2spps` forced 0 and read-only always. No ruleset touches `cl_physfps`,
`cl_physfps_spectator`, `cl_earlypackets` or `cl_maxfps`. read

## Quake II client (Q2PRO master; the actionquake fork)

**Defaults.** `cl_maxfps` 62 (the manual says 60), `r_maxfps` 0, `cl_async` 1, `cl_maxpackets` 30,
`cl_packetdup` 1, `cl_batchcmds` 1, `cl_instantpacket` 1, `cl_fuzzhack` 0, `cl_warn_on_fps_rounding`
1. read

**`cl_async`.** 0: the whole loop (input, move, send, draw) at `cl_maxfps`. 1: moves finished at
`cl_maxfps`, drawing at `r_maxfps`, the loop itself unpaced (each pass updates the pending move's
time and angles and reruns prediction). 2 or more: drawing waits on the GPU's fence when the
renderer has one, else as 1. Out of game or window inactive: 60 a second; minimized: 10 a second,
no drawing. read

**Rates in whole ms.** Period = 1000 over the rate, rounded down; the true rate is 1000 over the
period (62 and 60 give 16 ms, 62.5 a second; 77 gives 12 ms, 83.3; 120 and 125 give 8 ms). An
inexact rate prints a rounding warning. `cl_maxfps` is clamped to 10..125 (the cvar rewritten), 0
meaning 125; `r_maxfps` to 10..1000, 0 meaning 1000. In async mode each clock's accumulator fires
at one period, subtracts it, and is cut back to one period past four (lost time is dropped); at
most one physics frame per pass. read, the examples derived

**A move's msec.** The loop's whole-ms elapsed time accumulates into the pending move; when a
physics frame finishes it, its msec is the ms since the last move, not the nominal period (16 or
17 ms at the default). No fraction to carry. Above 250 ms becomes 100 ms; paused adds nothing.
`cl_instantpacket` 1: `+attack` or `+use` in game finishes the move at once and sends past every
cap (in `cl_async` 0 an extra pass without drawing). read

**Batched send** (protocol 36, or the fork's AQtion protocol, with `cl_batchcmds` 1). A packet goes
when a new move exists and the `cl_maxpackets` interval has passed, or an instant packet, or
reliable data or a reliable ack is pending. It carries every move since the last packet (each its
own msec, never merged) plus the moves of the previous `cl_packetdup` packets (0..3). Layout: one
byte, opcode in the low 5 bits and the backup count in the high 3; a 32-bit frame number to delta
from (absent under the no-delta opcode); a light-level byte; then per packet, oldest first, a 5-bit
move count and the bit-packed moves; 32 or more in one group is refused with a warning. read

**Default send** (protocols 34 and 35, or `cl_batchcmds` 0). Every move sent at once, one packet
per move: opcode, (protocol 34 only) a sequence-keyed checksum byte, the 32-bit frame number (minus
one for no delta), then the current move and the two before it, each followed by a light-level
byte. `cl_maxpackets` applies here only with `cl_fuzzhack` 1, which drops a move inside the
interval (sequence number used), never more than 2 in a row. read

**`cl_maxpackets`** (default 30, 0 no cap, below 10 set to 10). Interval = 1000 over it rounded
down, then 100 over the whole number of intervals in 100 ms: 10 gives 100 ms, 20 gives 50, 30 gives
33, 40 and 45 give 25, 50 gives 20, 60 gives 16, 72 gives 14, 100 gives 10. With `cl_async` 1 the
check runs every pass, so the defaults give 2 or 3 new moves a packet at about 30 a second; with
`cl_async` 0 it runs only on 16 ms passes, so 33 ms is met at 48 ms, about 21 packets a second of 3
moves. derived from the read rules

**The server's frame.** Nothing ties the client's rates to the server's frame (`sv_fps`, default
10, latched); the client sends 62.5 moves a second by default. The Q2PRO server runs each received
move as its own think, against an allotment of 1800 ms reset every 16 base frames (1.6 s); with
`sv_enforcetime` 1 (default) an overdrawing move is ignored. read

**The fork.** `cl_maxfps` clamps to 10..250, 0 meaning 250 (4 ms); the GPU-synced mode is gone (any
non-zero `cl_async` paces drawing by `r_maxfps`); batched sends also on the AQtion protocol. Every
other default and rule above is unchanged. read

## QuakeWorld server's view kicks (mvdsv master)

The protocol's server messages 34 (small kick) and 35 (big kick) are bare opcodes, described as
setting the client's punch angle to 2 and 4. mvdsv never sends them itself, and its QuakeWorld
entity fields have no punch angle, so nothing in the player's state triggers them: no threshold,
cadence or clearing on the server. A mod sends one by writing the opcode byte to one client (the
reliable-to-one destination); the server appends it to that client's reliable stream and, while an
MVD records, to that player's single-client block. When the stock mod sends which kick is the
mod's choice and this source does not settle it. With NQ-style progs, writes to one client are
discarded and a 34 in a broadcast stream is read as a cutscene. The punch angle is sent in no other
form: the per-frame player state (origin, velocity, frame, the move and its msec, model, skin,
effects, weapon frame, its flags) carries none. read
