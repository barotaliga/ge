// THE CLASSIC PROTOCOL'S EXTENSIONS, as a table: each family's magic, each bit's name and value,
// and which bits our server offers (the first tier). Adding an extension is adding a row to
// `kExtBits` and the code that honours it; negotiation never changes.
//
// THE FAMILIES: `x` (magic 0x58455446, the four letters "FTEX" read little endian), `x2`
// (0x32455446, "FTE2") and the demo stream's `mvd` (0x3144564D, "MVD1") are negotiated in the
// challenge and the connect (the server offers a bit set, the client answers the bits it accepts,
// the server keeps the AND) or in the signon's `pext` step; the `z` set travels only in user info
// and server info (`*z_ext`), with no handshake.
//
// THE FIRST TIER we offer: every z bit (511); x transparency, sound-double, model-double,
// entity-double and entity-double2, spawnstatic2, 256 packet entities and chunked downloads; mvd
// float coords and the teleport kind on setangle. Everything else is refused. x float coords
// (only with sv_bigcoords) and the rest exist in the table so they can be named, never offered.
//
// ge:summary The classic protocol's extension families and bits as a table; the first tier
// ge:keywords extensions pext z_ext fte-bits mvd-bits negotiate offered first-tier transparency
// ge:keywords model-double entity-double chunked-downloads float-coords spawnstatic2 sound-double
// ge:skill net shooter
// ge:test tests/shooters_edge_classic_wire_test.cpp
#pragma once

#include <cstdint>
#include <string>

#include "shooters_edge/net/classic_wire.hpp"

namespace shooters_edge::net {

enum class ExtFamily : uint8_t { x, x2, mvd, z, count };
constexpr uint32_t kMagicX = 0x58455446, kMagicX2 = 0x32455446, kMagicMvd = 0x3144564d;

// The x family's bits.
constexpr uint32_t kXTrans = 0x8, kXSoundDbl = 0x80, kXModelDbl = 0x1000, kXEntityDbl = 0x2000;
constexpr uint32_t kXEntityDbl2 = 0x4000, kXFloatCoords = 0x8000, kXColourMod = 0x80000;
constexpr uint32_t kXSpawnStatic2 = 0x400000, kX256Entities = 0x1000000;
constexpr uint32_t kXChunkedDl = 0x20000000, kXClientProgs = 0x40000000;
// The x2 family's.
constexpr uint32_t kX2Voice = 0x2;
// The demo family's.
constexpr uint32_t kMvdFloatCoords = 0x1, kMvdTeleport = 0x2;
// The z set's.
constexpr uint32_t kZPmType = 1, kZPmTypeNew = 2, kZViewHeight = 4, kZServerTime = 8;
constexpr uint32_t kZPitchLimits = 0x10, kZJoinObserve = 0x20, kZOnGround = 0x40;
constexpr uint32_t kZVwep = 0x80, kZSolid = 0x100, kZAll = 511;

struct ExtBit {
  ExtFamily family;
  uint32_t bit;
  const char* name;
  bool first_tier;  // offered by our server
};
extern const ExtBit kExtBits[];
extern const int kExtBitCount;

uint32_t family_magic(ExtFamily f);
const char* family_name(ExtFamily f);
// The family a magic names; false when none.
bool family_of_magic(uint32_t magic, ExtFamily& out);

// A set of bits per family: what was offered, asked for, or agreed.
struct Ext {
  uint32_t bits[static_cast<int>(ExtFamily::count)] = {};
  uint32_t& operator[](ExtFamily f) { return bits[static_cast<int>(f)]; }
  uint32_t operator[](ExtFamily f) const { return bits[static_cast<int>(f)]; }
  bool has(ExtFamily f, uint32_t bit) const { return (bits[static_cast<int>(f)] & bit) != 0; }
};

// The first tier (with x float coords too when `bigcoords`).
Ext first_tier(bool bigcoords = false);
// What both sides keep: the bits of `asked` that `offered` has.
Ext agree(const Ext& offered, const Ext& asked);
// The coordinates the agreed bits make.
Coords coords_of(const Ext& e);
// The bit names set, comma separated ("none" for an empty set): a status line, a lab's table.
std::string describe_ext(const Ext& e);

}  // namespace shooters_edge::net
