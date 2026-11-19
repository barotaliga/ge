// LAG COMPENSATION on both classic games' servers: a player's shot is traced against the other
// players where the shooter's screen showed them, not where they are now.
//
// THE HISTORY: each player keeps a ring of kAntilagSamples (server time, origin, box) samples,
// recorded after each frame. A step between two samples longer than any run could make
// (kAntilagJump units plus kAntilagSpeed units a second: a teleport, a respawn) starts the ring
// afresh, so a rewind never blends a player across a teleporter.
// THE TARGET TIME (`antilag_target`): the server time of the frame the shooter's last packet
// acknowledged, plus 0.02 s (or one of his command frames, 1 / maxfps, when his ping is under
// 20 ms; a maxfps outside 20..1000 counts as 77), less `back` (the Quake II server's
// sv_antilag_interp: one frame more for the client's interpolation), never later than now and,
// with `max_back` above 0 (sv_antilag_max), never earlier than now less that; with `exact`
// (sv_antilagexact) the acknowledged frame's own time, no lead.
// A PLAYER AT THE TARGET TIME (`antilag_place`): later than his newest sample, extrapolated
// from it by his velocity times min(gap, 0.02 s); with one sample, that sample; earlier than his
// oldest, the oldest; otherwise interpolated between the samples on either side, his box the
// earlier sample's (a crouch is rewound with him).
// THE LEVELS (sv_antilag): 0 off; 1 the traces the rules mark (kAntilagMarked added to a trace's
// kind, or a missile with kAntilagFlag in its flags); 2 every line a player's shot traces, and
// with sv_antilag_projectiles each player's missiles in flight too. A player whose userinfo has
// `cl_antilag 0` turns it off for his own shots (`antilag_wanted`).
// THE TRACE: a lagged trace sees every other live, spawned player (bots too) moved to his place
// at the target time; they are put back after it. A bot's own shots are never rewound.
//
// ge:summary Lag compensation for hitscan: the players' origin history, the target time, rewinds
// ge:keywords antilag lag-compensation rewind hitscan history target-time extrapolate
// ge:keywords interpolate antilag_record antilag_target antilag_position lagged-trace
// ge:keywords antilag_place antilag_wanted cl_antilag sv_antilag_max sv_antilag_projectiles
// ge:keywords sv_antilag_interp teleport-reset crouch-rewind
// ge:skill net shooter
// ge:test tests/shooters_edge_classic_antilag_test.cpp
#pragma once

#include <string>

#include "shooter/vec.hpp"

namespace shooters_edge::net {

namespace sh = ge::shooter;

constexpr int kAntilagSamples = 128;
constexpr double kAntilagLead = 0.02;  // s added to the acknowledged frame's time
constexpr int kAntilagPlayers = 32;
constexpr float kAntilagJump = 64;     // units: a longer step between samples starts afresh...
constexpr float kAntilagSpeed = 3000;  // ...past this many units a second (above any run's)
constexpr int kAntilagMarked = 64;     // added to a trace's kind: rewind it under sv_antilag 1
constexpr int kAntilagFlag = 65536;    // in a missile's flags: its flight is rewound

struct AntilagSample {
  double time = 0;  // the server's time, s
  sh::Vec3f origin, mins, maxs;
};

struct AntilagPlace {
  sh::Vec3f origin, mins, maxs;
};

struct AntilagRing {
  AntilagSample samples[kAntilagSamples];
  int count = 0;
  int head = 0;  // the next sample's index
};

struct AntilagHistory {
  AntilagRing players[kAntilagPlayers];
};

void antilag_record(AntilagHistory& h, int slot, double time, sh::Vec3f origin, sh::Vec3f mins = {},
                    sh::Vec3f maxs = {});
void antilag_clear(AntilagHistory& h, int slot);

// The time a shooter's shot is traced at (`exact`: the acknowledged frame's own, with no lead;
// `max_back` s, above 0: no further back than that; `back` s: further back by that).
double antilag_target(double acked_frame_time, double ping_s, int maxfps, double now,
                      bool exact = false, double max_back = 0, double back = 0);

// Where a player was at `t`, and his box; false when he has no sample.
bool antilag_place(const AntilagRing& ring, double t, sh::Vec3f velocity, AntilagPlace& out);
bool antilag_position(const AntilagRing& ring, double t, sh::Vec3f velocity, sh::Vec3f& out);

// Whether a player's own shots are rewound by his userinfo's `cl_antilag`: unset or above 0 yes.
bool antilag_wanted(const std::string& cl_antilag);
// The line a player reads when his `cl_antilag` turns it on or off for his shots.
const char* antilag_told(bool wanted);

}  // namespace shooters_edge::net
