// The program forms' tables (layout.hpp): names and types in order, the offsets counted and
// resolved by name at compile time, the multiplayer form held to the header's constants by
// static_assert.
//
// ge:summary The two forms' layout tables, offsets counted and resolved at compile time
// ge:keywords qc progs layout tables engine-side globals fields offsets static-assert
// ge:keywords single-player multiplayer resolve
// ge:skill progs
// ge:test tests/progs_file_test.cpp
#include "progs/layout.hpp"

#include <array>
#include <cstddef>

namespace ge::progs {

namespace {

struct Named {
  const char* name;
  uint16_t type;
};

constexpr uint16_t S = kTypeString, F = kTypeFloat, V = kTypeVector, E = kTypeEntity;
constexpr uint16_t Fn = kTypeFunction;

constexpr Named kMpGlobalNames[] = {
    {"self", E},
    {"other", E},
    {"world", E},
    {"time", F},
    {"frametime", F},
    {"newmis", E},
    {"force_retouch", F},
    {"mapname", S},
    {"serverflags", F},
    {"total_secrets", F},
    {"total_monsters", F},
    {"found_secrets", F},
    {"killed_monsters", F},
    {"parm1", F},
    {"parm2", F},
    {"parm3", F},
    {"parm4", F},
    {"parm5", F},
    {"parm6", F},
    {"parm7", F},
    {"parm8", F},
    {"parm9", F},
    {"parm10", F},
    {"parm11", F},
    {"parm12", F},
    {"parm13", F},
    {"parm14", F},
    {"parm15", F},
    {"parm16", F},
    {"v_forward", V},
    {"v_up", V},
    {"v_right", V},
    {"trace_allsolid", F},
    {"trace_startsolid", F},
    {"trace_fraction", F},
    {"trace_endpos", V},
    {"trace_plane_normal", V},
    {"trace_plane_dist", F},
    {"trace_ent", E},
    {"trace_inopen", F},
    {"trace_inwater", F},
    {"msg_entity", E},
    {"main", Fn},
    {"StartFrame", Fn},
    {"PlayerPreThink", Fn},
    {"PlayerPostThink", Fn},
    {"ClientKill", Fn},
    {"ClientConnect", Fn},
    {"PutClientInServer", Fn},
    {"ClientDisconnect", Fn},
    {"SetNewParms", Fn},
    {"SetChangeParms", Fn},
};

constexpr Named kMpFieldNames[] = {
    {"modelindex", F},  {"absmin", V},        {"absmax", V},       {"ltime", F},
    {"lastruntime", F}, {"movetype", F},      {"solid", F},        {"origin", V},
    {"oldorigin", V},   {"velocity", V},      {"angles", V},       {"avelocity", V},
    {"classname", S},   {"model", S},         {"frame", F},        {"skin", F},
    {"effects", F},     {"mins", V},          {"maxs", V},         {"size", V},
    {"touch", Fn},      {"use", Fn},          {"think", Fn},       {"blocked", Fn},
    {"nextthink", F},   {"groundentity", E},  {"health", F},       {"frags", F},
    {"weapon", F},      {"weaponmodel", S},   {"weaponframe", F},  {"currentammo", F},
    {"ammo_shells", F}, {"ammo_nails", F},    {"ammo_rockets", F}, {"ammo_cells", F},
    {"items", F},       {"takedamage", F},    {"chain", E},        {"deadflag", F},
    {"view_ofs", V},    {"button0", F},       {"button1", F},      {"button2", F},
    {"impulse", F},     {"fixangle", F},      {"v_angle", V},      {"netname", S},
    {"enemy", E},       {"flags", F},         {"colormap", F},     {"team", F},
    {"max_health", F},  {"teleport_time", F}, {"armortype", F},    {"armorvalue", F},
    {"waterlevel", F},  {"watertype", F},     {"ideal_yaw", F},    {"yaw_speed", F},
    {"aiment", E},      {"goalentity", E},    {"spawnflags", F},   {"target", S},
    {"targetname", S},  {"dmg_take", F},      {"dmg_save", F},     {"dmg_inflictor", E},
    {"owner", E},       {"movedir", V},       {"message", S},      {"sounds", F},
    {"noise", S},       {"noise1", S},        {"noise2", S},       {"noise3", S},
};

// The single-player form: no newmis; deathmatch, coop and teamplay after mapname.
constexpr Named kSpGlobalNames[] = {
    {"self", E},
    {"other", E},
    {"world", E},
    {"time", F},
    {"frametime", F},
    {"force_retouch", F},
    {"mapname", S},
    {"deathmatch", F},
    {"coop", F},
    {"teamplay", F},
    {"serverflags", F},
    {"total_secrets", F},
    {"total_monsters", F},
    {"found_secrets", F},
    {"killed_monsters", F},
    {"parm1", F},
    {"parm2", F},
    {"parm3", F},
    {"parm4", F},
    {"parm5", F},
    {"parm6", F},
    {"parm7", F},
    {"parm8", F},
    {"parm9", F},
    {"parm10", F},
    {"parm11", F},
    {"parm12", F},
    {"parm13", F},
    {"parm14", F},
    {"parm15", F},
    {"parm16", F},
    {"v_forward", V},
    {"v_up", V},
    {"v_right", V},
    {"trace_allsolid", F},
    {"trace_startsolid", F},
    {"trace_fraction", F},
    {"trace_endpos", V},
    {"trace_plane_normal", V},
    {"trace_plane_dist", F},
    {"trace_ent", E},
    {"trace_inopen", F},
    {"trace_inwater", F},
    {"msg_entity", E},
    {"main", Fn},
    {"StartFrame", Fn},
    {"PlayerPreThink", Fn},
    {"PlayerPostThink", Fn},
    {"ClientKill", Fn},
    {"ClientConnect", Fn},
    {"PutClientInServer", Fn},
    {"ClientDisconnect", Fn},
    {"SetNewParms", Fn},
    {"SetChangeParms", Fn},
};

// The single-player form: no lastruntime; punchangle after avelocity, idealpitch after v_angle.
constexpr Named kSpFieldNames[] = {
    {"modelindex", F},    {"absmin", V},       {"absmax", V},        {"ltime", F},
    {"movetype", F},      {"solid", F},        {"origin", V},        {"oldorigin", V},
    {"velocity", V},      {"angles", V},       {"avelocity", V},     {"punchangle", V},
    {"classname", S},     {"model", S},        {"frame", F},         {"skin", F},
    {"effects", F},       {"mins", V},         {"maxs", V},          {"size", V},
    {"touch", Fn},        {"use", Fn},         {"think", Fn},        {"blocked", Fn},
    {"nextthink", F},     {"groundentity", E}, {"health", F},        {"frags", F},
    {"weapon", F},        {"weaponmodel", S},  {"weaponframe", F},   {"currentammo", F},
    {"ammo_shells", F},   {"ammo_nails", F},   {"ammo_rockets", F},  {"ammo_cells", F},
    {"items", F},         {"takedamage", F},   {"chain", E},         {"deadflag", F},
    {"view_ofs", V},      {"button0", F},      {"button1", F},       {"button2", F},
    {"impulse", F},       {"fixangle", F},     {"v_angle", V},       {"idealpitch", F},
    {"netname", S},       {"enemy", E},        {"flags", F},         {"colormap", F},
    {"team", F},          {"max_health", F},   {"teleport_time", F}, {"armortype", F},
    {"armorvalue", F},    {"waterlevel", F},   {"watertype", F},     {"ideal_yaw", F},
    {"yaw_speed", F},     {"aiment", E},       {"goalentity", E},    {"spawnflags", F},
    {"target", S},        {"targetname", S},   {"dmg_take", F},      {"dmg_save", F},
    {"dmg_inflictor", E}, {"owner", E},        {"movedir", V},       {"message", S},
    {"sounds", F},        {"noise", S},        {"noise1", S},        {"noise2", S},
    {"noise3", S},
};

template <std::size_t N>
constexpr std::array<LayoutEntry, N> counted(const Named (&names)[N], int first) {
  std::array<LayoutEntry, N> out{};
  int at = first;
  for (std::size_t i = 0; i < N; i++) {
    out[i] = {names[i].name, names[i].type, at};
    at += names[i].type == kTypeVector ? 3 : 1;
  }
  return out;
}

constexpr auto kGlobals = counted(kMpGlobalNames, kReservedGlobals);
constexpr auto kFields = counted(kMpFieldNames, 0);
constexpr auto kSpGlobals = counted(kSpGlobalNames, kReservedGlobals);
constexpr auto kSpFields = counted(kSpFieldNames, 0);

constexpr int end_of(const LayoutEntry& e) { return e.offset + (e.type == kTypeVector ? 3 : 1); }

constexpr bool streq(const char* a, const char* b) {
  while (*a != 0 && *a == *b) {
    a++;
    b++;
  }
  return *a == *b;
}
template <std::size_t N>
constexpr int offset_of(const std::array<LayoutEntry, N>& table, const char* name) {
  for (const LayoutEntry& e : table) {
    if (streq(e.name, name)) return e.offset;
  }
  return -1;
}
constexpr int global_at(const char* name) { return offset_of(kGlobals, name); }
constexpr int field_at(const char* name) { return offset_of(kFields, name); }

static_assert(end_of(kGlobals.back()) == kEngineGlobalsEnd);
static_assert(end_of(kFields.back()) == kEngineFields);
static_assert(global_at("self") == kGSelf && global_at("time") == kGTime);
static_assert(global_at("newmis") == kGNewmis && global_at("force_retouch") == kGForceRetouch);
static_assert(global_at("parm1") == kGParm1 && global_at("v_forward") == kGVForward);
static_assert(global_at("v_up") == kGVUp && global_at("v_right") == kGVRight);
static_assert(global_at("trace_endpos") == kGTraceEndpos && global_at("trace_ent") == kGTraceEnt);
static_assert(global_at("msg_entity") == kGMsgEntity && global_at("main") == kGMain);
static_assert(global_at("StartFrame") == kGStartFrame);
static_assert(global_at("SetChangeParms") == kGSetChangeParms);
static_assert(field_at("origin") == kFOrigin && field_at("velocity") == kFVelocity);
static_assert(field_at("angles") == kFAngles && field_at("frame") == kFFrame);
static_assert(field_at("mins") == kFMins && field_at("size") == kFSize);
static_assert(field_at("think") == kFThink && field_at("nextthink") == kFNextthink);
static_assert(field_at("touch") == kFTouch && field_at("blocked") == kFBlocked);
static_assert(field_at("groundentity") == kFGroundentity && field_at("health") == kFHealth);
static_assert(field_at("button0") == kFButton0 && field_at("impulse") == kFImpulse);
static_assert(field_at("v_angle") == kFVAngle && field_at("flags") == kFFlags);
static_assert(field_at("teleport_time") == kFTeleportTime);
static_assert(field_at("waterlevel") == kFWaterlevel && field_at("watertype") == kFWatertype);
static_assert(field_at("spawnflags") == kFSpawnflags && field_at("owner") == kFOwner);
static_assert(field_at("takedamage") == kFTakedamage && field_at("colormap") == kFColormap);
static_assert(field_at("classname") == kFClassname && field_at("model") == kFModel);
static_assert(field_at("frags") == kFFrags && field_at("weaponmodel") == kFWeaponmodel);
static_assert(field_at("items") == kFItems && field_at("chain") == kFChain);
static_assert(field_at("view_ofs") == kFViewOfs && field_at("netname") == kFNetname);
static_assert(field_at("enemy") == kFEnemy && field_at("team") == kFTeam);
static_assert(field_at("ideal_yaw") == kFIdealYaw && field_at("yaw_speed") == kFYawSpeed);
static_assert(field_at("goalentity") == kFGoalentity);

template <std::size_t N>
constexpr LayoutGlobals resolve_globals(const std::array<LayoutEntry, N>& table) {
  LayoutGlobals g;
#define GE_PROGS_X(member, name) g.member = offset_of(table, name);
  GE_PROGS_LAYOUT_GLOBALS(GE_PROGS_X)
#undef GE_PROGS_X
  return g;
}
template <std::size_t N>
constexpr LayoutFields resolve_fields(const std::array<LayoutEntry, N>& table) {
  LayoutFields f;
#define GE_PROGS_X(member) f.member = offset_of(table, #member);
  GE_PROGS_LAYOUT_FIELDS(GE_PROGS_X)
#undef GE_PROGS_X
  return f;
}

// Every entry of a table has a member in LayoutGlobals or LayoutFields (parm2..16 ride on parm1).
template <std::size_t N>
constexpr bool all_named_globals(const std::array<LayoutEntry, N>& table) {
  for (const LayoutEntry& e : table) {
    bool named = e.name[0] == 'p' && e.name[1] == 'a' && e.name[2] == 'r' && e.name[3] == 'm';
#define GE_PROGS_X(m, qc) named = named || streq(e.name, qc);
    GE_PROGS_LAYOUT_GLOBALS(GE_PROGS_X)
#undef GE_PROGS_X
    if (!named) return false;
  }
  return true;
}
template <std::size_t N>
constexpr bool all_named_fields(const std::array<LayoutEntry, N>& table) {
  for (const LayoutEntry& e : table) {
    bool named = false;
#define GE_PROGS_X(m) named = named || streq(e.name, #m);
    GE_PROGS_LAYOUT_FIELDS(GE_PROGS_X)
#undef GE_PROGS_X
    if (!named) return false;
  }
  return true;
}
static_assert(all_named_globals(kGlobals) && all_named_globals(kSpGlobals));
static_assert(all_named_fields(kFields) && all_named_fields(kSpFields));

constexpr int kSpGlobalsEnd = end_of(kSpGlobals.back());
constexpr int kSpFieldsEnd = end_of(kSpFields.back());
static_assert(kSpGlobalsEnd == 92 && kSpFieldsEnd == 105);
static_assert(offset_of(kSpGlobals, "deathmatch") == 35 && offset_of(kSpGlobals, "parm1") == 43);
static_assert(offset_of(kSpGlobals, "SetChangeParms") == 91);
static_assert(offset_of(kSpFields, "punchangle") == 25 && offset_of(kSpFields, "classname") == 28);
static_assert(offset_of(kSpFields, "idealpitch") == 73 && offset_of(kSpFields, "noise3") == 104);

}  // namespace

const char* const kOptionalFields[10] = {"items2",      "maxspeed",   "gravity",  "movement",
                                         "vw_index",    "hideentity", "trackent", "visclients",
                                         "hideplayers", "teleported"};
const char* const kOptionalHooks[12] = {
    "SpectatorConnect", "SpectatorThink",   "SpectatorDisconnect",
    "ChatMessage",      "UserInfo_Changed", "ConsoleCmd",
    "UserCmd",          "localinfoChanged", "GE_ClientCommand",
    "GE_PausedTic",     "GE_ShouldPause",   "timeofday"};

const char* type_name(uint16_t type) {
  switch (type & kTypeMask) {
    case kTypeVoid:
      return "void";
    case kTypeString:
      return "string";
    case kTypeFloat:
      return "float";
    case kTypeVector:
      return "vector";
    case kTypeEntity:
      return "entity";
    case kTypeField:
      return "field";
    case kTypeFunction:
      return "function";
    case kTypePointer:
      return "pointer";
    default:
      return "bad";
  }
}

constexpr Layout kMultiplayerLayout = {
    "multiplayer",
    kClassicCrc,
    kGlobals.data(),
    static_cast<int>(kGlobals.size()),
    kFields.data(),
    static_cast<int>(kFields.size()),
    kEngineGlobalsEnd,
    kEngineFields,
    resolve_globals(kGlobals),
    resolve_fields(kFields),
    false,
};
constexpr Layout kSinglePlayerLayout = {
    "single-player",
    kSinglePlayerCrc,
    kSpGlobals.data(),
    static_cast<int>(kSpGlobals.size()),
    kSpFields.data(),
    static_cast<int>(kSpFields.size()),
    kSpGlobalsEnd,
    kSpFieldsEnd,
    resolve_globals(kSpGlobals),
    resolve_fields(kSpFields),
    true,
};
const Layout* const kLayouts[kLayoutForms] = {&kMultiplayerLayout, &kSinglePlayerLayout};

const Layout* layout_for_crc(int32_t crc) {
  for (const Layout* l : kLayouts) {
    if (l->crc == crc) return l;
  }
  return nullptr;
}
const LayoutEntry* layout_global(const Layout& layout, const char* name) {
  for (int i = 0; i < layout.num_globals; i++) {
    if (streq(layout.globals[i].name, name)) return &layout.globals[i];
  }
  return nullptr;
}
const LayoutEntry* layout_field(const Layout& layout, const char* name) {
  for (int i = 0; i < layout.num_fields; i++) {
    if (streq(layout.fields[i].name, name)) return &layout.fields[i];
  }
  return nullptr;
}

std::string layout_declarations(const Layout& layout) {
  auto type = [](uint16_t t) -> const char* {
    switch (t) {
      case kTypeString:
        return "string";
      case kTypeVector:
        return "vector";
      case kTypeEntity:
        return "entity";
      case kTypeFunction:
        return "void()";
      default:
        return "float";
    }
  };
  std::string out;
  for (int i = 0; i < layout.num_globals; i++)
    out += std::string(type(layout.globals[i].type)) + " " + layout.globals[i].name + ";\n";
  for (int i = 0; i < layout.num_fields; i++)
    out += std::string(".") + type(layout.fields[i].type) + " " + layout.fields[i].name + ";\n";
  return out;
}

}  // namespace ge::progs
