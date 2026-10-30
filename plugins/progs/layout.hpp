// THE ENGINE-SIDE LAYOUT of the classic multiplayer progs (the layout whose CRC is 54730): the
// globals and entity fields a program declares FIRST, in a fixed order, so the engine reads and
// writes them by offset. The globals sit at word offsets 28..89 (after the reserved 0..27: the
// null slot, the return value and the eight parameter slots); the fields start at word 0 of an
// entity, 102 words in all. A program's own globals and fields follow them.
//
// The tables below are the order and the types; the offsets are counted from them (a vector three
// words), never typed in. The checker holds a program's definitions to them where it names one
// (file.hpp), the assembler lays them out (assembler.hpp), and a game reads them by the constants.
// The optional fields and hooks a game finds BY NAME after loading are listed too; what each does
// is the game's.
//
// ge:summary The classic multiplayer engine-side layout: globals 28..89 and fields, by offset
// ge:keywords qc progs layout crc engine-side globals fields self other time frametime newmis
// ge:keywords force_retouch parm1 v_forward trace_fraction start_frame player_pre_think
// ge:keywords origin velocity nextthink think touch frame optional-fields optional-hooks
// ge:keywords classic_layout layout_field layout_global
// ge:skill progs
// ge:test tests/progs_file_test.cpp
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

constexpr int32_t kClassicCrc = 54730;

// The reserved globals.
constexpr int kGlobalNull = 0;
constexpr int kGlobalReturn = 1;  // three words
constexpr int kGlobalParm0 = 4;   // parameter i at kGlobalParm0 + 3 * i, three words each
constexpr int kMaxParms = 8;
constexpr int kReservedGlobals = 28;

// The engine-side globals, by offset.
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

// One engine-side global or field: its name, its type and its offset in words.
struct LayoutEntry {
  const char* name;
  uint16_t type;
  int offset;
};

// The engine-side globals (offsets 28..89) and fields (offsets 0..101), in declaration order.
const LayoutEntry* layout_globals(int& count);
const LayoutEntry* layout_fields(int& count);
// An entry by name, or nullptr.
const LayoutEntry* layout_global(const char* name);
const LayoutEntry* layout_field(const char* name);

// The fields' offsets the loop and STATE use, counted from the table (layout.cpp holds them to it).
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

}  // namespace ge::progs
