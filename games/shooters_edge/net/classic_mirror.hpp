// THE CLIENT'S WORLD: a fighter's world rebuilt from what its classic client was sent, so a bot
// plays on a server it cannot read (an existing server somewhere else) exactly as it plays on
// ours. The fighter (bots/fighter.hpp) reads a `server::RulesCommon`; the mirror is one, filled
// only from the client (classic_client.hpp), so whatever the fighter reads is a fact the server
// sent this player and nothing more.
//
// WHAT IS FILLED, each `mirror_update` (one a client tick, before its move is asked for):
// - THE PLAYERS sent in the newest packet (the client's own always is): their places, their views
//   (another player's off the command sent with him), their models; a player the packet left out
//   (outside the client's visibility set) is absent, as he is from the client's screen. A
//   player's health is not sent: another player reads 100, or 0 under the protocol's dead flag.
// - THE CLIENT'S OWN PLAYER predicted to now (plugins/shooter/predict.hpp): from the state the
//   server sent in reply to the newest acknowledged move, every move sent since run through the
//   same player move over the map, its movers where the newest frame put them and the other
//   players, as the window's remote client predicts it. So on the ground, in the water and the
//   velocity are the move's answers, not guesses. `predict false` takes the newest state as sent.
// - ITS STATS as the server sent them (health, armour and its kind off the item bits, the
//   ammunition, the item bits, the weapon in hand), and every player's frags.
// - THE ENTITIES of the newest valid frame: their places, angles, models, frames and effects; a
//   brush entity (a door, a plat, a button) linked with its model's bounds so the senses' traces
//   meet it; an item given the box the classic items stand in (its kind read off the model's
//   name). With `baselines` set, every entity the signon's baselines named and the frame left out
//   stands at its baseline: what a client that loaded the map knows before it has seen a thing,
//   so a fighter set up on it finds every item, not only those in view.
// - THE SOUNDS this tick's packets started, each where the packet put it.
// - THE CLOCK: the network's time, the client's own (`ClassicClientEnd::now`). Every bot timer is
//   a difference, so the server's own clock is never needed.
// - THE LISTS: the model and sound names, the map's name, the user infos (name, team, spectator).
//
// WHAT IS NOT: the program, its fields and globals, another player's health, armour or weapons,
// any entity the server did not send. A fighter that needs one has no field to read: it reads 0.
//
// THE MODELS: the brush models are the map file's (`MapSpec::models`, submodel k at k), laid out
// by the client's model list (`*k` at its index), so a modelindex the server sent finds its hulls.
//
// ge:summary A fighter's world rebuilt from what its classic client was sent, for any server
// ge:keywords client-mirror mirror client-world fighter-world existing-server live-server
// ge:keywords mirror_start mirror_update predicted own-player baselines sounds frags stats
// ge:keywords only-what-is-sent bot-honesty remote-bots
// ge:skill bots net shooter
// ge:test tests/shooters_edge_classic_mirror_test.cpp
#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "shooter/brush_hull.hpp"
#include "shooters_edge/net/classic_client.hpp"
#include "shooters_edge/server/rules_runner.hpp"

namespace shooters_edge::net {

namespace sh = ge::shooter;

struct ClientMirror {
  server::RulesCommon s;   // the fighter's world: only what the client was sent
  bool predict = true;     // the client's own player run forward through the moves since acked
  bool baselines = false;  // entities the frame left out stand at their baselines (`mirror_update`)

  // Counts.
  int64_t updates = 0;
  int64_t predicted = 0;  // updates whose own player came off the prediction
  int64_t players_sent = 0, entities_sent = 0, sounds_sent = 0;  // the newest update's

  // Kept for the update.
  std::vector<sh::BrushModel> map_models;  // the map's: [0] the world, [k] submodel *k
  std::vector<int> linked;                 // entities linked into the area tree last update
  std::vector<std::string> sound_names;    // the sound list the sounds' samples point into
};

// Ready to mirror a client whose signon has named the map's lists (the model list read; the
// client active). `models` are the map file's brush models. False (and `error`, with the next
// step) when the lists do not fit the map: the map file is not the server's.
bool mirror_start(ClientMirror& m, const std::vector<sh::BrushModel>& models,
                  const ClassicClientEnd& c, std::string& error);

// The world as the client holds it now (see the header).
void mirror_update(ClientMirror& m, const ClassicClientEnd& c);

// The box a classic item stands in, off its model's name: the brush-model boxes (health,
// ammunition) from their corner, a powerup about its middle, anything else on its floor.
void item_box(const std::string& model, sh::Vec3f& mins, sh::Vec3f& maxs);

}  // namespace shooters_edge::net
