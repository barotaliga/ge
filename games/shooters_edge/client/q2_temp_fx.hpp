// QUAKE II'S TEMPORARY ENTITIES AS THE CLIENT SHOWS AND PLAYS THEM: the specks a hit throws (the
// puff: how many and from which palette run) and the sound the client itself plays for one, by
// the protocol's kind byte or by our rules' effect name (server/q2_wire.cpp's table). The server
// sends no sound for these: a recording or a joined server plays them here (demo_q2.cpp), and our
// own rules' effects, which carry their sounds as sound events, take the specks alone (listen.cpp).
//
// The puff: `count` specks of palette `colour` + 0..7, each 0..31 units out along the face's normal
// and -4..3 across, drifting at up to 20 units/s and falling, living 0.5..0.8 s
// (app/effects.hpp's `q2_specks`, the splash's routine). Facts:
// porting/shooters-edge/q2-client-te.md.
//
// ge:summary Quake II's temporary entities' puffs and sounds by kind byte or effect name
// ge:keywords q2 temp-entity temporary-entity puff specks gunshot blood sparks ricochet
// ge:keywords explosion-sound splash-sparks spark5 spark7
// ge:keywords q2_puff q2_temp_sound te_gunshot te_blood te_sparks rocklx1a grenlx1a xpld_wat ric1
// ge:keywords disrupthit lashit q2_pack_te q2_pack_te_code te_flechette te_blaster2
// ge:keywords te_electric_sparks te_heatbeam_sparks te_heatbeam_steam te_moreblood
// ge:skill shooter render
// ge:test tests/shooters_edge_q2_wire_test.cpp
#pragma once

#include <cstdint>
#include <string_view>

namespace shooters_edge::client {

struct Q2Puff {
  int code;               // the protocol's kind byte
  std::string_view name;  // our rules' effect name
  int count;              // specks
  int colour;             // the palette run's first (each speck + 0..7)
};

inline constexpr Q2Puff kQ2Puffs[] = {
    {0, "te_gunshot", 40, 0x00},        {1, "te_blood", 60, 0xe8},
    {4, "te_shotgun", 20, 0x00},        {9, "te_sparks", 6, 0xe0},
    {12, "te_screen_sparks", 40, 0xd0}, {13, "te_shield_sparks", 40, 0xb0},
    {14, "te_bullet_sparks", 6, 0xe0},  {26, "te_greenblood", 30, 0xdf},  // a pack's gekk
};

// The puff of kind byte `code`, or null.
constexpr const Q2Puff* q2_puff(int code) {
  for (const Q2Puff& p : kQ2Puffs)
    if (p.code == code) return &p;
  return nullptr;
}
// The puff of our rules' effect `name`, or null.
constexpr const Q2Puff* q2_puff(std::string_view name) {
  for (const Q2Puff& p : kQ2Puffs)
    if (p.name == name) return &p;
  return nullptr;
}

// What the client plays for a temporary entity: the sound (static text, null for none), at full
// volume, the protocol's attenuation factor (1 normal, 0 heard everywhere), at the line's end
// (the rail's) or at its point, and whether it plays only when no other such sound is playing
// (a power screen's and a power shield's sparks share one).
struct Q2TempSound {
  const char* name = nullptr;
  float attenuation = 1;
  bool at_end = false;
  bool alone = false;
};

// The sound of kind byte `code`; `pick` (any number) picks a ricochet: 1 in 16 each of three,
// silent otherwise. A splash (10) of `splash` kind 1 (sparks; 7, the extended protocol's electric
// one, too) crackles: spark5 or spark6 1 in 4 each, spark7 2 in 4, dying off fast (3); any other
// splash is silent.
constexpr Q2TempSound q2_temp_sound(int code, uint32_t pick, int splash = -1) {
  switch (code) {
    case 10:
      if (splash != 1 && splash != 7) return {};
      switch (pick % 4) {
        case 0:
          return {"world/spark5.wav", 3};
        case 1:
          return {"world/spark6.wav", 3};
        default:
          return {"world/spark7.wav", 3};
      }
    case 0:   // a gunshot's hit
    case 14:  // a bullet's sparks
      switch (pick % 16) {
        case 0:
          return {"world/ric1.wav"};
        case 1:
          return {"world/ric2.wav"};
        case 2:
          return {"world/ric3.wav"};
        default:
          return {};
      }
    case 2:   // a blaster bolt's hit
    case 30:  // Ground Zero's: a green blaster's, the heat beam's sparks and steam, electric
    case 43:  // sparks, a flechette's
    case 44:
    case 46:
    case 55:
      return {"weapons/lashit.wav"};
    case 47:  // the disruptor's burst
      return {"weapons/disrupthit.wav"};
    case 53:  // the big explosion, the one with no specks (the plain one is silent)
    case 54:
      return {"weapons/rocklx1a.wav"};
    case 3:  // the rail, heard where it ends
      return {"weapons/railgf1a.wav", 1, true};
    case 5:  // explosion1, a rocket's
    case 7:
    case 28:  // The Reckoning's plasma explosion: explosion1's, heard as it
      return {"weapons/rocklx1a.wav"};
    case 6:  // explosion2, a grenade's
    case 8:
      return {"weapons/grenlx1a.wav"};
    case 17:  // either in water
    case 18:
      return {"weapons/xpld_wat.wav"};
    case 12:  // a power screen's or shield's sparks: one at a time
    case 13:
      return {"weapons/lashit.wav", 1, false, true};
    case 22:  // a boss's teleport, heard everywhere
      return {"misc/bigtele.wav", 0};
    default:
      return {};
  }
}

// GROUND ZERO'S POINT EFFECTS by kind byte and by our rules' name (server/q2_wire.cpp's): a green
// blaster's hit (30), more blood (42), the heat beam's sparks (43) and steam (44), electric sparks
// (46), a flechette's hit (55). A recording names them so (demo_q2.cpp) and the listen server's
// come so, and both draw by the name (world_fx_q2.cpp's q2_named_fx).
struct Q2PackTe {
  int code;
  std::string_view name;
};
inline constexpr Q2PackTe kQ2PackTes[] = {
    {30, "te_blaster2"},       {42, "te_moreblood"},       {43, "te_heatbeam_sparks"},
    {44, "te_heatbeam_steam"}, {46, "te_electric_sparks"}, {55, "te_flechette"},
};
constexpr std::string_view q2_pack_te(int code) {
  for (const Q2PackTe& t : kQ2PackTes)
    if (t.code == code) return t.name;
  return {};
}
constexpr int q2_pack_te_code(std::string_view name) {
  for (const Q2PackTe& t : kQ2PackTes)
    if (t.name == name) return t.code;
  return 0;
}

}  // namespace shooters_edge::client
