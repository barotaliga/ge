// The test mod (test_mod.hpp), assembled function by function. Each function's comment is the QC
// it stands for.
//
// ge:summary The test mod's assembly: every function as the QC it stands for, the arena's entities
// ge:keywords test-mod assembly build_test_mod test_mod_bytes worldspawn putclientinserver
// ge:keywords w_firerocket missile_touch t_damage startframe usercmd chatmessage
// ge:skill progs shooter
// ge:test tests/shooters_edge_mod_match_test.cpp
#include "shooters_edge/server/test_mod.hpp"

#include <initializer_list>
#include <utility>

#include "progs/assembler.hpp"
#include "progs/layout.hpp"
#include "progs/opcodes.hpp"
#include "progs/qc_compile.hpp"
#include "shooter/entity.hpp"

namespace shooters_edge::server {

namespace {

namespace pg = ge::progs;
namespace sh = ge::shooter;

constexpr int kLocals = 48;  // words of locals each function gets (more than any uses)

// The assembler with a function's scratch: each `loc` is a fresh local of the open function, so a
// function that is re-entered (T_Damage respawning, a touch inside a move) keeps its own.
struct Qc {
  pg::Assembler a;
  int next = 0;
  int ptr = 0;  // the open function's pointer temporary (ADDRESS, then STOREP)

  int32_t begin(const char* name, std::initializer_list<uint8_t> parms = {}) {
    const int32_t fn = a.begin_function(name, parms, kLocals, "mod.qc");
    next = 0;
    ptr = loc();
    return fn;
  }
  void end() {
    a.emit(pg::kOpDone);
    a.end_function();
  }
  int loc(int words = 1) {
    const int at = a.local(next);
    next += words;
    return at;
  }
  // e.f -> dst, by the field's type.
  void load(uint16_t op, int e, int f, int dst) { a.emit(op, e, f, dst); }
  // src -> e.f, by the store's type (STOREP_F, _V, _S, _ENT, _FNC).
  void store(uint16_t op, int e, int f, int src) {
    a.emit(pg::kOpAddress, e, f, ptr);
    a.emit(op, src, ptr);
  }
  void ret(int g) { a.emit(pg::kOpReturn, g); }
};

}  // namespace

std::vector<uint8_t> test_mod_bytes(std::vector<int32_t>& lines) {
  Qc q;
  pg::Assembler& a = q.a;
  using pg::kTypeEntity;
  using pg::kTypeFloat;

  // Globals of the mod's own.
  const int lastspot = a.global("lastspot", kTypeEntity);
  const int match_over = a.global("match_over", kTypeFloat);
  const int frames = a.global("frames", kTypeFloat);
  const int has_pause = a.global("has_pause", kTypeFloat);
  const int start_hour = a.global("start_hour", kTypeFloat);

  // Fields: the engine's (constants holding their offsets) and the mod's own, then the extension
  // fields the server wires into the player move.
  const int classname = a.field("classname", pg::kTypeString);
  const int health = a.field("health", kTypeFloat);
  const int frags = a.field("frags", kTypeFloat);
  const int takedamage = a.field("takedamage", kTypeFloat);
  const int solid = a.field("solid", kTypeFloat);
  const int movetype = a.field("movetype", kTypeFloat);
  const int origin = a.field("origin", pg::kTypeVector);
  const int angles = a.field("angles", pg::kTypeVector);
  const int velocity = a.field("velocity", pg::kTypeVector);
  const int view_ofs = a.field("view_ofs", pg::kTypeVector);
  const int v_angle = a.field("v_angle", pg::kTypeVector);
  const int fixangle = a.field("fixangle", kTypeFloat);
  const int button0 = a.field("button0", kTypeFloat);
  const int owner = a.field("owner", kTypeEntity);
  const int touch = a.field("touch", pg::kTypeFunction);
  const int think = a.field("think", pg::kTypeFunction);
  const int nextthink = a.field("nextthink", kTypeFloat);
  const int netname = a.field("netname", pg::kTypeString);
  const int chain = a.field("chain", kTypeEntity);
  const int attack_finished = a.field("attack_finished", kTypeFloat);
  a.field("movement", pg::kTypeVector);
  const int maxspeed = a.field("maxspeed", kTypeFloat);
  const int gravity = a.field("gravity", kTypeFloat);

  // Constants.
  const int zero = a.constant_f(0), one = a.constant_f(1), two = a.constant_f(2);
  const int three = a.constant_f(3), four = a.constant_f(4), half = a.constant_f(0.5f);
  const int hundred = a.constant_f(100), sixty = a.constant_f(60), thousand = a.constant_f(1000);
  const int eight = a.constant_f(8), refire = a.constant_f(kRefire);
  const int rocket_speed = a.constant_f(kRocketSpeed), radius = a.constant_f(kBlastRadius);
  const int five = a.constant_f(5), max_speed = a.constant_f(320);
  const int svc_temp_entity = a.constant_f(23);
  const int slidebox = a.constant_f(sh::kSolidSlidebox), walk = a.constant_f(sh::kMovetypeWalk);
  const int bbox = a.constant_f(sh::kSolidBbox), flymissile = a.constant_f(sh::kMovetypeFlyMissile);
  const int up16 = a.constant_v(0, 0, 16), up1 = a.constant_v(0, 0, 1);
  const int view = a.constant_v(0, 0, 22), vzero = a.constant_v(0, 0, 0);
  const int pmins = a.constant_v(-16, -16, -24), pmaxs = a.constant_v(16, 16, 32);
  const int s_player_mdl = a.constant_s("progs/player.mdl");
  const int s_missile_mdl = a.constant_s("progs/missile.mdl");
  const int s_rocket_wav = a.constant_s("weapons/rocket.wav");
  const int s_explode_wav = a.constant_s("weapons/explode.wav");
  const int s_spot = a.constant_s("info_player_deathmatch");
  const int s_player = a.constant_s("player");
  const int s_missile = a.constant_s("missile");
  const int s_m = a.constant_s("m");
  const int s_zq_pause = a.constant_s("ZQ_PAUSE");
  const int s_ftecall = a.constant_s("FTE_CALLTIMEOFDAY");
  const int s_entered = a.constant_s(" entered the game\n");
  const int s_left = a.constant_s(" left the game with ");
  const int s_frags_nl = a.constant_s(" frags\n");
  const int s_bored = a.constant_s(" becomes bored with life\n");
  const int s_rides = a.constant_s(" rides ");
  const int s_rocket = a.constant_s("'s rocket\n");
  const int s_timelimit = a.constant_s("timelimit");
  const int s_tl_hit = a.constant_s("Timelimit hit.\n");
  const int s_frags_cmd = a.constant_s("frags");
  const int s_frags_is = a.constant_s("frags: ");
  const int s_nl = a.constant_s("\n");
  const int s_gg = a.constant_s("gg");
  const int s_good_game = a.constant_s(" says good game\n");

  // The builtins by their standard numbers.
  const int32_t makevectors = a.builtin("makevectors", 1, {3});
  const int32_t setorigin = a.builtin("setorigin", 2, {1, 3});
  const int32_t setmodel = a.builtin("setmodel", 3, {1, 1});
  const int32_t setsize = a.builtin("setsize", 4, {1, 3, 3});
  const int32_t sound = a.builtin("sound", 8, {1, 1, 1, 1, 1});
  const int32_t vlen = a.builtin("vlen", 12, {3});
  const int32_t spawn = a.builtin("spawn", 14);
  const int32_t remove = a.builtin("remove", 15, {1});
  const int32_t find = a.builtin("find", 18, {1, 1, 1});
  const int32_t precache_sound = a.builtin("precache_sound", 19, {1});
  const int32_t precache_model = a.builtin("precache_model", 20, {1});
  const int32_t findradius = a.builtin("findradius", 22, {3, 1});
  const int32_t bprint = a.builtin("bprint", 23);
  const int32_t sprint = a.builtin("sprint", 24);
  const int32_t ftos = a.builtin("ftos", 26, {1});
  const int32_t lightstyle = a.builtin("lightstyle", 35, {1, 1});
  const int32_t cvar = a.builtin("cvar", 45, {1});
  const int32_t vectoangles = a.builtin("vectoangles", 51, {3});
  const int32_t writebyte = a.builtin("WriteByte", 52, {1, 1});
  const int32_t writecoord = a.builtin("WriteCoord", 56, {1, 1});
  const int32_t changelevel = a.builtin("changelevel", 70, {1});
  const int32_t logfrag = a.builtin("logfrag", 79, {1, 1});
  const int32_t multicast = a.builtin("multicast", 82, {3, 1});
  const int32_t checkextension = a.builtin("checkextension", 99, {1});
  const int32_t calltimeofday = a.builtin("calltimeofday", 231);

  const int32_t put_client = a.declare("PutClientInServer");
  const int32_t t_damage = a.declare("T_Damage");
  const int32_t missile_touch = a.declare("missile_touch");
  const int32_t sub_remove = a.declare("SUB_Remove");
  const int touch_fn = a.constant_fn(missile_touch), remove_fn = a.constant_fn(sub_remove);

  // void() worldspawn: the precaches, light style 0, the extensions asked for.
  a.line(10);
  q.begin("worldspawn");
  {
    a.call(precache_model, {{s_player_mdl, 1}});
    a.call(precache_model, {{s_missile_mdl, 1}});
    a.call(precache_sound, {{s_rocket_wav, 1}});
    a.call(precache_sound, {{s_explode_wav, 1}});
    a.call(lightstyle, {{zero, 1}, {s_m, 1}});
    a.call(checkextension, {{s_zq_pause, 1}});
    a.emit(pg::kOpStoreF, pg::kGlobalReturn, has_pause);
    a.call(checkextension, {{s_ftecall, 1}});
    const int no = a.label();
    a.jump(pg::kOpIfNot, no, pg::kGlobalReturn);
    a.call(calltimeofday);
    a.place(no);
  }
  q.end();

  // void(float sec, min, hour, day, mon, year, string date) timeofday: start_hour = hour.
  q.begin("timeofday", {1, 1, 1, 1, 1, 1, 1});
  a.emit(pg::kOpStoreF, a.param(2), start_hour);
  q.end();

  q.begin("info_player_deathmatch");
  q.end();

  // void() SetNewParms: parm1 = 100; parm2 = 0.
  q.begin("SetNewParms");
  a.emit(pg::kOpStoreF, hundred, pg::kGParm1);
  a.emit(pg::kOpStoreF, zero, pg::kGParm1 + 1);
  q.end();

  // void() SetChangeParms: parm1 = self.health; parm2 = self.frags.
  q.begin("SetChangeParms");
  q.load(pg::kOpLoadF, pg::kGSelf, health, pg::kGParm1);
  q.load(pg::kOpLoadF, pg::kGSelf, frags, pg::kGParm1 + 1);
  q.end();

  // void() ClientConnect: bprint(PRINT_HIGH, self.netname, " entered the game\n").
  a.line(30);
  q.begin("ClientConnect");
  {
    const int n = q.loc();
    q.load(pg::kOpLoadS, pg::kGSelf, netname, n);
    a.call(bprint, {{two, 1}, {n, 1}, {s_entered, 1}});
  }
  q.end();

  // void() PutClientInServer: the next spot round the map, then a fresh player there.
  a.line(40);
  q.begin("PutClientInServer");
  {
    const int spot = q.loc(), t = q.loc(3);
    a.call(find, {{lastspot, 1}, {classname, 1}, {s_spot, 1}});
    a.emit(pg::kOpStoreEnt, pg::kGlobalReturn, spot);
    const int found = a.label();
    a.jump(pg::kOpIf, found, spot);
    a.call(find, {{pg::kGWorld, 1}, {classname, 1}, {s_spot, 1}});
    a.emit(pg::kOpStoreEnt, pg::kGlobalReturn, spot);
    a.place(found);
    a.emit(pg::kOpStoreEnt, spot, lastspot);
    q.store(pg::kOpStorePS, pg::kGSelf, classname, s_player);
    q.store(pg::kOpStorePF, pg::kGSelf, health, hundred);
    q.store(pg::kOpStorePF, pg::kGSelf, takedamage, two);
    q.store(pg::kOpStorePF, pg::kGSelf, solid, slidebox);
    q.store(pg::kOpStorePF, pg::kGSelf, movetype, walk);
    a.emit(pg::kOpAddF, pg::kGTime, half, t);
    q.store(pg::kOpStorePF, pg::kGSelf, attack_finished, t);
    a.call(setmodel, {{pg::kGSelf, 1}, {s_player_mdl, 1}});
    a.call(setsize, {{pg::kGSelf, 1}, {pmins, 3}, {pmaxs, 3}});
    q.store(pg::kOpStorePV, pg::kGSelf, view_ofs, view);
    q.store(pg::kOpStorePV, pg::kGSelf, velocity, vzero);
    q.load(pg::kOpLoadV, spot, origin, t);
    a.emit(pg::kOpAddV, t, up1, t);
    a.call(setorigin, {{pg::kGSelf, 1}, {t, 3}});
    q.load(pg::kOpLoadV, spot, angles, t);
    q.store(pg::kOpStorePV, pg::kGSelf, angles, t);
    q.store(pg::kOpStorePF, pg::kGSelf, fixangle, one);
    q.store(pg::kOpStorePF, pg::kGSelf, maxspeed, max_speed);
    q.store(pg::kOpStorePF, pg::kGSelf, gravity, one);
  }
  q.end();

  q.begin("PlayerPreThink");
  q.end();

  // void() W_FireRocket: a missile along the view.
  a.line(70);
  const int32_t fire = q.begin("W_FireRocket");
  {
    const int m = q.loc(), t = q.loc(3), u = q.loc(3);
    q.load(pg::kOpLoadV, pg::kGSelf, v_angle, t);
    a.call(makevectors, {{t, 3}});
    a.call(sound, {{pg::kGSelf, 1}, {one, 1}, {s_rocket_wav, 1}, {one, 1}, {one, 1}});
    a.call(spawn);
    a.emit(pg::kOpStoreEnt, pg::kGlobalReturn, m);
    a.emit(pg::kOpStoreEnt, m, pg::kGNewmis);
    q.store(pg::kOpStorePEnt, m, owner, pg::kGSelf);
    q.store(pg::kOpStorePF, m, movetype, flymissile);
    q.store(pg::kOpStorePF, m, solid, bbox);
    q.store(pg::kOpStorePS, m, classname, s_missile);
    a.emit(pg::kOpMulVF, pg::kGVForward, rocket_speed, t);
    q.store(pg::kOpStorePV, m, velocity, t);
    a.call(vectoangles, {{t, 3}});
    q.store(pg::kOpStorePV, m, angles, pg::kGlobalReturn);
    q.store(pg::kOpStorePFnc, m, touch, touch_fn);
    a.emit(pg::kOpAddF, pg::kGTime, five, t);
    q.store(pg::kOpStorePF, m, nextthink, t);
    q.store(pg::kOpStorePFnc, m, think, remove_fn);
    a.call(setmodel, {{m, 1}, {s_missile_mdl, 1}});
    a.call(setsize, {{m, 1}, {vzero, 3}, {vzero, 3}});
    q.load(pg::kOpLoadV, pg::kGSelf, origin, t);
    a.emit(pg::kOpMulVF, pg::kGVForward, eight, u);
    a.emit(pg::kOpAddV, t, u, t);
    a.emit(pg::kOpAddV, t, up16, t);
    a.call(setorigin, {{m, 1}, {t, 3}});
  }
  q.end();

  // void() PlayerPostThink: fire when attack is held, the match is on and the weapon is ready.
  a.line(100);
  q.begin("PlayerPostThink");
  {
    const int c = q.loc(), t = q.loc();
    const int out = a.label();
    a.jump(pg::kOpIf, out, match_over);
    q.load(pg::kOpLoadF, pg::kGSelf, button0, c);
    a.jump(pg::kOpIfNot, out, c);
    q.load(pg::kOpLoadF, pg::kGSelf, attack_finished, t);
    a.emit(pg::kOpLt, pg::kGTime, t, c);
    a.jump(pg::kOpIf, out, c);
    a.emit(pg::kOpAddF, pg::kGTime, refire, t);
    q.store(pg::kOpStorePF, pg::kGSelf, attack_finished, t);
    a.call(fire);
    a.place(out);
  }
  q.end();

  // void() SUB_Remove: remove(self).
  q.begin("SUB_Remove");
  a.call(remove, {{pg::kGSelf, 1}});
  q.end();

  // void() missile_touch: the explosion, the blast's damage, the missile gone.
  a.line(120);
  q.begin("missile_touch");
  {
    const int org = q.loc(3), head = q.loc(), d = q.loc(), c = q.loc(), t = q.loc(3), own = q.loc();
    const int done = a.label(), loop = a.label(), next = a.label();
    q.load(pg::kOpLoadEnt, pg::kGSelf, owner, own);
    a.emit(pg::kOpEqE, pg::kGOther, own, c);
    a.jump(pg::kOpIf, done, c);
    q.load(pg::kOpLoadV, pg::kGSelf, origin, org);
    a.call(writebyte, {{four, 1}, {svc_temp_entity, 1}});  // MSG_MULTICAST, svc_temp_entity
    a.call(writebyte, {{four, 1}, {three, 1}});            // TE_EXPLOSION
    a.call(writecoord, {{four, 1}, {org, 1}});
    a.call(writecoord, {{four, 1}, {org + 1, 1}});
    a.call(writecoord, {{four, 1}, {org + 2, 1}});
    a.call(multicast, {{org, 3}, {one, 1}});  // MULTICAST_PHS
    a.call(sound, {{pg::kGSelf, 1}, {one, 1}, {s_explode_wav, 1}, {one, 1}, {one, 1}});
    a.call(findradius, {{org, 3}, {radius, 1}});
    a.emit(pg::kOpStoreEnt, pg::kGlobalReturn, head);
    a.place(loop);
    a.jump(pg::kOpIfNot, done, head);
    q.load(pg::kOpLoadF, head, takedamage, c);
    a.jump(pg::kOpIfNot, next, c);
    q.load(pg::kOpLoadV, head, origin, t);
    a.emit(pg::kOpSubV, t, org, t);
    a.call(vlen, {{t, 3}});
    a.emit(pg::kOpMulF, pg::kGlobalReturn, half, d);
    a.emit(pg::kOpSubF, hundred, d, d);
    a.emit(pg::kOpGt, d, zero, c);
    a.jump(pg::kOpIfNot, next, c);
    a.call(t_damage, {{head, 1}, {pg::kGSelf, 1}, {own, 1}, {d, 1}});
    a.place(next);
    q.load(pg::kOpLoadEnt, head, chain, head);
    a.jump(pg::kOpGoto, loop);
    a.place(done);
    a.emit(pg::kOpEqE, pg::kGOther, own, c);
    const int keep = a.label();
    a.jump(pg::kOpIf, keep, c);
    a.call(remove, {{pg::kGSelf, 1}});
    a.place(keep);
  }
  q.end();

  // void(entity targ, entity inflictor, entity attacker, float damage) T_Damage.
  a.line(160);
  q.begin("T_Damage", {1, 1, 1, 1});
  {
    const int targ = a.param(0), attacker = a.param(2), damage = a.param(3);
    const int h = q.loc(), c = q.loc(), n = q.loc(), n2 = q.loc(), f = q.loc(), oldself = q.loc();
    const int out = a.label(), suicide = a.label(), logged = a.label();
    q.load(pg::kOpLoadF, targ, health, h);
    a.emit(pg::kOpSubF, h, damage, h);
    q.store(pg::kOpStorePF, targ, health, h);
    a.emit(pg::kOpGt, h, zero, c);
    a.jump(pg::kOpIf, out, c);
    q.load(pg::kOpLoadS, targ, netname, n);
    a.emit(pg::kOpEqE, attacker, targ, c);
    a.jump(pg::kOpIf, suicide, c);
    q.load(pg::kOpLoadF, attacker, frags, f);
    a.emit(pg::kOpAddF, f, one, f);
    q.store(pg::kOpStorePF, attacker, frags, f);
    q.load(pg::kOpLoadS, attacker, netname, n2);
    a.call(bprint, {{one, 1}, {n, 1}, {s_rides, 1}, {n2, 1}, {s_rocket, 1}});
    a.jump(pg::kOpGoto, logged);
    a.place(suicide);
    q.load(pg::kOpLoadF, targ, frags, f);
    a.emit(pg::kOpSubF, f, one, f);
    q.store(pg::kOpStorePF, targ, frags, f);
    a.call(bprint, {{one, 1}, {n, 1}, {s_bored, 1}});
    a.place(logged);
    a.call(logfrag, {{attacker, 1}, {targ, 1}});
    a.emit(pg::kOpStoreEnt, pg::kGSelf, oldself);
    a.emit(pg::kOpStoreEnt, targ, pg::kGSelf);
    a.call(put_client);
    a.emit(pg::kOpStoreEnt, oldself, pg::kGSelf);
    a.place(out);
  }
  q.end();

  // void() ClientKill: T_Damage(self, self, self, 1000).
  q.begin("ClientKill");
  a.call(t_damage, {{pg::kGSelf, 1}, {pg::kGSelf, 1}, {pg::kGSelf, 1}, {thousand, 1}});
  q.end();

  // void() ClientDisconnect: bprint(PRINT_HIGH, name, " left the game with ", ftos(frags), ...).
  q.begin("ClientDisconnect");
  {
    const int n = q.loc(), f = q.loc();
    q.load(pg::kOpLoadS, pg::kGSelf, netname, n);
    q.load(pg::kOpLoadF, pg::kGSelf, frags, f);
    a.call(ftos, {{f, 1}});
    a.emit(pg::kOpStoreS, pg::kGlobalReturn, f);
    a.call(bprint, {{two, 1}, {n, 1}, {s_left, 1}, {f, 1}, {s_frags_nl, 1}});
  }
  q.end();

  // void() StartFrame: frames += 1; the time limit ends the match and changes the level.
  a.line(200);
  q.begin("StartFrame");
  {
    const int t = q.loc(), c = q.loc();
    const int out = a.label();
    a.emit(pg::kOpAddF, frames, one, frames);
    a.jump(pg::kOpIf, out, match_over);
    a.call(cvar, {{s_timelimit, 1}});
    a.emit(pg::kOpMulF, pg::kGlobalReturn, sixty, t);
    a.jump(pg::kOpIfNot, out, t);
    a.emit(pg::kOpLt, pg::kGTime, t, c);
    a.jump(pg::kOpIf, out, c);
    a.emit(pg::kOpStoreF, one, match_over);
    a.call(bprint, {{two, 1}, {s_tl_hit, 1}});
    a.call(changelevel, {{pg::kGMapname, 1}});
    a.place(out);
  }
  q.end();

  // float(string cmd) UserCmd: "frags" prints the player's frags.
  a.line(220);
  q.begin("UserCmd", {1});
  {
    const int c = q.loc(), f = q.loc();
    const int no = a.label();
    a.emit(pg::kOpEqS, a.param(0), s_frags_cmd, c);
    a.jump(pg::kOpIfNot, no, c);
    q.load(pg::kOpLoadF, pg::kGSelf, frags, f);
    a.call(ftos, {{f, 1}});
    a.emit(pg::kOpStoreS, pg::kGlobalReturn, f);
    a.call(sprint, {{pg::kGSelf, 1}, {two, 1}, {s_frags_is, 1}, {f, 1}, {s_nl, 1}});
    q.ret(one);
    a.place(no);
    q.ret(zero);
  }
  q.end();

  // float(string text, float team) ChatMessage: "gg" is announced, handled.
  q.begin("ChatMessage", {1, 1});
  {
    const int c = q.loc(), n = q.loc();
    const int no = a.label();
    a.emit(pg::kOpEqS, a.param(0), s_gg, c);
    a.jump(pg::kOpIfNot, no, c);
    q.load(pg::kOpLoadS, pg::kGSelf, netname, n);
    a.call(bprint, {{three, 1}, {n, 1}, {s_good_game, 1}});
    q.ret(one);
    a.place(no);
    q.ret(zero);
  }
  q.end();

  // The hook globals, as a compiler fills them.
  const std::pair<int, const char*> hooks[] = {{pg::kGStartFrame, "StartFrame"},
                                               {pg::kGPlayerPreThink, "PlayerPreThink"},
                                               {pg::kGPlayerPostThink, "PlayerPostThink"},
                                               {pg::kGClientKill, "ClientKill"},
                                               {pg::kGClientConnect, "ClientConnect"},
                                               {pg::kGPutClientInServer, "PutClientInServer"},
                                               {pg::kGClientDisconnect, "ClientDisconnect"},
                                               {pg::kGSetNewParms, "SetNewParms"},
                                               {pg::kGSetChangeParms, "SetChangeParms"}};
  for (const auto& [g, name] : hooks) {
    const int32_t name_ofs = a.string(name);
    for (size_t i = 1; i < a.functions.size(); i++)
      if (a.functions[i].name == name_ofs)
        a.globals[static_cast<size_t>(g)] = static_cast<uint32_t>(i);
  }
  lines = a.lines();
  return a.bytes();
}

bool build_test_mod(ge::progs::Progs& out, std::string& error) {
  std::vector<int32_t> lines;
  const std::vector<uint8_t> bytes = test_mod_bytes(lines);
  if (!ge::progs::load_progs(bytes.data(), bytes.size(), out, error)) return false;
  return ge::progs::set_lines(out, std::move(lines), error);
}

const char* const kArenaEntities = R"({
"classname" "worldspawn"
"message" "the test arena"
"wad" "arena.wad"
"_comment" "the world: its keys before any entity's"
}
{
"classname" "info_player_deathmatch"
"origin" "-416 -256 24"
"angle" "45"
}
{
"classname" "info_player_deathmatch"
"origin" "-416 256 24"
"angle" "315"
}
{
"classname" "info_player_deathmatch"
"origin" "416 256 24"
"angle" "225"
}
{
"classname" "info_player_deathmatch"
"origin" "416 -96 24"
"angle" "180"
}
{
"classname" "item_health"
"origin" "-272 -8 0"
"spawnflags" "2048"
}
{
"classname" "light"
"origin" "0 0 200"
"light" "300"
}
)";

bool build_single_player_stub(ge::progs::Progs& out, std::string& error) {
  pg::Assembler a(pg::kSinglePlayerLayout);
  for (const char* name : {"worldspawn", "target"}) {
    a.begin_function(name, {}, 0, "stub.qc");
    a.emit(pg::kOpDone);
    a.end_function();
  }
  return a.build(out, error);
}

const char* const kStubEntities = R"({
"classname" "worldspawn"
"message" "a single-player map"
}
)";

namespace {

const char* const kCampaignStub = R"(
entity() spawn = #14;
void(string s, ...) dprint = #25;
float(string s) cvar = #45;
entity(entity e) nextent = #47;
void(string s) changelevel = #70;

void() SetNewParms = { parm1 = 100; parm2 = 0; };
void() SetChangeParms = { parm1 = self.health; parm2 = self.frags + 1; };
void() ClientConnect = {};
void() PutClientInServer = {
  self.classname = "player";
  self.health = parm1;
  self.frags = parm2;
};
void() ClientDisconnect = {};
void() ClientKill = {};
void() PlayerPreThink = {};
void() PlayerPostThink = {};
void() StartFrame = {};
void() main = {};

void() leave = {
  local entity p;
  p = nextent(world);
  p.health = p.health - 25;
  serverflags = serverflags + 1;
  changelevel("second");
  changelevel("third");
};

void() count = {
  self.frags = self.frags + 1;
  self.nextthink = time + 0.001;
};

void() worldspawn = {
  local entity t;
  world.max_health = cvar("registered");
  world.frags = cvar("skill");
  world.armorvalue = deathmatch;
  world.ammo_shells = coop;
  world.items = serverflags;
  total_monsters = 3;
  killed_monsters = 1;
  total_secrets = 2;
  if (mapname == "first") {
    t = spawn();
    t.think = leave;
    t.nextthink = 1.5;
  }
  if (mapname == "second") {
    t = spawn();
    t.classname = "counter";
    t.think = count;
    t.nextthink = time + 0.001;
  }
};
)";

}  // namespace

bool build_campaign_stub(ge::progs::Progs& out, std::string& error) {
  const std::vector<pg::QcSource> src{{"defs.qc", pg::layout_declarations(pg::kSinglePlayerLayout)},
                                      {"campaign.qc", kCampaignStub}};
  pg::QcResult r = pg::compile_qc(src);
  if (!r.ok) {
    error = "the campaign stub did not compile:";
    for (const pg::QcDiagnostic& d : r.diagnostics) error += " " + pg::diagnostic_text(d) + ";";
    return false;
  }
  if (r.progs.crc != pg::kSinglePlayerCrc) {
    error = "the campaign stub is not of the single-player form (crc " +
            std::to_string(r.progs.crc) + ")";
    return false;
  }
  out = std::move(r.progs);
  return true;
}

}  // namespace shooters_edge::server
