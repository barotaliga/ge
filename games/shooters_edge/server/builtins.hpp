// THE GAME'S BUILTINS, declared for the catalogue (catalogue.cpp puts each at its number). Each is
// a `pg::Builtin`: it reads its arguments from the parameter slots, writes the return slot, and
// faults the call with `pg::vm_error`. They run under the server the adapter's call is for
// (`server_of`); one that reads the world first flushes the VM's entities to the loop
// (`flush_to_world`), and one that moves the world refreshes them back (`refresh_from_world`),
// restoring `self`, `other` and `time` after any QC it ran (a touch). What each does is stated
// in catalogue.hpp; the rules of the monster moves are bi_monster.cpp's.
//
// ge:summary The game's builtins, declared for the catalogue: world, monsters, messages, settings
// ge:keywords qc builtins declarations world traces messages settings monster-moves
// ge:skill progs shooter
// ge:test tests/shooters_edge_qc_builtins_test.cpp
#pragma once

#include "progs/vm.hpp"
#include "shooters_edge/server/qc_server.hpp"

namespace shooters_edge::server {

// World and entities (bi_world.cpp).
void bi_makevectors(pg::Vm& vm);
void bi_setmodel(pg::Vm& vm);
void bi_setsize(pg::Vm& vm);
void bi_break(pg::Vm& vm);
void bi_traceline(pg::Vm& vm);
void bi_tracebox(pg::Vm& vm);
void bi_checkclient(pg::Vm& vm);
void bi_find(pg::Vm& vm);
void bi_findradius(pg::Vm& vm);
void bi_droptofloor(pg::Vm& vm);
void bi_pointcontents(pg::Vm& vm);
void bi_aim(pg::Vm& vm);
void bi_nextent(pg::Vm& vm);
void bi_makestatic(pg::Vm& vm);
void bi_setspawnparms(pg::Vm& vm);
void bi_coredump(pg::Vm& vm);
void bi_traceon(pg::Vm& vm);
void bi_traceoff(pg::Vm& vm);
void bi_eprint(pg::Vm& vm);

// The monster moves (bi_monster.cpp).
void bi_walkmove(pg::Vm& vm);
void bi_checkbottom(pg::Vm& vm);
void bi_changeyaw(pg::Vm& vm);
void bi_movetogoal(pg::Vm& vm);

// The monster moves' parts, for the tests: an angle wrapped into 0..360 on the 16-bit grid; a
// yaw turned toward an ideal by at most a speed; whether an entity stands on ground; the step
// move (with `enemy` 0 for none).
float wrap_angle(float a);
float turned_yaw(float yaw, float ideal, float speed);
bool check_bottom(QcServer& s, int e);
bool step_move(QcServer& s, int e, sh::Vec3f move, int enemy, bool relink);

// Messages, prints and sounds (bi_message.cpp).
void bi_sound(pg::Vm& vm);
void bi_stuffcmd(pg::Vm& vm);
void bi_bprint(pg::Vm& vm);
void bi_sprint(pg::Vm& vm);
void bi_dprint(pg::Vm& vm);
void bi_lightstyle(pg::Vm& vm);
void bi_particle(pg::Vm& vm);
void bi_writebyte(pg::Vm& vm);
void bi_writechar(pg::Vm& vm);
void bi_writeshort(pg::Vm& vm);
void bi_writelong(pg::Vm& vm);
void bi_writecoord(pg::Vm& vm);
void bi_writeangle(pg::Vm& vm);
void bi_writestring(pg::Vm& vm);
void bi_writeentity(pg::Vm& vm);
void bi_centerprint(pg::Vm& vm);
void bi_ambientsound(pg::Vm& vm);
void bi_multicast(pg::Vm& vm);

// Settings, info keys, precaching, the map's flow and the date (bi_settings.cpp).
void bi_error(pg::Vm& vm);
void bi_objerror(pg::Vm& vm);
void bi_precache_sound(pg::Vm& vm);
void bi_precache_model(pg::Vm& vm);
void bi_cvar(pg::Vm& vm);
void bi_localcmd(pg::Vm& vm);
void bi_changelevel(pg::Vm& vm);
void bi_cvar_set(pg::Vm& vm);
void bi_logfrag(pg::Vm& vm);
void bi_infokey(pg::Vm& vm);
void bi_registercvar(pg::Vm& vm);
void bi_checkextension(pg::Vm& vm);
void bi_cvar_string(pg::Vm& vm);
void bi_calltimeofday(pg::Vm& vm);
void bi_setpause(pg::Vm& vm);
void bi_precache_vwep_model(pg::Vm& vm);

// The recipients of a sound, a multicast or a print: a bit per client slot.
// Every active, spawned client.
uint32_t all_clients(const QcServer& s);
// Clients that can hear (`see` false) or see a point: by the sets (`sees`, `hears`), or within
// 1024 units of the eye for hearing. A spectator tracking a player (its `trackent`) is judged
// from that player's eye.
uint32_t clients_reaching(const QcServer& s, sh::Vec3f where, bool see);
// Whether a point is in the visibility (or hearing) set of another. The map's sets come with
// M18b; until then every point is in every set, as on a map compiled without visibility.
bool sees(const QcServer& s, sh::Vec3f from, sh::Vec3f to);
bool hears(const QcServer& s, sh::Vec3f from, sh::Vec3f to);
// An entity's eye: origin + view_ofs.
sh::Vec3f eye_of(const QcServer& s, int e);

// Around a builtin that runs QC (a touch): saves self, other and time, and puts them back.
struct SavedGlobals {
  uint32_t self = 0, other = 0, time = 0;
};
SavedGlobals save_globals(const pg::Vm& vm);
void restore_globals(pg::Vm& vm, const SavedGlobals& g);

}  // namespace shooters_edge::server
