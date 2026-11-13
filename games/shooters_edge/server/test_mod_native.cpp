// The test mod (test_mod.hpp) as a native program: every function in C++ over the classic
// server's calls (rules_calls.hpp), statement for statement as the assembled one runs, so a match
// on either plays the same. Each function's comment is the rule it carries.
//
// ge:summary The test mod written in C++ as a native program, played as the assembled one plays
// ge:keywords test-mod native native-test-mod native-rules worldspawn putclientinserver t_damage
// ge:keywords missile_touch w_firerocket startframe native_test_mod
// ge:skill progs shooter
// ge:test tests/shooters_edge_native_rules_test.cpp
#include "shooters_edge/server/test_mod.hpp"

#include <bit>
#include <cstdint>

#include "shooter/entity.hpp"
#include "shooters_edge/rules/calls.hpp"

namespace shooters_edge::server {

namespace {

namespace sh = ge::shooter;

// The declaration's indices.
enum Global { kLastspot, kMatchOver, kFrames, kHasPause, kStartHour };
enum Field { kAttackFinished, kMovement, kMaxspeed, kGravity };
enum Str {
  kPlayerMdl,
  kMissileMdl,
  kRocketWav,
  kExplodeWav,
  kSpot,
  kPlayer,
  kMissile,
  kM,
  kZqPause,
  kFteCall,
  kEntered,
  kLeft,
  kFragsNl,
  kBored,
  kRides,
  kRocket,
  kTimelimit,
  kTlHit,
  kFragsCmd,
  kFragsIs,
  kNl,
  kGg,
  kGoodGame,
};
enum Fn { kMissileTouch = 11, kSubRemove = 10 };  // their index in kFunctions

constexpr Vec3 kZero{0, 0, 0};

// A condition (IF / IFNOT) tests a word for zero, so -0 is true: the reads below compare words.

// void() worldspawn: the precaches, light style 0, the extensions asked for.
void worldspawn(pg::Vm& vm) {
  Q q{vm};
  q.precache_model(q.str(kPlayerMdl));
  q.precache_model(q.str(kMissileMdl));
  q.precache_sound(q.str(kRocketWav));
  q.precache_sound(q.str(kExplodeWav));
  q.lightstyle(0, q.str(kM));
  q.set_gf(q.own_global(kHasPause), q.checkextension(q.str(kZqPause)));
  q.checkextension(q.str(kFteCall));
  if (pg::result_int(vm) != 0) q.calltimeofday();  // IFNOT tests the word, as bytecode does
}

// void(float sec, min, hour, day, mon, year, string date) timeofday: start_hour = hour.
void timeofday(pg::Vm& vm) {
  Q q{vm};
  q.set_gf(q.own_global(kStartHour), pg::parm_float(vm, 2));
}

void nothing(pg::Vm&) {}

// void() SetNewParms: parm1 = 100; parm2 = 0.
void set_new_parms(pg::Vm& vm) {
  Q q{vm};
  q.set_gf(q.parm(1), 100);
  q.set_gf(q.parm(2), 0);
}

// void() SetChangeParms: parm1 = self.health; parm2 = self.frags.
void set_change_parms(pg::Vm& vm) {
  Q q{vm};
  q.set_gf(q.parm(1), q.f(q.self(), q.L.f.health));
  q.set_gf(q.parm(2), q.f(q.self(), q.L.f.frags));
}

// void() ClientConnect: bprint(PRINT_HIGH, self.netname, " entered the game\n").
void client_connect(pg::Vm& vm) {
  Q q{vm};
  q.bprint(2, {q.i(q.self(), q.L.f.netname), q.str(kEntered)});
}

// void() PutClientInServer: the next spot round the map, then a fresh player there.
void put_client_in_server(pg::Vm& vm) {
  Q q{vm};
  const int lastspot = q.own_global(kLastspot);
  int32_t spot = q.find(q.gi(lastspot), q.L.f.classname, q.str(kSpot));
  if (spot == 0) spot = q.find(q.world(), q.L.f.classname, q.str(kSpot));
  q.set_gi(lastspot, spot);
  q.set_i(q.self(), q.L.f.classname, q.str(kPlayer));
  q.set_f(q.self(), q.L.f.health, 100);
  q.set_f(q.self(), q.L.f.takedamage, 2);
  q.set_f(q.self(), q.L.f.solid, static_cast<float>(sh::kSolidSlidebox));
  q.set_f(q.self(), q.L.f.movetype, static_cast<float>(sh::kMovetypeWalk));
  q.set_f(q.self(), q.own_field(kAttackFinished), q.time() + 0.5f);
  q.setmodel(q.self(), q.str(kPlayerMdl));
  q.setsize(q.self(), {-16, -16, -24}, {16, 16, 32});
  q.set_v(q.self(), q.L.f.view_ofs, {0, 0, 22});
  q.set_v(q.self(), q.L.f.velocity, kZero);
  q.setorigin(q.self(), q.v(spot, q.L.f.origin) + Vec3{0, 0, 1});
  q.set_v(q.self(), q.L.f.angles, q.v(spot, q.L.f.angles));
  q.set_f(q.self(), q.L.f.fixangle, 1);
  q.set_f(q.self(), q.own_field(kMaxspeed), 320);
  q.set_f(q.self(), q.own_field(kGravity), 1);
}

// void() W_FireRocket: a missile along the view.
void fire_rocket(pg::Vm& vm) {
  Q q{vm};
  q.makevectors(q.v(q.self(), q.L.f.v_angle));
  q.sound(q.self(), 1, q.str(kRocketWav), 1, 1);
  const int32_t m = q.spawn();
  q.set_newmis(m);
  q.set_i(m, q.L.f.owner, q.self());
  q.set_f(m, q.L.f.movetype, static_cast<float>(sh::kMovetypeFlyMissile));
  q.set_f(m, q.L.f.solid, static_cast<float>(sh::kSolidBbox));
  q.set_i(m, q.L.f.classname, q.str(kMissile));
  const Vec3 t = q.v_forward() * kRocketSpeed;
  q.set_v(m, q.L.f.velocity, t);
  q.set_v(m, q.L.f.angles, q.vectoangles(t));
  q.set_i(m, q.L.f.touch, q.fn(kMissileTouch));
  q.set_f(m, q.L.f.nextthink, q.time() + 5);
  q.set_i(m, q.L.f.think, q.fn(kSubRemove));
  q.setmodel(m, q.str(kMissileMdl));
  q.setsize(m, kZero, kZero);
  const Vec3 u = q.v_forward() * 8;
  q.setorigin(m, q.v(q.self(), q.L.f.origin) + u + Vec3{0, 0, 16});
}

// void() PlayerPostThink: fire when attack is held, the match is on and the weapon is ready.
void player_post_think(pg::Vm& vm) {
  Q q{vm};
  if (q.gi(q.own_global(kMatchOver)) != 0) return;
  if (q.i(q.self(), q.L.f.button0) == 0) return;
  if (q.time() < q.f(q.self(), q.own_field(kAttackFinished))) return;
  q.set_f(q.self(), q.own_field(kAttackFinished), q.time() + kRefire);
  fire_rocket(vm);
}

// void() SUB_Remove: remove(self).
void sub_remove(pg::Vm& vm) {
  Q q{vm};
  q.remove(q.self());
}

void t_damage_of(pg::Vm& vm, int32_t targ, int32_t attacker, float damage);

// void() missile_touch: the explosion, the blast's damage, the missile gone.
void missile_touch(pg::Vm& vm) {
  Q q{vm};
  const int32_t own = q.i(q.self(), q.L.f.owner);
  if (q.other() != own) {
    const Vec3 org = q.v(q.self(), q.L.f.origin);
    q.WriteByte(4, 23);  // MSG_MULTICAST, svc_temp_entity
    q.WriteByte(4, 3);   // TE_EXPLOSION
    q.WriteCoord(4, org.x);
    q.WriteCoord(4, org.y);
    q.WriteCoord(4, org.z);
    q.multicast(org, 1);  // MULTICAST_PHS
    q.sound(q.self(), 1, q.str(kExplodeWav), 1, 1);
    for (int32_t head = q.findradius(org, kBlastRadius); head != 0 && !q.failed();
         head = q.i(head, q.L.f.chain)) {
      if (q.i(head, q.L.f.takedamage) == 0) continue;
      float d = q.vlen(q.v(head, q.L.f.origin) - org) * 0.5f;
      d = 100 - d;
      if (d > 0) t_damage_of(vm, head, own, d);
    }
  }
  if (q.other() != own) q.remove(q.self());
}

// T_Damage's body: health down; at or below 0 the frags, the obituary, the respawn.
void t_damage_of(pg::Vm& vm, int32_t targ, int32_t attacker, float damage) {
  Q q{vm};
  const float h = q.f(targ, q.L.f.health) - damage;
  q.set_f(targ, q.L.f.health, h);
  if (h > 0) return;
  const int32_t n = q.i(targ, q.L.f.netname);
  if (attacker == targ) {
    q.set_f(targ, q.L.f.frags, q.f(targ, q.L.f.frags) - 1);
    q.bprint(1, {n, q.str(kBored)});
  } else {
    q.set_f(attacker, q.L.f.frags, q.f(attacker, q.L.f.frags) + 1);
    q.bprint(1, {n, q.str(kRides), q.i(attacker, q.L.f.netname), q.str(kRocket)});
  }
  q.logfrag(attacker, targ);
  const int32_t oldself = q.self();
  q.set_self(targ);
  put_client_in_server(vm);
  q.set_self(oldself);
}

// void(entity targ, entity inflictor, entity attacker, float damage) T_Damage.
void t_damage(pg::Vm& vm) {
  t_damage_of(vm, pg::parm_int(vm, 0), pg::parm_int(vm, 2), pg::parm_float(vm, 3));
}

// void() ClientKill: T_Damage(self, self, self, 1000).
void client_kill(pg::Vm& vm) {
  Q q{vm};
  t_damage_of(vm, q.self(), q.self(), 1000);
}

// void() ClientDisconnect: bprint(PRINT_HIGH, name, " left the game with ", ftos(frags), ...).
void client_disconnect(pg::Vm& vm) {
  Q q{vm};
  const int32_t n = q.i(q.self(), q.L.f.netname);
  const int32_t f = q.ftos(q.f(q.self(), q.L.f.frags));
  q.bprint(2, {n, q.str(kLeft), f, q.str(kFragsNl)});
}

// void() StartFrame: frames += 1; the time limit ends the match and changes the level.
void start_frame(pg::Vm& vm) {
  Q q{vm};
  const int frames = q.own_global(kFrames), over = q.own_global(kMatchOver);
  q.set_gf(frames, q.gf(frames) + 1);
  if (q.gi(over) != 0) return;
  const float t = q.cvar(q.str(kTimelimit)) * 60;
  if (std::bit_cast<uint32_t>(t) == 0) return;
  if (q.time() < t) return;
  q.set_gf(over, 1);
  q.bprint(2, {q.str(kTlHit)});
  q.changelevel(q.mapname());
}

// float(string cmd) UserCmd: "frags" prints the player's frags.
void user_cmd(pg::Vm& vm) {
  Q q{vm};
  if (!q.same(pg::parm_int(vm, 0), q.str(kFragsCmd))) {
    pg::set_return_float(vm, 0);
    return;
  }
  const int32_t f = q.ftos(q.f(q.self(), q.L.f.frags));
  q.sprint(q.self(), 2, {q.str(kFragsIs), f, q.str(kNl)});
  pg::set_return_float(vm, 1);
}

// float(string text, float team) ChatMessage: "gg" is announced, handled.
void chat_message(pg::Vm& vm) {
  Q q{vm};
  if (!q.same(pg::parm_int(vm, 0), q.str(kGg))) {
    pg::set_return_float(vm, 0);
    return;
  }
  q.bprint(3, {q.i(q.self(), q.L.f.netname), q.str(kGoodGame)});
  pg::set_return_float(vm, 1);
}

constexpr pg::NativeDef kGlobals[] = {{"lastspot", pg::kTypeEntity},
                                      {"match_over", pg::kTypeFloat},
                                      {"frames", pg::kTypeFloat},
                                      {"has_pause", pg::kTypeFloat},
                                      {"start_hour", pg::kTypeFloat}};
constexpr pg::NativeDef kFields[] = {{"attack_finished", pg::kTypeFloat},
                                     {"movement", pg::kTypeVector},
                                     {"maxspeed", pg::kTypeFloat},
                                     {"gravity", pg::kTypeFloat}};
constexpr const char* kStrings[] = {"progs/player.mdl",
                                    "progs/missile.mdl",
                                    "weapons/rocket.wav",
                                    "weapons/explode.wav",
                                    "info_player_deathmatch",
                                    "player",
                                    "missile",
                                    "m",
                                    "ZQ_PAUSE",
                                    "FTE_CALLTIMEOFDAY",
                                    " entered the game\n",
                                    " left the game with ",
                                    " frags\n",
                                    " becomes bored with life\n",
                                    " rides ",
                                    "'s rocket\n",
                                    "timelimit",
                                    "Timelimit hit.\n",
                                    "frags",
                                    "frags: ",
                                    "\n",
                                    "gg",
                                    " says good game\n"};
constexpr uint8_t kSeven[] = {1, 1, 1, 1, 1, 1, 1};
constexpr uint8_t kFour[] = {1, 1, 1, 1};
constexpr uint8_t kOne[] = {1};
constexpr uint8_t kTwo[] = {1, 1};
constexpr pg::NativeFunction kFunctions[] = {
    {"worldspawn", &worldspawn},
    {"timeofday", &timeofday, kSeven},
    {"info_player_deathmatch", &nothing},
    {"SetNewParms", &set_new_parms},
    {"SetChangeParms", &set_change_parms},
    {"ClientConnect", &client_connect},
    {"PutClientInServer", &put_client_in_server},
    {"PlayerPreThink", &nothing},
    {"W_FireRocket", &fire_rocket},
    {"PlayerPostThink", &player_post_think},
    {"SUB_Remove", &sub_remove},
    {"missile_touch", &missile_touch},
    {"T_Damage", &t_damage, kFour},
    {"ClientKill", &client_kill},
    {"ClientDisconnect", &client_disconnect},
    {"StartFrame", &start_frame},
    {"UserCmd", &user_cmd, kOne},
    {"ChatMessage", &chat_message, kTwo},
};
static_assert(kFunctions[kMissileTouch].run == &missile_touch &&
              kFunctions[kSubRemove].run == &sub_remove);

}  // namespace

const pg::NativeProgram& native_test_mod() {
  static const pg::NativeProgram p = [] {
    pg::NativeProgram n;
    n.file = "test_mod_native";
    n.globals = kGlobals;
    n.fields = kFields;
    n.strings = kStrings;
    n.functions = kFunctions;
    return n;
  }();
  return p;
}

}  // namespace shooters_edge::server
