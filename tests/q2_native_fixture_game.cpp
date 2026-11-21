// A GAME LIBRARY OF QUAKE II'S INTERFACE, OUR OWN, for the native host's test
// (tests/shooters_edge_q2_native_test.cpp, server/q2_native.hpp): built as a loadable
// library named as a mod's is, it makes the calls an existing game library makes, so the host is
// held to the interface without one. A small deathmatch:
//
// - init: the entity array (with the game's own fields after each head) and the clients from the
//   memory calls, sized by `maxclients`;
// - spawn: the world's sky, a brush model's entity on `*1` (when the map's text names it) and a box
// entity on a model from the
//   lists, the first deathmatch spot read from the entity text;
// - a player: refused when named `refuse` (with a reason), else put on the spot, moved by the
//   server's move through the game's own trace callback, firing a spark and a sound on attack;
// - commands: `score` prints to the player, `layout` sends him a layout, `crash` stops the game,
//   `next` asks for map `hall2`, `turn <yaw>` turns the brush entity, `probe <x> <y>` prints where
//   a point traced down there stops and the contents it ends in; its own bar (configstring 5) shows
//   the health number; the server's `sv hello` prints to the console.
// - single player: the spawn point it was given, `hit` takes 10 from the box, `go <level>` asks for
//   a level string, `die` drops the player's health to 0 and asks for the load menu, `where`
//   prints the spawn point, the box's health, `deathmatch` and how the player came in; its level
//   file keeps the box's health and each player's body (used or not, and where), its game file
//   whether it was an autosave.
//
// ge:summary A game library of Quake II's interface for the native host's test
// ge:keywords game-library fixture native-host test-library m18v10
// ge:skill testing shooter
// ge:test tests/shooters_edge_q2_native_test.cpp
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "shooters_edge/server/q2_native_abi.hpp"

#if defined(_WIN32)
#define FIXTURE_EXPORT extern "C" __declspec(dllexport)
#else
#define FIXTURE_EXPORT extern "C" __attribute__((visibility("default")))
#endif

namespace {

using namespace shooters_edge::server::native;  // NOLINT(google-build-using-namespace)

struct Client {
  NativeClientHead head;
  int score;
  int health;
};

struct Entity {
  NativeEntityHead head;
  int health;  // the game's own field after the head
};

constexpr int kMostEntities = 64;
constexpr int kTagGame = 765;
constexpr int kMaskPlayer = 0x1 | 0x2 | 0x10000 | 0x2000000;

NativeImports gi;
NativeExports game;
Entity* entities = nullptr;
Client* clients = nullptr;
int max_clients = 1;
float spot[3] = {0, 0, 24};
int frames = 0;
int spark_sound = 0;
NativeEntityHead* mover = nullptr;  // the player whose move is running
char spawn_point[64] = "";
const char* came_in = "none";  // how the last player came in: "spot" or "body"
int autosaved = 0;             // the game file read said so
NativeCvar* deathmatch = nullptr;

NativeTrace trace_cb(const float* a, const float* lo, const float* hi, const float* b) {
  return gi.trace(a, lo, hi, b, mover, kMaskPlayer);
}

int contents_cb(const float* p) { return gi.pointcontents(p); }

void init() {
  max_clients = static_cast<int>(gi.cvar("maxclients", "4", 0)->value);
  deathmatch = gi.cvar("deathmatch", "0", 16);  // latched: for the next game
  entities = static_cast<Entity*>(gi.tag_malloc(int(sizeof(Entity)) * kMostEntities, kTagGame));
  clients = static_cast<Client*>(gi.tag_malloc(int(sizeof(Client)) * max_clients, kTagGame));
  game.edicts = reinterpret_cast<uint8_t*>(entities);
  game.edict_size = sizeof(Entity);
  game.max_edicts = kMostEntities;
  game.num_edicts = max_clients + 1;
  for (int i = 0; i < max_clients; i++) entities[i + 1].head.client = &clients[i].head;
}

void shutdown() { gi.free_tags(kTagGame); }

// The first deathmatch spot's origin in the entity text.
void read_spot(const char* text) {
  const char* at = std::strstr(text, "info_player_deathmatch");
  if (at == nullptr) return;
  const char* block = at;
  while (block > text && *block != '{') block--;
  const char* origin = std::strstr(block, "\"origin\"");
  if (origin == nullptr) return;
  std::sscanf(origin + 8, " \"%f %f %f\"", &spot[0], &spot[1], &spot[2]);
}

void spawn_entities(const char*, char* text, const char* point) {
  std::snprintf(spawn_point, sizeof spawn_point, "%s", point != nullptr ? point : "");
  for (int i = max_clients + 1; i < kMostEntities; i++)
    std::memset(&entities[i], 0, sizeof(Entity));
  for (int i = 1; i <= max_clients; i++)
    entities[i].head.inuse = 0;  // every body put back at a spot
  entities[0].head.inuse = 1;
  read_spot(text);
  gi.configstring(2, "sky1");
  gi.configstring(5, "yb -24 xv 0 hnum");  // the game's own bar: its health number
  spark_sound = gi.soundindex("misc/spark.wav");
  Entity& plat = entities[max_clients + 1];
  if (std::strstr(text, "\"*1\"") != nullptr) {  // a brush entity the map names
    plat.head.inuse = 1;
    plat.head.solid = kSolidBsp;
    gi.setmodel(&plat.head, "*1");
    gi.linkentity(&plat.head);
  }
  Entity& box = entities[max_clients + 2];
  box.head.inuse = 1;
  box.head.solid = kSolidBox;
  box.head.s.model[0] = gi.modelindex("models/thing.md2");
  box.head.s.origin[0] = spot[0] + 96;
  box.head.s.origin[1] = spot[1];
  box.head.s.origin[2] = spot[2];
  const float lo[3] = {-16, -16, -24}, hi[3] = {16, 16, 32};
  std::memcpy(box.head.mins, lo, sizeof lo);
  std::memcpy(box.head.maxs, hi, sizeof hi);
  box.health = 50;
  gi.linkentity(&box.head);
  game.num_edicts = max_clients + 3;
}

int client_connect(NativeEntityHead*, char* userinfo) {
  if (std::strstr(userinfo, "\\name\\refuse") != nullptr) {
    const size_t used = std::strlen(userinfo);
    std::snprintf(userinfo + used, 512 - used, "\\rejmsg\\Go away");
    return 0;
  }
  return 1;
}

void client_userinfo_changed(NativeEntityHead* ent, char* userinfo) {
  const char* at = std::strstr(userinfo, "\\name\\");
  char name[32] = "player";
  if (at != nullptr) std::sscanf(at + 6, "%31[^\\]", name);
  char skin[64];
  std::snprintf(skin, sizeof skin, "%s\\male/grunt", name);
  gi.configstring(1312 + static_cast<int>(reinterpret_cast<Entity*>(ent) - entities) - 1, skin);
}

void client_begin(NativeEntityHead* ent) {
  came_in = ent->inuse != 0 ? "body" : "spot";
  reinterpret_cast<Client*>(ent->client)->health = 100;
  if (ent->inuse != 0) {  // his body from a loaded level, where he left it
    gi.linkentity(ent);
    return;
  }
  ent->inuse = 1;
  ent->solid = kSolidBox;
  ent->s.model[0] = 255;
  const float lo[3] = {-16, -16, -24}, hi[3] = {16, 16, 32};
  std::memcpy(ent->mins, lo, sizeof lo);
  std::memcpy(ent->maxs, hi, sizeof hi);
  std::memcpy(ent->s.origin, spot, sizeof spot);
  NativeMoveState& m = ent->client->ps.move;
  std::memset(&m, 0, sizeof m);
  for (int k = 0; k < 3; k++) m.origin[k] = static_cast<int16_t>(spot[k] * 8);
  m.gravity = 800;
  ent->client->ps.fov = 90;
  ent->client->ps.gun_index = gi.modelindex("models/weapons/v_blast/tris.md2");  // the first gun
  ent->client->ps.gun_frame = 3;
  gi.linkentity(ent);
  gi.bprintf(2, "player %d entered the game\n",
             static_cast<int>(reinterpret_cast<Entity*>(ent) - entities));
}

void client_disconnect(NativeEntityHead* ent) {
  gi.unlinkentity(ent);
  ent->inuse = 0;
  ent->s.model[0] = 0;
}

void client_think(NativeEntityHead* ent, NativeCmd* cmd) {
  NativeMoveBlock pm;
  std::memset(&pm, 0, sizeof pm);
  pm.s = ent->client->ps.move;
  pm.cmd = *cmd;
  pm.trace = trace_cb;
  pm.pointcontents = contents_cb;
  mover = ent;
  gi.pmove(&pm);
  ent->client->ps.move = pm.s;
  for (int k = 0; k < 3; k++) {
    ent->s.origin[k] = pm.s.origin[k] * 0.125f;
    ent->client->ps.view_angles[k] = pm.view_angles[k];
  }
  ent->client->ps.view_offset[2] = pm.view_height;
  gi.linkentity(ent);
  if ((cmd->buttons & 1) != 0) {
    gi.write_byte(3);  // a temporary entity
    gi.write_byte(0);  // a spark
    gi.write_position(ent->s.origin);
    const float up[3] = {0, 0, 1};
    gi.write_dir(up);
    gi.multicast(ent->s.origin, 2);
    gi.sound(ent, 1, spark_sound, 1, 1, 0);
    ent->s.event = 1;
  }
}

void client_command(NativeEntityHead* ent) {
  const char* w = gi.argv(0);
  if (std::strcmp(w, "score") == 0) {
    gi.cprintf(ent, 2, "your score: %d (%s)\n", reinterpret_cast<Client*>(ent->client)->score,
               gi.args());
  } else if (std::strcmp(w, "crash") == 0) {
    gi.error("asked to stop by %s", w);
  } else if (std::strcmp(w, "layout") == 0) {
    gi.write_byte(4);  // a layout program for this player alone
    gi.write_string("xv 32 yv 8 string \"fixture scores\"");
    gi.unicast(ent, 1);
  } else if (std::strcmp(w, "turn") == 0) {  // the platform turned to a yaw about its origin
    Entity& plat = entities[max_clients + 1];
    plat.head.s.angles[1] = static_cast<float>(std::atof(gi.argv(1)));
    gi.linkentity(&plat.head);
  } else if (std::strcmp(w, "probe") == 0) {  // a point traced down at x y: where it stops
    const float x = static_cast<float>(std::atof(gi.argv(1)));
    const float y = static_cast<float>(std::atof(gi.argv(2)));
    const float a[3] = {x, y, 64}, b[3] = {x, y, 4}, in[3] = {x, y, 4};
    const NativeTrace t = gi.trace(a, nullptr, nullptr, b, nullptr, kMaskPlayer);
    gi.cprintf(ent, 2, "probe z %d up %d inside %d\n", static_cast<int>(t.endpos[2]),
               static_cast<int>(t.plane.normal[2] * 100), gi.pointcontents(in));
  } else if (std::strcmp(w, "next") == 0) {
    gi.add_command_string("gamemap \"hall2\"\n");
  } else if (std::strcmp(w, "go") == 0) {
    char line[128];
    std::snprintf(line, sizeof line, "gamemap \"%s\"\n", gi.argv(1));
    gi.add_command_string(line);
  } else if (std::strcmp(w, "hit") == 0) {
    entities[max_clients + 2].health -= 10;
  } else if (std::strcmp(w, "die") == 0) {
    reinterpret_cast<Client*>(ent->client)->health = 0;
    gi.add_command_string("menu_loadgame\n");
  } else if (std::strcmp(w, "where") == 0) {
    gi.cprintf(ent, 2, "where '%s' box %d dm %d came %s autosaved %d\n", spawn_point,
               entities[max_clients + 2].health, static_cast<int>(deathmatch->value), came_in,
               autosaved);
  }
}

void run_frame() {
  frames++;
  for (int i = 0; i < max_clients; i++) {
    Entity& e = entities[i + 1];
    if (e.head.inuse == 0) continue;
    e.head.client->ps.stats[1] =
        static_cast<int16_t>(reinterpret_cast<Client*>(e.head.client)->health);
    e.head.client->ps.stats[14] = static_cast<int16_t>(frames);
  }
  for (int n = 0; n < game.num_edicts; n++)
    std::memcpy(entities[n].head.s.old_origin, entities[n].head.s.origin, sizeof(float) * 3);
}

void server_command() {
  if (std::strcmp(gi.argv(1), "hello") == 0) gi.dprintf("hello from the game (%d)\n", gi.argc());
}

// The game file: whether it was an autosave.
void write_game(const char* file, int autosave) {
  FILE* f = std::fopen(file, "w");
  if (f == nullptr) {
    gi.error("cannot write %s", file);
    return;
  }
  std::fprintf(f, "%d\n", autosave);
  std::fclose(f);
}

void read_game(const char* file) {
  FILE* f = std::fopen(file, "r");
  if (f == nullptr) {
    gi.error("cannot read %s", file);
    return;
  }
  if (std::fscanf(f, "%d", &autosaved) != 1) autosaved = -1;
  std::fclose(f);
}

// The level file: the box's health, then each player's body (used or not, and where).
void write_level(const char* file) {
  FILE* f = std::fopen(file, "w");
  if (f == nullptr) {
    gi.error("cannot write %s", file);
    return;
  }
  std::fprintf(f, "%d\n", entities[max_clients + 2].health);
  for (int i = 1; i <= max_clients; i++) {
    const NativeEntityHead& e = entities[i].head;
    std::fprintf(f, "%d %g %g %g\n", e.inuse, e.s.origin[0], e.s.origin[1], e.s.origin[2]);
  }
  std::fclose(f);
}

void read_level(const char* file) {
  FILE* f = std::fopen(file, "r");
  if (f == nullptr) {
    gi.error("cannot read %s", file);
    return;
  }
  if (std::fscanf(f, "%d", &entities[max_clients + 2].health) != 1) gi.error("bad level file");
  for (int i = 1; i <= max_clients; i++) {
    NativeEntityHead& e = entities[i].head;
    float o[3] = {0, 0, 0};
    if (std::fscanf(f, "%d %f %f %f", &e.inuse, &o[0], &o[1], &o[2]) != 4)
      gi.error("bad level file");
    if (e.inuse == 0) continue;
    std::memcpy(e.s.origin, o, sizeof o);
    for (int k = 0; k < 3; k++) e.client->ps.move.origin[k] = static_cast<int16_t>(o[k] * 8);
  }
  std::fclose(f);
  gi.linkentity(&entities[max_clients + 2].head);
}

}  // namespace

FIXTURE_EXPORT NativeExports* GetGameAPI(NativeImports* imports) {
  gi = *imports;
  game = NativeExports{};
  game.apiversion = kNativeVersion;
  game.init = init;
  game.shutdown = shutdown;
  game.spawn_entities = spawn_entities;
  game.write_game = write_game;
  game.read_game = read_game;
  game.write_level = write_level;
  game.read_level = read_level;
  game.client_connect = client_connect;
  game.client_begin = client_begin;
  game.client_userinfo_changed = client_userinfo_changed;
  game.client_disconnect = client_disconnect;
  game.client_command = client_command;
  game.client_think = client_think;
  game.run_frame = run_frame;
  game.server_command = server_command;
  return &game;
}
