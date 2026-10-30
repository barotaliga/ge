// The classic protocol's extension table (classic_ext.hpp).
//
// ge:summary The extension table's rows, the first tier, agreement and the coordinates it makes
// ge:keywords kextbits first_tier agree coords_of describe_ext family_magic
// ge:skill net shooter
// ge:test tests/shooters_edge_classic_wire_test.cpp
#include "shooters_edge/net/classic_ext.hpp"

namespace shooters_edge::net {

const ExtBit kExtBits[] = {
    {ExtFamily::x, true, kXTrans, "transparency"},
    {ExtFamily::x, true, kXSoundDbl, "sound-double"},
    {ExtFamily::x, true, kXModelDbl, "model-double"},
    {ExtFamily::x, true, kXEntityDbl, "entity-double"},
    {ExtFamily::x, true, kXEntityDbl2, "entity-double2"},
    {ExtFamily::x, false, kXFloatCoords, "float-coords"},
    {ExtFamily::x, false, kXColourMod, "colour-mod"},
    {ExtFamily::x, true, kXSpawnStatic2, "spawnstatic2"},
    {ExtFamily::x, true, kX256Entities, "256-entities"},
    {ExtFamily::x, true, kXChunkedDl, "chunked-downloads"},
    {ExtFamily::x, false, kXClientProgs, "client-programs"},
    {ExtFamily::x2, false, kX2Voice, "voice"},
    {ExtFamily::mvd, true, kMvdFloatCoords, "origin-floats"},
    {ExtFamily::mvd, true, kMvdTeleport, "teleport-kind"},
    {ExtFamily::z, true, kZPmType, "pm-type"},
    {ExtFamily::z, true, kZPmTypeNew, "pm-type-new"},
    {ExtFamily::z, true, kZViewHeight, "view-height"},
    {ExtFamily::z, true, kZServerTime, "server-time"},
    {ExtFamily::z, true, kZPitchLimits, "pitch-limits"},
    {ExtFamily::z, true, kZJoinObserve, "join-observe"},
    {ExtFamily::z, true, kZOnGround, "on-ground"},
    {ExtFamily::z, true, kZVwep, "view-weapons"},
    {ExtFamily::z, true, kZSolid, "solid"},
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
