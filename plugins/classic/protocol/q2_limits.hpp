// QUAKE II'S EXTENDED LIMITS, THE TABLE BOTH SIDES READ: protocol 36's extension levels (minor
// 1024 on), the limits and configstring layout each one uses, and the codings of the fields only
// they carry. The server (games/shooters_edge/net, server/q2_wire*) writes them and the client
// (net/q2_proto_in, the demo reader) reads them off this one table, so the two never disagree.
//
// THE LEVELS. A server turns the extended limits on when the hosted game library sets its
// protocol-extensions feature bit (15); clients are then granted:
//   extensions    every client on the extended server (protocol 36, minor 1024 or later)
//   extensions 2  also when the library is the newer interface (3302): minor 1025 or later
//   more bits     on top of extensions 2 for minor 1026 or later (the player's fog)
// The server data's flags word (36, minor 1024 on) says which: `kQ2ServerFlag*` below; a client
// told extensions 2 without extensions drops.
//
// THE CONFIGSTRINGS. The first five (the name, the CD track, the sky, its axis and turn) never
// move; the status bar starts at 5 and may run on through the strings up to the next block (23 of
// them, or 53 extended); the rest move to make room for more models, sounds and images:
//   block          ordinary   extended
//   air accel      29         59
//   max clients    30         60
//   map checksum   31         61
//   models         32 (256)   62 (8192)
//   sounds         288 (256)  8254 (2048)
//   images         544 (256)  10302 (2048)
//   light styles   800 (256)  12350 (256)
//   items          1056 (256) 12606 (256)
//   player skins   1312 (256) 12862 (256)
//   general        1568 (512) 13118 (512)
//   end (count)    2080       13630
// `q2_cs_remap` moves an index from one layout to the other (-1 when it has no slot there).
//
// THE CODINGS. A 23-bit coordinate (extensions 2: origins, the player's origin and velocity, a
// sound's position, temporary entities' positions) is in 1/8 units: when the change from the last
// value lies in -16384..16383 it is a 16-bit word holding the change shifted left one (low bit 0);
// otherwise three bytes, little endian, holding the absolute value shifted left one with the low
// bit 1 (sign-extended from 23 bits on read). An absolute one is the same with a last value of 0.
// Extensions 2's stat mask is a variable-length integer: engine/net/wire.hpp's `write_varint`.
//
// ge:summary Quake II's extended limits: the levels, the limits, the configstring layouts, codings
// ge:keywords q2 protocol-36 extended-limits protocol-extensions configstring-layout q2_cs_remap
// ge:keywords coord23 stat-mask loop-attenuation loop-volume alpha scale model16 minor
// ge:keywords extensions2 more-bits server-data-flags q2_limits
// ge:skill classic net
// ge:test tests/classic_q2_limits_test.cpp
#pragma once

#include <cstdint>

#include "net/wire.hpp"

namespace ge::classic::protocol {

// The minors that bring each level, and the window a client's minor is clipped into.
inline constexpr int kQ2MinorExtensions = 1024;
inline constexpr int kQ2MinorExtensions2 = 1025;
inline constexpr int kQ2MinorMoreBits = 1026;
inline constexpr int kQ2MinorLowest = 1015, kQ2MinorHighest = 1026;
// The client's minor as the server takes it: clipped into the window, 1016 counted as 1015.
int q2_clip_minor(int minor);

// The library's feature bit that turns the extended limits on, and the newer interface's version.
inline constexpr uint32_t kQ2FeatureProtocolExtensions = 1u << 15;
inline constexpr int kQ2GameVersion = 3, kQ2GameVersionNew = 3302;

// The server data's flags word (protocol 36, minor 1024 on).
inline constexpr int kQ2ServerFlagStrafeJump = 1 << 0;
inline constexpr int kQ2ServerFlagQwMode = 1 << 1;
inline constexpr int kQ2ServerFlagWaterJump = 1 << 2;
inline constexpr int kQ2ServerFlagExtensions = 1 << 3;
inline constexpr int kQ2ServerFlagExtensions2 = 1 << 4;

struct Q2Limits {
  int entities, models, sounds, images, lights, items, clients, general;
  int frame_entities;  // entities in one client frame
  int stats;           // the player's stats on the wire
};
inline constexpr Q2Limits kQ2Limits{1024, 256, 256, 256, 256, 256, 256, 512, 128, 32};
inline constexpr Q2Limits kQ2LimitsExtended{8192, 8192, 2048, 2048, 256, 256, 256, 512, 512, 32};
inline constexpr int kQ2StatsExtensions2 = 64;  // extensions 2 carries 64 stats

struct Q2CsLayout {
  int air_accel, max_clients, map_checksum;
  int models, sounds, images, lights, items, player_skins, general;
  int end;  // the count, and the configstring stream's terminator
};
inline constexpr int kQ2CsName = 0, kQ2CsCdTrack = 1, kQ2CsSky = 2, kQ2CsSkyAxis = 3,
                     kQ2CsSkyRotate = 4, kQ2CsStatusBar = 5;
inline constexpr Q2CsLayout kQ2Cs{29, 30, 31, 32, 288, 544, 800, 1056, 1312, 1568, 2080};
inline constexpr Q2CsLayout kQ2CsExtended{59,    60,    61,    62,    8254, 10302,
                                          12350, 12606, 12862, 13118, 13630};
inline const Q2CsLayout& q2_cs_layout(bool extended) { return extended ? kQ2CsExtended : kQ2Cs; }
inline const Q2Limits& q2_limits(bool extended) { return extended ? kQ2LimitsExtended : kQ2Limits; }

// `index` in layout `from` as the same slot in layout `to`; -1 when `to` has no such slot (a
// model past 256 into the ordinary layout) or `index` is outside `from`.
int q2_cs_remap(int index, const Q2CsLayout& from, const Q2CsLayout& to);

// The entity delta's bits only the extended limits use (bit 31 says a fifth bits byte follows).
inline constexpr uint64_t kQ2UModel16 = 1ull << 28;    // every model index sent is a word
inline constexpr uint64_t kQ2UMoreFx8 = 1ull << 29;    // more effects: with 16, a long
inline constexpr uint64_t kQ2UAlpha = 1ull << 30;      // a byte
inline constexpr uint64_t kQ2UMoreBits4 = 1ull << 31;  // a fifth bits byte follows
inline constexpr uint64_t kQ2UScale = 1ull << 32;      // a byte
inline constexpr uint64_t kQ2UMoreFx16 = 1ull << 33;
// The looping sound word (extended): the index in its low 14 bits, then which bytes follow.
inline constexpr int kQ2LoopIndexMask = 0x3fff;
inline constexpr int kQ2LoopVolume = 1 << 14, kQ2LoopAttenuation = 1 << 15;
// The sound message's flag for a 16-bit index (an extended server's sound past 255).
inline constexpr int kQ2SoundIndex16 = 1 << 5;
// The print levels an extended server adds after chat (3): shown centred, the first typed out.
inline constexpr int kQ2PrintTypewriter = 4, kQ2PrintCenter = 5;

// The byte codings of the extended entity fields. 0 is each one's default.
uint8_t q2_alpha_to_wire(float alpha);         // x 255, clamped 1..255
float q2_alpha_from_wire(uint8_t b);           // 0: 1 (opaque)
uint8_t q2_scale_to_wire(float scale);         // x 16, clamped 1..255; 1 sent as 0
float q2_scale_from_wire(uint8_t b);           // 0: 1
uint8_t q2_loop_volume_to_wire(float volume);  // x 255, clamped; 1 sent as 0
float q2_loop_volume_from_wire(uint8_t b);     // 0: 1
// Attenuation x 64 clamped 1..255; the static value (3) coded as 0, none (-1) as 192.
uint8_t q2_loop_attenuation_to_wire(float attenuation);
float q2_loop_attenuation_from_wire(uint8_t b);

// A 23-bit coordinate in 1/8 units against the last value `from` (0: absolute).
void write_q2_coord23(net::Writer& w, int32_t value, int32_t from);
int32_t read_q2_coord23(net::Reader& r, int32_t from);

}  // namespace ge::classic::protocol
