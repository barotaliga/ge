// The classic protocol's extension table (classic_ext.hpp).
//
// ge:summary The extension table's rows, the first tier, agreement and the coordinates it makes
// ge:keywords kextbits first_tier agree coords_of describe_ext family_magic
// ge:skill net shooter
// ge:test tests/shooters_edge_classic_wire_test.cpp
#include "shooters_edge/net/classic_ext.hpp"

namespace shooters_edge::net {

const ExtBit kExtBits[] = {
    {ExtFamily::x, kXTrans, "transparency", true},
    {ExtFamily::x, kXSoundDbl, "sound-double", true},
    {ExtFamily::x, kXModelDbl, "model-double", true},
    {ExtFamily::x, kXEntityDbl, "entity-double", true},
    {ExtFamily::x, kXEntityDbl2, "entity-double2", true},
    {ExtFamily::x, kXFloatCoords, "float-coords", false},
    {ExtFamily::x, kXColourMod, "colour-mod", false},
    {ExtFamily::x, kXSpawnStatic2, "spawnstatic2", true},
    {ExtFamily::x, kX256Entities, "256-entities", true},
    {ExtFamily::x, kXChunkedDl, "chunked-downloads", true},
    {ExtFamily::x, kXClientProgs, "client-programs", false},
    {ExtFamily::x2, kX2Voice, "voice", false},
    {ExtFamily::mvd, kMvdFloatCoords, "origin-floats", true},
    {ExtFamily::mvd, kMvdTeleport, "teleport-kind", true},
    {ExtFamily::z, kZPmType, "pm-type", true},
    {ExtFamily::z, kZPmTypeNew, "pm-type-new", true},
    {ExtFamily::z, kZViewHeight, "view-height", true},
    {ExtFamily::z, kZServerTime, "server-time", true},
    {ExtFamily::z, kZPitchLimits, "pitch-limits", true},
    {ExtFamily::z, kZJoinObserve, "join-observe", true},
    {ExtFamily::z, kZOnGround, "on-ground", true},
    {ExtFamily::z, kZVwep, "view-weapons", true},
    {ExtFamily::z, kZSolid, "solid", true},
};
const int kExtBitCount = static_cast<int>(sizeof kExtBits / sizeof kExtBits[0]);

uint32_t family_magic(ExtFamily f) {
  switch (f) {
    case ExtFamily::x:
      return kMagicX;
    case ExtFamily::x2:
      return kMagicX2;
    case ExtFamily::mvd:
      return kMagicMvd;
    default:
      return 0;
  }
}

const char* family_name(ExtFamily f) {
  switch (f) {
    case ExtFamily::x:
      return "x";
    case ExtFamily::x2:
      return "x2";
    case ExtFamily::mvd:
      return "mvd";
    case ExtFamily::z:
      return "z";
    default:
      return "?";
  }
}

bool family_of_magic(uint32_t magic, ExtFamily& out) {
  for (ExtFamily f : {ExtFamily::x, ExtFamily::x2, ExtFamily::mvd})
    if (family_magic(f) == magic) {
      out = f;
      return true;
    }
  return false;
}

Ext first_tier(bool bigcoords) {
  Ext e;
  for (int i = 0; i < kExtBitCount; i++)
    if (kExtBits[i].first_tier) e[kExtBits[i].family] |= kExtBits[i].bit;
  if (bigcoords) e[ExtFamily::x] |= kXFloatCoords;
  return e;
}

Ext agree(const Ext& offered, const Ext& asked) {
  Ext e;
  for (int f = 0; f < static_cast<int>(ExtFamily::count); f++)
    e.bits[f] = offered.bits[f] & asked.bits[f];
  return e;
}

Coords coords_of(const Ext& e) {
  if (e.has(ExtFamily::x, kXFloatCoords)) return Coords::all;
  if (e.has(ExtFamily::mvd, kMvdFloatCoords)) return Coords::origins;
  return Coords::shorts;
}

std::string describe_ext(const Ext& e) {
  std::string s;
  for (int i = 0; i < kExtBitCount; i++)
    if (e.has(kExtBits[i].family, kExtBits[i].bit)) {
      if (!s.empty()) s += ",";
      s += family_name(kExtBits[i].family);
      s += ":";
      s += kExtBits[i].name;
    }
  return s.empty() ? "none" : s;
}

}  // namespace shooters_edge::net
