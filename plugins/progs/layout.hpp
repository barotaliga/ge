// THE ENGINE-SIDE LAYOUTS, one per PROGRAM FORM: the globals and entity fields a program declares
// FIRST, in a fixed order, so the engine reads and writes them by offset. A form is named by the
// crc in a file's header: 54730 the classic MULTIPLAYER program, 5927 the classic SINGLE-PLAYER
// program. Both put their globals after the reserved 0..27 (the null slot, the return value and
// the eight parameter slots) and their fields from word 0 of an entity; a program's own globals
// and fields follow them.
//
// A form is a TABLE (`Layout`: the names and types in order, the offsets counted from them, never
// typed in) and the offsets resolved from it by name (`Layout::g`, `Layout::f`; -1 for a name the
// form does not have). The checker holds a program to the table its crc names (file.hpp), the
// assembler and the compiler lay out from a table, and a game reads and writes the engine's
// globals and fields through `Layout::g` and `Layout::f` of the program it runs, so a third form
// is a third table and nothing that reads one changes.
//
// The two classic forms differ only here: the single-player globals have no `newmis` and carry
// `deathmatch`, `coop` and `teamplay` after `mapname` (92 words in all against 90); its fields
// have no `lastruntime` and carry `punchangle` (a vector after `avelocity`) and `idealpitch`
// (after `v_angle`), 105 words against 102.
//
// The `kG*` and `kF*` constants are the MULTIPLAYER form's offsets, for code that builds or reads
// only multiplayer programs (the assembler's tests, the test mod); they are held to its table at
// compile time. The optional fields and hooks a game finds BY NAME after loading are listed too;
// what each does is the game's.
//
// ge:summary The engine-side layouts by program form (multiplayer 54730, single-player 5927)
// ge:keywords qc progs layout crc engine-side globals fields self other time frametime newmis
// ge:keywords force_retouch parm1 v_forward trace_fraction start_frame player_pre_think
// ge:keywords origin velocity nextthink think touch frame optional-fields optional-hooks
// ge:keywords classic_layout layout_field layout_global layout-table program-form single-player
// ge:keywords campaign multiplayer punchangle idealpitch deathmatch coop teamplay layout_for_crc
// ge:keywords multiplayer_layout single_player_layout kmultiplayerlayout ksingleplayerlayout
// ge:skill progs
// ge:test tests/progs_file_test.cpp tests/progs_layout_test.cpp
#pragma once

#include <cstdint>

namespace ge::progs {

// The type of a definition (the low 15 bits of its type word).
enum Type : uint16_t {
  kTypeVoid = 0,
  kTypeString = 1,
  kTypeFloat = 2,
  kTypeVector = 3,
  kTypeEntity = 4,
  kTypeField = 5,
  kTypeFunction = 6,
  kTypePointer = 7,
};
constexpr uint16_t kTypeSaved = 0x8000;  // bit 15: saved with a game (globals only)
constexpr uint16_t kTypeMask = 0x7fff;
inline int type_words(uint16_t type) { return (type & kTypeMask) == kTypeVector ? 3 : 1; }
const char* type_name(uint16_t type);

constexpr int32_t kClassicCrc = 54730;      // the multiplayer form
constexpr int32_t kSinglePlayerCrc = 5927;  // the single-player form

// The reserved globals.
constexpr int kGlobalNull = 0;
constexpr int kGlobalReturn = 1;  // three words
constexpr int kGlobalParm0 = 4;   // parameter i at kGlobalParm0 + 3 * i, three words each
constexpr int kMaxParms = 8;
constexpr int kReservedGlobals = 28;

// The MULTIPLAYER form's engine-side globals, by offset (game code reads `Layout::g`).
constexpr int kGSelf = 28, kGOther = 29, kGWorld = 30, kGTime = 31, kGFrametime = 32;
constexpr int kGNewmis = 33, kGForceRetouch = 34, kGMapname = 35, kGServerflags = 36;
constexpr int kGParm1 = 41;  // parm1..parm16 at 41..56
constexpr int kGVForward = 57, kGVUp = 60, kGVRight = 63;
constexpr int kGTraceAllsolid = 66, kGTraceStartsolid = 67, kGTraceFraction = 68;
constexpr int kGTraceEndpos = 69, kGTracePlaneNormal = 72, kGTracePlaneDist = 75;
constexpr int kGTraceEnt = 76, kGTraceInopen = 77, kGTraceInwater = 78, kGMsgEntity = 79;
constexpr int kGMain = 80, kGStartFrame = 81, kGPlayerPreThink = 82, kGPlayerPostThink = 83;
constexpr int kGClientKill = 84, kGClientConnect = 85, kGPutClientInServer = 86;
constexpr int kGClientDisconnect = 87, kGSetNewParms = 88, kGSetChangeParms = 89;
constexpr int kEngineGlobalsEnd = 90;  // the program's own globals usually start here

// The MULTIPLAYER form's field offsets, counted from its table (layout.cpp holds them to it; game
// code reads `Layout::f`).
constexpr int kFModelindex = 0, kFAbsmin = 1, kFAbsmax = 4, kFLtime = 7, kFMovetype = 9;
constexpr int kFSolid = 10, kFOrigin = 11, kFOldorigin = 14, kFVelocity = 17, kFAngles = 20;
constexpr int kFAvelocity = 23, kFClassname = 26, kFModel = 27, kFFrame = 28, kFSkin = 29;
constexpr int kFEffects = 30, kFMins = 31, kFMaxs = 34, kFSize = 37, kFTouch = 40, kFUse = 41;
constexpr int kFThink = 42, kFBlocked = 43, kFNextthink = 44, kFGroundentity = 45;
constexpr int kFHealth = 46, kFTakedamage = 57, kFButton0 = 63, kFButton1 = 64;
constexpr int kFButton2 = 65, kFImpulse = 66, kFFixangle = 67, kFVAngle = 68, kFFlags = 73;
constexpr int kFColormap = 74, kFTeleportTime = 77, kFWaterlevel = 80, kFWatertype = 81;
constexpr int kFSpawnflags = 86, kFOwner = 92;
constexpr int kFFrags = 47, kFWeaponmodel = 49, kFItems = 56, kFChain = 58, kFViewOfs = 60;
constexpr int kFNetname = 71, kFEnemy = 72, kFTeam = 75, kFIdealYaw = 82, kFYawSpeed = 83;
constexpr int kFGoalentity = 85;
constexpr int kEngineFields = 102;  // words

// The optional fields and hooks a game looks up by name after loading (absent: the feature is off).
extern const char* const kOptionalFields[10];
extern const char* const kOptionalHooks[12];

// One engine-side global or field: its name, its type and its offset in words.
struct LayoutEntry {
  const char* name;
  uint16_t type;
  int offset;
};

// clang-format off
// Every engine-side global any form has: the member, then the name a program declares.
#define GE_PROGS_LAYOUT_GLOBALS(X) \
  X(self, "self") \
  X(other, "other") \
  X(world, "world") \
  X(time, "time") \
  X(frametime, "frametime") \
  X(newmis, "newmis") \
  X(force_retouch, "force_retouch") \
  X(mapname, "mapname") \
  X(deathmatch, "deathmatch") \
  X(coop, "coop") \
  X(teamplay, "teamplay") \
  X(serverflags, "serverflags") \
  X(total_secrets, "total_secrets") \
  X(total_monsters, "total_monsters") \
  X(found_secrets, "found_secrets") \
  X(killed_monsters, "killed_monsters") \
  X(parm1, "parm1") \
  X(v_forward, "v_forward") \
  X(v_up, "v_up") \
  X(v_right, "v_right") \
  X(trace_allsolid, "trace_allsolid") \
  X(trace_startsolid, "trace_startsolid") \
  X(trace_fraction, "trace_fraction") \
  X(trace_endpos, "trace_endpos") \
  X(trace_plane_normal, "trace_plane_normal") \
  X(trace_plane_dist, "trace_plane_dist") \
  X(trace_ent, "trace_ent") \
  X(trace_inopen, "trace_inopen") \
  X(trace_inwater, "trace_inwater") \
  X(msg_entity, "msg_entity") \
  X(main, "main") \
  X(start_frame, "StartFrame") \
  X(player_pre_think, "PlayerPreThink") \
  X(player_post_think, "PlayerPostThink") \
  X(client_kill, "ClientKill") \
  X(client_connect, "ClientConnect") \
  X(put_client_in_server, "PutClientInServer") \
  X(client_disconnect, "ClientDisconnect") \
  X(set_new_parms, "SetNewParms") \
  X(set_change_parms, "SetChangeParms")

// Every engine-side field any form has (the member is the name a program declares).
#define GE_PROGS_LAYOUT_FIELDS(X) \
  X(modelindex) X(absmin) X(absmax) X(ltime) X(lastruntime) X(movetype) \
  X(solid) X(origin) X(oldorigin) X(velocity) X(angles) X(avelocity) \
  X(punchangle) X(classname) X(model) X(frame) X(skin) X(effects) \
  X(mins) X(maxs) X(size) X(touch) X(use) X(think) \
  X(blocked) X(nextthink) X(groundentity) X(health) X(frags) X(weapon) \
  X(weaponmodel) X(weaponframe) X(currentammo) X(ammo_shells) X(ammo_nails) X(ammo_rockets) \
  X(ammo_cells) X(items) X(takedamage) X(chain) X(deadflag) X(view_ofs) \
  X(button0) X(button1) X(button2) X(impulse) X(fixangle) X(v_angle) \
  X(idealpitch) X(netname) X(enemy) X(flags) X(colormap) X(team) \
  X(max_health) X(teleport_time) X(armortype) X(armorvalue) X(waterlevel) X(watertype) \
  X(ideal_yaw) X(yaw_speed) X(aiment) X(goalentity) X(spawnflags) X(target) \
  X(targetname) X(dmg_take) X(dmg_save) X(dmg_inflictor) X(owner) X(movedir) \
  X(message) X(sounds) X(noise) X(noise1) X(noise2) X(noise3)
// clang-format on

// A form's engine-side global offsets by name; -1 where the form has no such global.
struct LayoutGlobals {
#define GE_PROGS_X(member, name) int member = -1;
  GE_PROGS_LAYOUT_GLOBALS(GE_PROGS_X)
#undef GE_PROGS_X
};
// A form's engine-side field offsets by name; -1 where the form has no such field.
struct LayoutFields {
#define GE_PROGS_X(member) int member = -1;
  GE_PROGS_LAYOUT_FIELDS(GE_PROGS_X)
#undef GE_PROGS_X
};

// One program form: its name, its crc, its tables in declaration order and the offsets read off
// them.
struct Layout {
  const char* name;  // "multiplayer", "single-player"
  int32_t crc;
  const LayoutEntry* globals;
  int num_globals;
  const LayoutEntry* fields;
  int num_fields;
  int globals_end;  // the first word past the engine-side globals
  int fields_end;   // the engine-side fields' words
  LayoutGlobals g;
  LayoutFields f;
  bool single_player;  // the campaign host's form (spawn parms carried, autoaim, no multicast)
};

extern const Layout kMultiplayerLayout;
extern const Layout kSinglePlayerLayout;
constexpr int kLayoutForms = 2;
extern const Layout* const kLayouts[kLayoutForms];

// The form whose crc is `crc`, or nullptr.
const Layout* layout_for_crc(int32_t crc);
// A form's entry by name, or nullptr.
const LayoutEntry* layout_global(const Layout& layout, const char* name);
const LayoutEntry* layout_field(const Layout& layout, const char* name);

}  // namespace ge::progs
