// THE CLASSIC PROTOCOL'S STATE DELTAS: an entity's state and its delta record (packet entities,
// svc 47 and 48, the baselines and static entities under spawnstatic2), and a player's state
// (svc 42).
//
// AN ENTITY RECORD: a u16 word with the number in bits 0..8 and flags in bits 9..15 (origin x
// 1<<9, origin y 1<<10, origin z 1<<11, angle y 1<<12, frame 1<<13, remove 1<<14, more bits
// 1<<15); with more bits a byte (angle x 1, angle z 2, model 4, colormap 8, skin 0x10, effects
// 0x20, solid 0x40, even more 0x80); with even more (x family) a byte (transparency 2,
// model-double 8, number +512 0x20, number +1024 0x40, yet more 0x80); with yet more a byte. The
// flags are kept here as one 32-bit word: the word's bits 9..15 as they are, the more byte in bits
// 0..7, the even-more byte in 16..23, the yet-more byte in 24..31. Fields follow in the order
// model, frame, colormap, skin, effects, origin x, angle x, origin y, angle y, origin z, angle z,
// transparency. A model above 255 travels as a short under model-double with the model bit clear
// (a record with both bits reads a byte plus 256). A removal is the word with the remove bit (and
// the number's high bits for an entity above 511). A list ends with a u16 0.
//
// A LIST (`write_packet_entities`): the visible entities by number, each a delta from the
// client's acknowledged frame when there is one (svc 48 with that frame's sequence's low byte),
// else from its baseline (svc 47). An entity in the from-frame but not in the list is removed; a
// new one comes from its baseline, every field it differs in. States are compared on the wire's
// grid (`grid_entity`), so a list of unchanged entities is the end word alone.
//
// A PLAYER (svc 42): byte slot, short flags (under transparency, flag bit 15 set adds a byte:
// flags 16..23), origin x 3, byte frame, then by flag: msec 0x1 (byte), command 0x2 (a delta
// usercmd from the null command), velocity x, y, z 0x4, 0x8, 0x10 (shorts), model 0x20 (byte),
// skin 0x40 (byte; bit 7 adds 256 to the model), effects 0x80, weapon frame 0x100, dead 0x200,
// gib 0x400 (no field), the move type in bits 11..13, on ground and solid in bits 14 and 15 (22
// and 23 in the extended form), transparency bit 17 (a byte, extended form only).
//
// ge:summary Entity and player states as the classic protocol deltas them, lists and records
// ge:keywords packet-entities deltapacketentities entity-delta baseline remove playerinfo
// ge:keywords player-state pflags u-origin u-morebits entity-number model-double transparency
// ge:keywords write_entity_delta read_entity_record write_packet_entities read_packet_entities
// ge:keywords write_playerinfo read_playerinfo grid_entity
// ge:skill net shooter
// ge:test tests/shooters_edge_classic_messages_test.cpp
#pragma once

#include <cstdint>
#include <vector>

#include "shooter/pmove.hpp"
#include "shooters_edge/net/classic_ext.hpp"

namespace shooters_edge::net {

// The word's flags.
constexpr uint32_t kUOrigin1 = 1u << 9, kUOrigin2 = 1u << 10, kUOrigin3 = 1u << 11;
constexpr uint32_t kUAngle2 = 1u << 12, kUFrame = 1u << 13, kURemove = 1u << 14;
constexpr uint32_t kUMoreBits = 1u << 15;
// The more byte (bits 0..7 of the combined flags).
constexpr uint32_t kUAngle1 = 1, kUAngle3 = 2, kUModel = 4, kUColormap = 8, kUSkin = 0x10;
constexpr uint32_t kUEffects = 0x20, kUSolid = 0x40, kUEvenMore = 0x80;
// The even-more byte (bits 16..23).
constexpr uint32_t kUTrans = 2u << 16, kUModelDbl = 8u << 16, kUEntityDbl = 0x20u << 16;
constexpr uint32_t kUEntityDbl2 = 0x40u << 16, kUYetMore = 0x80u << 16;

struct EntityState {
  int number = 0;
  int modelindex = 0, frame = 0, colormap = 0, skin = 0, effects = 0;
  int alpha = 0;  // transparency 1..254 (x family); 0 opaque
  sh::Vec3f origin, angles;
};

struct EntityRecord {
  int number = 0;
  uint32_t bits = 0;
  EntityState state;  // the record applied to the state it was read against
};

// A state on the wire's grid for `coords` (what a receiver reads back).
EntityState grid_entity(const EntityState& s, Coords coords);

// The bits that tell `to` from `from` (on the grid already), with the number's.
uint32_t entity_bits(const EntityState& from, const EntityState& to, const Ext& ext);
// A record of `to` against `from`; with `force` written even when nothing changed. Nothing is
// written (false) when nothing changed and nothing is forced.
bool write_entity_delta(gn::Writer& w, const EntityState& from, const EntityState& to, bool force,
                        const Ext& ext);
void write_entity_remove(gn::Writer& w, int number);
// One record, its word already read (`word` non-zero), against `from`.
EntityRecord read_entity_record(gn::Reader& r, uint16_t word, const EntityState& from,
                                const Ext& ext);

// A whole list (the svc byte and, for a delta, the from-sequence's byte are the caller's). `from`
// null: every entity from its baseline. `baselines` by entity number.
void write_packet_entities(gn::Writer& w, const std::vector<EntityState>* from,
                           const std::vector<EntityState>& to,
                           const std::vector<EntityState>& baselines, const Ext& ext);
// Reads a list against `from` (null for a full list) onto `out`, sorted by number; the records
// as read in `records` when given. False when the list is damaged.
bool read_packet_entities(gn::Reader& r, const std::vector<EntityState>* from,
                          const std::vector<EntityState>& baselines, const Ext& ext,
                          std::vector<EntityState>& out,
                          std::vector<EntityRecord>* records = nullptr);

// Player flags (logical: `write_playerinfo` places on-ground and solid by the form).
constexpr uint32_t kPfMsec = 0x1, kPfCommand = 0x2, kPfVelocity1 = 0x4, kPfVelocity2 = 0x8;
constexpr uint32_t kPfVelocity3 = 0x10, kPfModel = 0x20, kPfSkin = 0x40, kPfEffects = 0x80;
constexpr uint32_t kPfWeaponFrame = 0x100, kPfDead = 0x200, kPfGib = 0x400;
constexpr int kPfPmTypeShift = 11;
constexpr uint32_t kPfOnGround = 1u << 14, kPfSolid = 1u << 15;    // base form
constexpr uint32_t kPfExtra = 1u << 15, kPfTrans = 1u << 17;       // extended form
constexpr uint32_t kPfOnGroundX = 1u << 22, kPfSolidX = 1u << 23;  // extended form

// The move types bits 11..13 carry.
enum class PmType : uint8_t { normal, normal_jump_held, old_spectator, spectator, fly, none, lock };

struct PlayerState {
  int slot = 0;
  sh::Vec3f origin;
  int frame = 0;
  bool has_msec = false;
  int msec = 0;
  bool has_command = false;
  sh::MoveCmd command;
  sh::Vec3f velocity;  // whole units/s
  int modelindex = 0;  // 0: the player model (no model field)
  int skin = 0, effects = 0;
  bool has_weaponframe = false;
  int weaponframe = 0;
  bool dead = false, gib = false;
  PmType pm = PmType::normal;
  bool onground = false, solid = false;  // sent under the z set's on-ground and solid bits
  int alpha = 0;                         // transparency 1..254 (x family)
};

// The flags as written (by the form the agreed bits make).
uint32_t playerinfo_flags(const PlayerState& p, const Ext& ext);
void write_playerinfo(gn::Writer& w, const PlayerState& p, const Ext& ext);
PlayerState read_playerinfo(gn::Reader& r, const Ext& ext, uint32_t* flags = nullptr);

}  // namespace shooters_edge::net
