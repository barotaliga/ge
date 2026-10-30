// THE BUILTIN CATALOGUE: every builtin a multiplayer mod may call, by number, and the extensions
// the server answers `checkextension` for, as one table. A row is a number, a name, the function
// (the game's, or the plugin's standard one from plugins/progs/stdlib.hpp when null), and the
// extension that groups it ("" for the standard numbers 1..82). `install_catalogue` puts the
// standard rows into the VM's table and registers each extension (its builtins, fields and hooks)
// in the server's registry, which installs the rest.
//
// THE STANDARD NUMBERS (1..82): 1 makevectors, 2 setorigin, 3 setmodel, 4 setsize, 6 break,
// 7 random, 8 sound, 9 normalize, 10 error, 11 objerror, 12 vlen, 13 vectoyaw, 14 spawn,
// 15 remove, 16 traceline, 17 checkclient, 18 find, 19 precache_sound, 20 precache_model,
// 21 stuffcmd, 22 findradius, 23 bprint, 24 sprint, 25 dprint, 26 ftos, 27 vtos, 28 coredump,
// 29 traceon, 30 traceoff, 31 eprint, 32 walkmove, 34 droptofloor, 35 lightstyle, 36 rint,
// 37 floor, 38 ceil, 40 checkbottom, 41 pointcontents, 43 fabs, 44 aim, 45 cvar, 46 localcmd,
// 47 nextent, 48 particle, 49 changeyaw, 51 vectoangles, 52..59 the Write family, 67 movetogoal,
// 68 precache_file, 69 makestatic, 70 changelevel, 72 cvar_set, 73 centerprint, 74 ambientsound,
// 75..77 precache_model2, precache_sound2, precache_file2, 78 setspawnparms, 79 logfrag,
// 80 infokey, 81 stof, 82 multicast. Unlisted numbers (0, 5, 33, 39, 42, 50, 63..66, 71) are
// left empty: the VM faults "builtin N is not provided by this game".
//
// THE SINGLE-PLAYER FORM (a program of crc 5927, the campaign's): 79..82 are left empty;
// bprint takes no level (`(s ...)`) and sprint none (`(client, s ...)`); aim is the autoaim under
// `sv_aim`; the Write family has no multicast destination (0..3).
//
// THE EXTENSIONS: DP_QC_SINCOSSQRTPOW (60 sin, 61 cos, 62 sqrt, 97 pow), ZQ_QC_TOKENIZE (84
// tokenize, 85 argc, 86 argv), DP_QC_TRACEBOX (90), DP_QC_RANDOMVEC (91), DP_REGISTERCVAR (93),
// DP_QC_MINMAXBOUND (94 min, 95 max, 96 bound), QSG_CVARSTRING (103 cvar_string),
// DP_QC_CVAR_STRING (448 cvar_string), ZQ_QC_STRINGS (114 strlen, 115 strcat, 116 substr, 118
// strzone, 119 strunzone), FTE_CALLTIMEOFDAY (231), ZQ_PAUSE (531 setpause; the pause hooks),
// ZQ_VWEP (532 precache_vwep_model; vw_index), and with no builtin: DP_CON_SET (the console's
// `set`), ZQ_CLIENTCOMMAND (the GE_ClientCommand hook), ZQ_ITEMS2 (items2), ZQ_MOVETYPE_NOCLIP,
// ZQ_MOVETYPE_FLY, ZQ_MOVETYPE_NONE (the loop's move types). `checkextension` (#99) answers 1 for
// any of these names in any case.
//
// ge:summary The builtin catalogue: every number, its function and its extension; checkextension
// ge:keywords qc builtins catalogue table numbers extensions checkextension extension-list
// ge:keywords install_catalogue offers_extension dp_qc zq_qc fte qsg
// ge:skill progs shooter
// ge:test tests/shooters_edge_qc_builtins_test.cpp
#pragma once

#include "shooters_edge/server/qc_server.hpp"

namespace shooters_edge::server {

struct CatalogueRow {
  int number;
  const char* name;
  pg::Builtin fn;         // null: plugins/progs/stdlib.hpp's builtin of this number
  const char* extension;  // "" for a standard number
};

extern const CatalogueRow kCatalogue[];
extern const int kCatalogueCount;

// The extension names `checkextension` answers yes to, in the table's order.
extern const char* const kExtensionNames[];
extern const int kExtensionCount;

// The standard rows into the VM, every extension registered (and its builtins installed).
// False with `error` when the registry refuses a row (a duplicate number).
bool install_catalogue(QcServer& s, std::string& error);
// Whether the server offers extension `name` (any case).
bool offers_extension(const QcServer& s, const char* name);

}  // namespace shooters_edge::server
