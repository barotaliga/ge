// QUAKE II'S RECORDER'S DUMMY SPECTATOR: the client `[MVDSPEC]` the multiview recorder keeps in
// the game while it is on, so the stream carries a scoreboard and the messages a viewer reads
// (porting/shooters-edge/q2-mvd-dummy-facts.md § A).
//
// IT IS MADE when the recorder switches on (a relay's stream start, `mvdrecord`), and with
// `sv_mvd_autorecord` at every level (the recorder switched on with it, its pause off), unless
// `sv_mvd_spawn_dummy` is 0 or less (1: when the rules know it, which ours do; 2: always). It
// takes the first free slot past the players' and spectators' seats, else the highest free one
// (`No slot for dummy MVD client` and the recorder refused when none is free). It joins the
// rules as a spectator named `[MVDSPEC]` with the skin `male/grunt`, and is seated again at
// every level; each seating adds `sv_mvd_begincmd` to its commands.
//
// EACH 10 Hz FRAME, before the recorder's block: the relays' forwarded commands run as its own,
// then its command buffer up to a `wait N` (N frames, at least 1), the wait counted down; then,
// while the stream plays and `sv_mvd_scorecmd` is set, the score command added when its
// scoreboard has not been seen for 9 seconds. Its words: `cmd <line>` and anything unknown go to
// the rules as its command; `set` and `alias`, and a setting's name, to the server's console;
// `play`, `stopsound`, `exec` and `screenshot` ignored; `record <name>` and `stop` with
// `sv_mvd_autorecord` (an auto-recording). After the block, the lines the game stuffed it (with
// `sv_mvd_allow_stufftext` above 0) join its buffer. It leaves when the recorder switches off.
//
// ge:summary Quake II's recorder's dummy spectator: its slot, its commands, the scoreboard it holds
// ge:keywords mvdspec dummy sv_mvd_spawn_dummy sv_mvd_begincmd sv_mvd_scorecmd mvdstuff
// ge:keywords sv_mvd_autorecord sv_mvd_capture_flags sv_mvd_nomsgs
// ge:skill net shooter
// ge:test tests/shooters_edge_q2_mvd_server_test.cpp
#pragma once

#include "shooters_edge/net/q2_net_server.hpp"
#include "shooters_edge/server/q2_mvd2_live.hpp"

namespace shooters_edge::net {

// The dummy's turn before a recorder's block (`level`: a level's game state): made, seated or
// run as the header says, and `cap` told its slot and the capture's settings.
void q2_mvd_dummy_before(Q2NetServer& s, bool level, sv::Mvd2Capture& cap);
// After the block: the scoreboard's clock, the game's stuffed lines, and the dummy gone when the
// recorder switched off.
void q2_mvd_dummy_after(Q2NetServer& s);

}  // namespace shooters_edge::net
