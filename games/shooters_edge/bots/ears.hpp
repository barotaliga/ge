// WHAT A FIGHTER HEARS: the game's sounds read by the ears of its skill (hearing.hpp), and what
// each tells it about the map:
// - an item's PICKUP (the armour's, the mega's, a weapon's, a power's): the item whose spot the
//   sound came from is gone, taken by the player it came from. A player who knows the map
//   (average and expert) puts the sound on the item of that pickup nearest where it seemed to come
//   from, so he knows WHICH armour went, starts its timer the way a sighting does (knowledge.hpp:
//   an expert times it to the second), and knows where its taker is;
// - a SPAWN (the fog a player comes back in): a fresh, weak enemy at the spawn point nearest;
// - a teleporter's exit: the same sound, the player at its far end;
// - a SHOT, a CRY, a JUMP: someone is there, as far as the ear can judge.
// A sound from an enemy is a heard contact (senses.hpp: chased like a sighting a moment old); one
// from a teammate whose slot is known moves him on the fighter's map of its team (mind.hpp).
//
// THE SAMPLES are the game profile's data (profiles/items.hpp: `kClassicSounds`, and the heard
// words in `kClassicWords`), as the baseline mod plays them.
//
// Deterministic: the fighter's own stream decides what it takes in and how far off it judges it.
//
// ge:summary What a fighter hears: pickups put on their item and timed, spawns, shots placed
// ge:keywords ears hearing pickup-sound spawn-sound item-heard sound-table heard-contact
// ge:keywords fighter_hear ears_init hears_as
// ge:skill shooter bots
// ge:test tests/shooters_edge_ears_test.cpp
#pragma once

#include <span>

#include "shooter/hearing.hpp"
#include "shooters_edge/bots/fighter.hpp"

namespace shooters_edge::bots {

// The baseline mod's samples and what each means.
std::span<const ge::shooter::SoundMeaning> sound_table();

// The word an item's pickup sound is read as ("armor" for every armour, "mh", "weapon" ...).
const char* hears_as(const ItemSpot& it);

// The fighter's ears off its skill, and the map's spawn points off its entity text.
void ears_init(Fighter& f, ge::shooter::AimSkill skill, const char* entities);

struct EarNews {
  int32_t taken = -1;  // an item it heard taken this frame (-1 none)
  int32_t heard = 0;   // sounds taken in this frame
  int32_t spawns = 0;  // and of them, players heard spawning (or out of a teleporter)
};

// This frame's sounds as the fighter (entity `me`, its ear at `ear`) takes them in at time `t`;
// once a server frame, however many commands it sends in it.
EarNews fighter_hear(Fighter& f, const server::QcServer& s, int me, ge::shooter::Vec3f ear,
                     double t, bool fighting);

}  // namespace shooters_edge::bots
