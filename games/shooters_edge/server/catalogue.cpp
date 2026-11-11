// The builtin catalogue (catalogue.hpp): the table and its installation.
//
// ge:summary The builtin catalogue's table, the extension registry filled from it
// ge:keywords kcatalogue install_catalogue offers_extension extension-registry
// ge:skill progs shooter
// ge:test tests/shooters_edge_qc_builtins_test.cpp
#include "shooters_edge/server/catalogue.hpp"

#include <cstring>
#include <string_view>

#include "progs/stdlib.hpp"
#include "shooters_edge/server/builtins.hpp"

namespace shooters_edge::server {

const CatalogueRow kCatalogue[] = {
    {1, "makevectors", bi_makevectors, ""},
    {2, "setorigin", sim::qc_setorigin, ""},
    {3, "setmodel", bi_setmodel, ""},
    {4, "setsize", bi_setsize, ""},
    {6, "break", bi_break, ""},
    {7, "random", nullptr, ""},
    {8, "sound", bi_sound, ""},
    {9, "normalize", nullptr, ""},
    {10, "error", bi_error, ""},
    {11, "objerror", bi_objerror, ""},
    {12, "vlen", nullptr, ""},
    {13, "vectoyaw", nullptr, ""},
    {14, "spawn", sim::qc_spawn, ""},
    {15, "remove", sim::qc_remove, ""},
    {16, "traceline", bi_traceline, ""},
    {17, "checkclient", bi_checkclient, ""},
    {18, "find", bi_find, ""},
    {19, "precache_sound", bi_precache_sound, ""},
    {20, "precache_model", bi_precache_model, ""},
    {21, "stuffcmd", bi_stuffcmd, ""},
    {22, "findradius", bi_findradius, ""},
    {23, "bprint", bi_bprint, ""},
    {24, "sprint", bi_sprint, ""},
    {25, "dprint", bi_dprint, ""},
    {26, "ftos", nullptr, ""},
    {27, "vtos", nullptr, ""},
    {28, "coredump", bi_coredump, ""},
    {29, "traceon", bi_traceon, ""},
    {30, "traceoff", bi_traceoff, ""},
    {31, "eprint", bi_eprint, ""},
    {32, "walkmove", bi_walkmove, ""},
    {34, "droptofloor", bi_droptofloor, ""},
    {35, "lightstyle", bi_lightstyle, ""},
    {36, "rint", nullptr, ""},
    {37, "floor", nullptr, ""},
    {38, "ceil", nullptr, ""},
    {40, "checkbottom", bi_checkbottom, ""},
    {41, "pointcontents", bi_pointcontents, ""},
    {43, "fabs", nullptr, ""},
    {44, "aim", bi_aim, ""},
    {45, "cvar", bi_cvar, ""},
    {46, "localcmd", bi_localcmd, ""},
    {47, "nextent", bi_nextent, ""},
    {48, "particle", bi_particle, ""},
    {49, "changeyaw", bi_changeyaw, ""},
    {51, "vectoangles", nullptr, ""},
    {52, "WriteByte", bi_writebyte, ""},
    {53, "WriteChar", bi_writechar, ""},
    {54, "WriteShort", bi_writeshort, ""},
    {55, "WriteLong", bi_writelong, ""},
    {56, "WriteCoord", bi_writecoord, ""},
    {57, "WriteAngle", bi_writeangle, ""},
    {58, "WriteString", bi_writestring, ""},
    {59, "WriteEntity", bi_writeentity, ""},
    {67, "movetogoal", bi_movetogoal, ""},
    {68, "precache_file", nullptr, ""},
    {69, "makestatic", bi_makestatic, ""},
    {70, "changelevel", bi_changelevel, ""},
    {72, "cvar_set", bi_cvar_set, ""},
    {73, "centerprint", bi_centerprint, ""},
    {74, "ambientsound", bi_ambientsound, ""},
    {75, "precache_model2", bi_precache_model, ""},
    {76, "precache_sound2", bi_precache_sound, ""},
    {77, "precache_file2", nullptr, ""},
    {78, "setspawnparms", bi_setspawnparms, ""},
    {79, "logfrag", bi_logfrag, ""},
    {80, "infokey", bi_infokey, ""},
    {81, "stof", nullptr, ""},
    {82, "multicast", bi_multicast, ""},
    {99, "checkextension", bi_checkextension, ""},
    {60, "sin", nullptr, "DP_QC_SINCOSSQRTPOW"},
    {61, "cos", nullptr, "DP_QC_SINCOSSQRTPOW"},
    {62, "sqrt", nullptr, "DP_QC_SINCOSSQRTPOW"},
    {97, "pow", nullptr, "DP_QC_SINCOSSQRTPOW"},
    {84, "tokenize", nullptr, "ZQ_QC_TOKENIZE"},
    {85, "argc", nullptr, "ZQ_QC_TOKENIZE"},
    {86, "argv", nullptr, "ZQ_QC_TOKENIZE"},
    {90, "tracebox", bi_tracebox, "DP_QC_TRACEBOX"},
    {91, "randomvec", nullptr, "DP_QC_RANDOMVEC"},
    {93, "registercvar", bi_registercvar, "DP_REGISTERCVAR"},
    {94, "min", nullptr, "DP_QC_MINMAXBOUND"},
    {95, "max", nullptr, "DP_QC_MINMAXBOUND"},
    {96, "bound", nullptr, "DP_QC_MINMAXBOUND"},
    {103, "cvar_string", bi_cvar_string, "QSG_CVARSTRING"},
    {448, "cvar_string", bi_cvar_string, "DP_QC_CVAR_STRING"},
    {114, "strlen", nullptr, "ZQ_QC_STRINGS"},
    {115, "strcat", nullptr, "ZQ_QC_STRINGS"},
    {116, "substr", nullptr, "ZQ_QC_STRINGS"},
    {118, "strzone", nullptr, "ZQ_QC_STRINGS"},
    {119, "strunzone", nullptr, "ZQ_QC_STRINGS"},
    {231, "calltimeofday", bi_calltimeofday, "FTE_CALLTIMEOFDAY"},
    {531, "setpause", bi_setpause, "ZQ_PAUSE"},
    {532, "precache_vwep_model", bi_precache_vwep_model, "ZQ_VWEP"},
    {335, "particleeffectnum", bi_particleeffectnum, "DP_SV_POINTPARTICLES"},
    {336, "trailparticles", bi_trailparticles, "DP_SV_POINTPARTICLES"},
    {337, "pointparticles", bi_pointparticles, "DP_SV_POINTPARTICLES"},
    {640, "words", bi_words, "GE_QC_WORDS"},
    {65, "etos", nullptr, "DP_QC_ETOS"},
    {98, "findfloat", bi_findfloat, "DP_QC_FINDFLOAT"},
    {432, "vectorvectors", bi_vectorvectors, "DP_QC_VECTORVECTORS"},
    {400, "copyentity", bi_copyentity, "DP_QC_COPYENTITY"},
    {402, "findchain", bi_findchain, "DP_QC_FINDCHAIN"},
    {403, "findchainfloat", bi_findchainfloat, "DP_QC_FINDCHAINFLOAT"},
    {471, "asin", nullptr, "DP_QC_ASINACOSATANATAN2TAN"},
    {472, "acos", nullptr, "DP_QC_ASINACOSATANATAN2TAN"},
    {473, "atan", nullptr, "DP_QC_ASINACOSATANATAN2TAN"},
    {474, "atan2", nullptr, "DP_QC_ASINACOSATANATAN2TAN"},
    {475, "tan", nullptr, "DP_QC_ASINACOSATANATAN2TAN"},
    {480, "strtolower", nullptr, "DP_QC_STRING_CASE_FUNCTIONS"},
    {481, "strtoupper", nullptr, "DP_QC_STRING_CASE_FUNCTIONS"},
    {484, "strreplace", nullptr, "DP_QC_STRREPLACE"},
    {485, "strireplace", nullptr, "DP_QC_STRREPLACE"},
    {117, "stov", nullptr, "FTE_STRINGS"},
    {221, "strstrofs", nullptr, "FTE_STRINGS"},
    {222, "str2chr", nullptr, "FTE_STRINGS"},
    {223, "chr2str", nullptr, "FTE_STRINGS"},
    {224, "strconv", nullptr, "FTE_STRINGS"},
    {225, "strpad", nullptr, "FTE_STRINGS"},
    {226, "infoadd", nullptr, "FTE_STRINGS"},
    {227, "infoget", nullptr, "FTE_STRINGS"},
    {228, "strncmp", nullptr, "FTE_STRINGS"},
    {229, "strcasecmp", nullptr, "FTE_STRINGS"},
    {230, "strncasecmp", nullptr, "FTE_STRINGS"},
    {627, "sprintf", nullptr, "DP_QC_SPRINTF"},
};
const int kCatalogueCount = static_cast<int>(sizeof kCatalogue / sizeof kCatalogue[0]);

const char* const kExtensionNames[] = {
    "DP_CON_SET",
    "DP_ENT_ALPHA",
    "DP_ENT_COLORMOD",
    "DP_ENT_SCALE",
    "DP_QC_ASINACOSATANATAN2TAN",
    "DP_QC_COPYENTITY",
    "DP_QC_CVAR_STRING",
    "DP_QC_ETOS",
    "DP_QC_FINDCHAIN",
    "DP_QC_FINDCHAINFLOAT",
    "DP_QC_FINDFLOAT",
    "DP_QC_MINMAXBOUND",
    "DP_QC_RANDOMVEC",
    "DP_QC_SINCOSSQRTPOW",
    "DP_QC_SPRINTF",
    "DP_QC_STRING_CASE_FUNCTIONS",
    "DP_QC_STRREPLACE",
    "DP_QC_TRACEBOX",
    "DP_QC_VECTORVECTORS",
    "DP_REGISTERCVAR",
    "DP_SV_POINTPARTICLES",
    "FTE_CALLTIMEOFDAY",
    "FTE_STRINGS",
    "GE_QC_WORDS",
    "QSG_CVARSTRING",
    "ZQ_CLIENTCOMMAND",
    "ZQ_ITEMS2",
    "ZQ_MOVETYPE_NOCLIP",
    "ZQ_MOVETYPE_FLY",
    "ZQ_MOVETYPE_NONE",
    "ZQ_PAUSE",
    "ZQ_QC_STRINGS",
    "ZQ_QC_TOKENIZE",
    "ZQ_VWEP",
};
const int kExtensionCount = static_cast<int>(sizeof kExtensionNames / sizeof kExtensionNames[0]);

namespace {

pg::Builtin resolve(const CatalogueRow& r) {
  if (r.fn != nullptr) return r.fn;
  for (int i = 0; i < pg::kStdlibCount; i++)
    if (pg::kStdlib[i].number == r.number) return pg::kStdlib[i].fn;
  return nullptr;
}

// The fields and hooks an extension brings with it (beyond its builtins).
void extras_of(pg::Extension& e) {
  if (e.name == "DP_ENT_ALPHA") e.fields = {"alpha"};
  if (e.name == "DP_ENT_COLORMOD") e.fields = {"colormod"};
  if (e.name == "DP_ENT_SCALE") e.fields = {"scale"};
  if (e.name == "ZQ_CLIENTCOMMAND") e.hooks = {"GE_ClientCommand"};
  if (e.name == "ZQ_ITEMS2") e.fields = {"items2"};
  if (e.name == "ZQ_PAUSE") e.hooks = {"GE_ShouldPause", "GE_PausedTic"};
  if (e.name == "ZQ_VWEP") e.fields = {"vw_index"};
  if (e.name == "FTE_CALLTIMEOFDAY") e.hooks = {"timeofday"};
}

// The re-release's builtins that only report or draw for a debugger, or ask the player's
// preferences: nothing done, 0 returned (the classic behaviour each falls back to).
void bi_rerelease_nothing(pg::Vm& vm) { pg::return_float(vm, 0); }

// The re-release's numbers (its first form; the later one binds the same builtins by name).
struct RereleaseRow {
  int number;
  const char* name;
  pg::Builtin fn;
};
const RereleaseRow kRerelease[] = {
    {79, "finaleFinished", bi_rerelease_nothing},  // 0: the finale's text stays up
    {80, "localsound", bi_rerelease_nothing},
    {81, "draw_point", bi_rerelease_nothing},
    {82, "draw_line", bi_rerelease_nothing},
    {83, "draw_arrow", bi_rerelease_nothing},
    {84, "draw_ray", bi_rerelease_nothing},
    {85, "draw_circle", bi_rerelease_nothing},
    {86, "draw_bounds", bi_rerelease_nothing},
    {87, "draw_worldtext", bi_rerelease_nothing},
    {88, "draw_sphere", bi_rerelease_nothing},
    {89, "draw_cylinder", bi_rerelease_nothing},
    {90, "centerprint", bi_centerprint},  // the key and its arguments
    {91, "bprint", bi_bprint},
    {92, "sprint", bi_sprint},
    {kRereleaseOnly + 0, "CheckPlayerEXFlags", bi_rerelease_nothing},  // 0: always switch
    {kRereleaseOnly + 1, "walkpathtogoal", bi_rerelease_nothing},      // 0: movetogoal instead
    {kRereleaseOnly + 2, "bot_movetopoint", bi_rerelease_nothing},
    {kRereleaseOnly + 3, "bot_followentity", bi_rerelease_nothing},
};
const pg::NamedBuiltin kRereleaseNames[] = {
    {"ex_bprint", 23},
    {"ex_sprint", 24},
    {"ex_centerprint", 73},
    {"ex_finaleFinished", 79},
    {"ex_localsound", 80},
    {"ex_draw_point", 81},
    {"ex_draw_line", 82},
    {"ex_draw_arrow", 83},
    {"ex_draw_ray", 84},
    {"ex_draw_circle", 85},
    {"ex_draw_bounds", 86},
    {"ex_draw_worldtext", 87},
    {"ex_draw_sphere", 88},
    {"ex_draw_cylinder", 89},
    {"ex_CheckPlayerEXFlags", kRereleaseOnly + 0},
    {"ex_walkpathtogoal", kRereleaseOnly + 1},
    {"ex_bot_movetopoint", kRereleaseOnly + 2},
    {"ex_bot_followentity", kRereleaseOnly + 3},
};

}  // namespace

bool rerelease_program(const pg::Progs& p) {
  const std::string_view strings(p.strings.data(), p.strings.size());
  if (strings.find("$qc_") != std::string_view::npos) return true;
  for (const pg::Function& f : p.functions) {
    if (f.first_statement != 0 || f.name <= 0 || static_cast<size_t>(f.name) >= p.strings.size())
      continue;
    if (std::strncmp(p.strings.data() + f.name, "ex_", 3) == 0) return true;
  }
  return false;
}

void install_rerelease(QcServer& s) {
  for (const RereleaseRow& r : kRerelease) pg::set_builtin(s.q.vm, r.number, r.fn, r.name);
  pg::bind_by_name(s.q.vm, kRereleaseNames);
}

bool install_catalogue(QcServer& s, std::string& error) {
  for (int i = 0; i < kCatalogueCount; i++) {
    const CatalogueRow& r = kCatalogue[i];
    if (r.number >= static_cast<int>(s.q.vm.builtins.size())) {
      error = std::string("install_catalogue: builtin #") + std::to_string(r.number) + " (" +
              r.name + ") is past the VM's table of " + std::to_string(s.q.vm.builtins.size()) +
              "; raise sim::kQcBuiltins";
      return false;
    }
    if (r.extension[0] != 0) continue;
    // The multiplayer server's own numbers are absent from a single-player program's table.
    if (s.q.layout->single_player && r.number >= 79 && r.number <= 82) continue;
    pg::set_builtin(s.q.vm, r.number, resolve(r), r.name);
  }
  s.extensions = {};
  for (int k = 0; k < kExtensionCount; k++) {
    pg::Extension e;
    e.name = kExtensionNames[k];
    for (int i = 0; i < kCatalogueCount; i++)
      if (e.name == kCatalogue[i].extension)
        e.builtins.push_back({kCatalogue[i].number, kCatalogue[i].name, resolve(kCatalogue[i])});
    extras_of(e);
    if (!pg::register_extension(s.extensions, std::move(e), error)) return false;
  }
  pg::install(s.extensions, s.q.vm);
  if (rerelease_program(*s.q.vm.progs)) install_rerelease(s);
  return true;
}

bool offers_extension(const QcServer& s, const char* name) {
  for (const pg::Extension& e : s.extensions.extensions) {
    const char* a = e.name.c_str();
    const char* b = name;
    for (; *a != 0 && *b != 0; a++, b++) {
      const char x = *a >= 'a' && *a <= 'z' ? static_cast<char>(*a - 32) : *a;
      const char y = *b >= 'a' && *b <= 'z' ? static_cast<char>(*b - 32) : *b;
      if (x != y) break;
    }
    if (*a == 0 && *b == 0) return true;
  }
  return false;
}

}  // namespace shooters_edge::server
