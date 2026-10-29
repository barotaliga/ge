// THE SCRIPTED MATCH: Shooter's Edge's loop run end to end, deterministically, on the test arena.
// Two clients play from a script of key presses and view turns; each renders every
// `client_frame_ms`, builds commands at the server's 77 a second (usercmd.hpp), and sends each
// packet (its three newest commands) to arrive `latency_ms` later, the second player losing every
// `lose_every`-th one; the server runs a frame every 13 ms on the run clock (frame.hpp), the
// packets that arrived since the last frame in order of arrival, then client, then sequence; each
// client owed a reply gets one `latency_ms` later (its acknowledged command, its own player and
// the other player as the protocol carries them, the door's and the plat's places); the client
// predicts its own player from the acknowledged state over its newer commands, smooths what a
// reply corrects, and predicts the other player forward (predict.hpp).
//
// The whole run is a pure function of the options: one timeline in whole milliseconds, no clock,
// no randomness. `MatchReport::frames[i].hash` is the server's whole state after frame i
// (`server_hash`), so two runs agreeing on every frame's hash replay to the bit.
//
// The script (`match_script`): player 1 looks down and fires a rocket at his feet, takes the
// health box, runs and jumps east through the doorway (opening the door) and comes back to stand
// in it as it closes (the door is blocked and goes back up); player 2 rides the plat up, steps off
// onto the ledge, throws a grenade that bounces across the east room, switches to rockets and
// fires at the far wall, and strafe-jumps.
//
// ge:summary Shooter's Edge's scripted match: two clients, latency, loss, prediction, the server
// ge:keywords match scripted-match script clients latency loss packets replies prediction
// ge:keywords smoothing hash replay deterministic run_match match_script frame-record
// ge:skill shooter
// ge:test tests/shooters_edge_match_test.cpp
#pragma once

#include <cstdint>
#include <vector>

#include "shooter/predict.hpp"
#include "shooters_edge/sim/rules.hpp"

namespace shooters_edge::sim {

constexpr int kServerFrameMs = 13;
constexpr int kPlayers = 2;

enum class ScriptKey : uint8_t { forward, back, left, right, jump, attack, speed };
enum class ScriptAct : uint8_t { press, release, yaw, pitch, impulse };

struct ScriptStep {
  int t_ms = 0;
  int player = 0;  // 0 or 1
  ScriptAct act = ScriptAct::press;
  ScriptKey key = ScriptKey::forward;
  float value = 0;  // degrees for yaw and pitch; the impulse's number
};

// The match's script, in time order.
const std::vector<ScriptStep>& match_script();

struct MatchOptions {
  int frames = 1100;        // server frames of 13 ms (14.3 s)
  int client_frame_ms = 8;  // each client renders every this many ms
  int latency_ms = 40;      // one way, each direction
  int lose_every = 23;      // player 2 loses every Nth packet it sends (0: none lost)
  bool predict = true;      // the clients predict (false: they show the server's state)
};

// The server after one frame.
struct FrameRecord {
  double time = 0;
  sh::Vec3f origin[kPlayers], velocity[kPlayers];
  bool onground[kPlayers] = {false, false};
  float health[kPlayers] = {0, 0};
  float door_z = 0, plat_z = 0;
  uint64_t hash = 0;
};

// A missile's path, one point a server frame from its first.
struct MissileTrack {
  int ent = 0;
  bool rocket = true;
  int first_frame = 0;
  std::vector<sh::Vec3f> points;
};

// A client's view of its own player at one of its frames.
struct ViewSample {
  int t_ms = 0;
  sh::Vec3f predicted;  // the prediction over the unacknowledged commands
  sh::Vec3f shown;      // with the smoothing's correction on top
  int unacked = 0;      // commands predicted over
};

// A reply's correction: the newest command's prediction before and after it.
struct Correction {
  int t_ms = 0;
  float error = 0;  // units
};

struct ClientReport {
  int packets_sent = 0, packets_lost = 0, replies = 0, commands = 0;
  std::vector<ViewSample> views;
  std::vector<Correction> corrections;
  float worst_correction = 0;
};

struct MatchReport {
  std::vector<FrameRecord> frames;
  std::vector<MissileTrack> missiles;
  std::vector<GameEvent> events;
  ClientReport clients[kPlayers];
  sh::LoopCounts counts;
  uint64_t final_hash = 0;
};

MatchReport run_match(const MatchOptions& opt);

}  // namespace shooters_edge::sim
