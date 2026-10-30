// The builtin catalogue (catalogue.hpp): the table and its installation.
//
// ge:summary The builtin catalogue's table, the extension registry filled from it
// ge:keywords kcatalogue install_catalogue offers_extension extension-registry
// ge:skill progs shooter
// ge:test tests/shooters_edge_qc_builtins_test.cpp
#include "shooters_edge/server/catalogue.hpp"

#include <cstring>

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
};
const int kCatalogueCount = static_cast<int>(sizeof kCatalogue / sizeof kCatalogue[0]);

const char* const kExtensionNames[] = {
    "DP_CON_SET",          "DP_QC_CVAR_STRING", "DP_QC_MINMAXBOUND", "DP_QC_RANDOMVEC",
    "DP_QC_SINCOSSQRTPOW", "DP_QC_TRACEBOX",    "DP_REGISTERCVAR",   "FTE_CALLTIMEOFDAY",
    "QSG_CVARSTRING",      "ZQ_CLIENTCOMMAND",  "ZQ_ITEMS2",         "ZQ_MOVETYPE_NOCLIP",
    "ZQ_MOVETYPE_FLY",     "ZQ_MOVETYPE_NONE",  "ZQ_PAUSE",          "ZQ_QC_STRINGS",
    "ZQ_QC_TOKENIZE",      "ZQ_VWEP",
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
  if (e.name == "ZQ_CLIENTCOMMAND") e.hooks = {"GE_ClientCommand"};
  if (e.name == "ZQ_ITEMS2") e.fields = {"items2"};
  if (e.name == "ZQ_PAUSE") e.hooks = {"GE_ShouldPause", "GE_PausedTic"};
  if (e.name == "ZQ_VWEP") e.fields = {"vw_index"};
  if (e.name == "FTE_CALLTIMEOFDAY") e.hooks = {"timeofday"};
}

}  // namespace

bool install_catalogue(QcServer& s, std::string& error) {
  for (int i = 0; i < kCatalogueCount; i++) {
    const CatalogueRow& r = kCatalogue[i];
    if (r.number >= static_cast<int>(s.q.vm.builtins.size())) {
      error = std::string("install_catalogue: builtin #") + std::to_string(r.number) + " (" +
              r.name + ") is past the VM's table of " + std::to_string(s.q.vm.builtins.size()) +
              "; raise sim::kQcBuiltins";
      return false;
    }
    if (r.extension[0] == 0) pg::set_builtin(s.q.vm, r.number, resolve(r), r.name);
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
