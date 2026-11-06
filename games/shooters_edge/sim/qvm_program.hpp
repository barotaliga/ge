// A COMPILED MODULE DRIVES THE LOOP: the adapter between the shooter loop's program seam
// (plugins/shooter/program.hpp) and the compiled-module VM (plugins/progs/qvm.hpp), the module
// form's counterpart of qc_program.hpp. The module keeps its entities and globals INSIDE its own
// memory, at the addresses its init export hands back (the GAME DATA BLOCK: the entity records'
// base and stride, the globals' address, the extension field table, the interface version and the
// entity limit); the adapter reads and writes them there in place.
//
// - THE RECORD: an entity's record starts with the engine fields in the multiplayer form's word
//   order (progs/layout.hpp's `kMultiplayerLayout.f`: byte offset = word x 4), the module's own
//   data after them; the global block has the multiplayer form's global words the same way. Entity
//   values (self, other, groundentity, owner, ...) are number x the module's stride.
// - THE MIRROR: before each export call every entity in use has its loop fields written into its
//   record (the same list the QC adapter mirrors) and `self`, `other`, `time`, `frametime`,
//   `newmis`, `force_retouch` into the globals; after it, the same are read back. The function
//   fields (`touch`, `think`, `blocked`) are the module's and are never written: the loop gets a
//   TAG from them (`touch` and `blocked` when the module's word is non-zero, `think` always, since
//   a module's think runs whenever `nextthink` comes due), and calling a tag calls the export that
//   dispatches it (touch 15, think 16, blocked 17) with `self` and `other` set.
// - THE HOOKS are tags too: start frame (11 with the server's time in ms), the pre- and post-think
//   (8, 10), connect and put-in-server (3, 4; a spectator's with 1), disconnect (6), the parms
//   (13, 12). The loop's `client_kill` is none: a module answers `kill` as a client command.
// - EXTENSION FIELDS are found by name in the module's field table after init (case-insensitive,
//   the first match): their byte offsets, 0 when absent (the feature off).
// - STRING FIELDS: in REFERENCE MODE (the module turned it on during init) a string field's word
//   is a byte offset from the entity base to the word holding the text's address; otherwise the
//   word is the address itself. `entity_text` reads either, every address checked.
// - A NESTED CALL (a touch a system call's move ran) mirrors the same way and restores the server
//   it found; a system call that reads or moves the world brackets itself with `qvm_flush` (the
//   module's records to the loop) and `qvm_refresh` (the loop back into the records).
// - A FAULT ends the export call (the VM unwinds every nested call), is counted with its text
//   kept, and is cleared: the match goes on with what was written before it.
//
// ge:summary The adapter that lets a compiled module drive the shooter loop's program seam
// ge:keywords qvm compiled-module adapter program-seam mirror game-data-block entity-records
// ge:keywords exports tags think touch blocked reference-mode string-fields extension-fields
// ge:keywords qvm_program qvm_export qvm_flush qvm_refresh entity_text clear-edict free-rule
// ge:skill progs shooter
// ge:test tests/shooters_edge_qvm_server_test.cpp
#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "progs/qvm.hpp"
#include "shooter/program.hpp"
#include "shooter/server.hpp"

namespace shooters_edge::sim {

namespace sh = ge::shooter;

// The exports: the command numbers of the module's entry function.
enum QvmExport : uint8_t {
  kQvmInit = 0,
  kQvmLoadEntities = 1,
  kQvmShutdown = 2,
  kQvmClientConnect = 3,
  kQvmPutClientInServer = 4,
  kQvmUserInfoChanged = 5,
  kQvmClientDisconnect = 6,
  kQvmClientCommand = 7,
  kQvmPreThink = 8,
  kQvmPostThink = 10,
  kQvmStartFrame = 11,
  kQvmSetChangeParms = 12,
  kQvmSetNewParms = 13,
  kQvmConsoleCommand = 14,
  kQvmTouch = 15,
  kQvmThink = 16,
  kQvmBlocked = 17,
  kQvmClientSay = 18,
  kQvmPausedTic = 19,
  kQvmClearEdict = 20,
};
constexpr int kQvmExports = 21;

// The game data block init returns: six words.
struct QvmGameData {
  int32_t ents = 0;         // entity 0's record
  int32_t entity_size = 0;  // the record's stride, bytes
  int32_t globals = 0;      // the global block
  int32_t fields = 0;       // the extension field table
  int32_t api = 0;          // the interface version (16 or 17)
  int32_t max_entities = 0;
};
constexpr int32_t kQvmGlobalBytes = 90 * 4;  // the engine-side global words the server touches

// The extension fields' byte offsets inside a record; 0 when the module has none.
struct QvmFields {
  int32_t vw_index = 0, movement = 0, maxspeed = 0, gravity = 0, isbot = 0, items2 = 0;
  int32_t hideentity = 0, trackent = 0, hideplayers = 0, teleported = 0, visclients = 0;
  int32_t mod_admin = 0;
};

// The loop's function numbers for a module: the tags.
constexpr sh::Func kTagTouch = 1, kTagThink = 2, kTagBlocked = 3;

constexpr int kMaxQvmNesting = 40;

struct QvmProgram {
  ge::progs::Qvm vm;
  QvmGameData data;
  QvmFields fields;
  bool references = false;     // string fields through references (the module's choice at init)
  int max_edicts = 0;          // the pool, at most the block's maximum
  sh::Server* home = nullptr;  // the server it serves (entity numbers are checked against it)
  sh::Server* sv = nullptr;    // the server of the call running now (null between calls)
  sh::Server* outer[kMaxQvmNesting] = {};
  int nesting = 0;
  std::vector<uint8_t> live;  // by entity number: in use in the module's records
  int64_t calls = 0, faults = 0;
  int64_t export_counts[kQvmExports] = {};
  std::string last_fault;
  // The client being set up by a connect: its loop fields get gravity 1 and this maximum speed
  // before its first hook runs (0: none).
  int setting_up = 0;
  float setup_maxspeed = 320;
  // Called after the parms exports (the server saves a client's spawn parms); null for none.
  void (*after_parms)(QvmProgram& q, int self) = nullptr;
  void* owner = nullptr;  // the server's
};

// Reads the game data block at `addr` (init's result) and the extension fields; the pool becomes
// min(`max_edicts`, the block's maximum). False with `error` when the block is refused (an
// interface version outside 16..17, records or globals outside the memory).
bool bind_game_data(QvmProgram& q, int32_t addr, int max_edicts, std::string& error);

// The seam the loop's server is created with.
sh::Program qvm_program(QvmProgram& q);

// One export call from outside the seam: the server mirrored in, the call, mirrored back (a fault
// counted and cleared). `self` and `other` are the server's globals as set.
int32_t qvm_export(QvmProgram& q, sh::Server& sv, int32_t command, int32_t a0 = 0, int32_t a1 = 0,
                   int32_t a2 = 0);
// An export call while one is running (a system call's re-entry): no mirror, the globals as the
// running call has them.
int32_t qvm_reenter(QvmProgram& q, int32_t command, int32_t a0 = 0, int32_t a1 = 0, int32_t a2 = 0);

// Inside a system call: the records to the loop (before reading or moving the world), and the
// loop's fields back into the records (after moving it).
void qvm_flush(QvmProgram& q);
void qvm_refresh(QvmProgram& q);
// The same for one entity in use (setorigin, setsize, setmodel move only theirs).
void qvm_flush_entity(QvmProgram& q, int e);
void qvm_refresh_entity(QvmProgram& q, int e);

// A record's address, a byte offset inside it, an entity value (number x stride) and back. The
// numbers from values and addresses are -1 when they name no entity in use.
inline int32_t record_of(const QvmProgram& q, int e) {
  return q.data.ents + e * q.data.entity_size;
}
inline int32_t entity_value(const QvmProgram& q, int e) { return e * q.data.entity_size; }
int entity_from_value(const QvmProgram& q, int32_t value);
int entity_from_address(const QvmProgram& q, int32_t addr);

// Engine field words of a record (word offsets of the multiplayer form) and global words.
float field_f(const QvmProgram& q, int e, int word);
int32_t field_i(const QvmProgram& q, int e, int word);
sh::Vec3f field_v(const QvmProgram& q, int e, int word);
void set_field_f(QvmProgram& q, int e, int word, float v);
void set_field_i(QvmProgram& q, int e, int word, int32_t v);
void set_field_v(QvmProgram& q, int e, int word, sh::Vec3f v);
// The same at a byte offset (an extension field); nothing when the offset is 0.
float ext_f(const QvmProgram& q, int e, int32_t byte_offset);
void set_ext_f(QvmProgram& q, int e, int32_t byte_offset, float v);
int32_t ext_i(const QvmProgram& q, int e, int32_t byte_offset);
void set_ext_i(QvmProgram& q, int e, int32_t byte_offset, int32_t v);
float global_f(const QvmProgram& q, int word);
int32_t global_i(const QvmProgram& q, int word);
void set_global_f(QvmProgram& q, int word, float v);
void set_global_i(QvmProgram& q, int word, int32_t v);
void set_global_v(QvmProgram& q, int word, sh::Vec3f v);
// An entity-valued global's number (the world for a value naming nothing).
int global_entity(const QvmProgram& q, int word);

// A string field's text by either encoding ("" for none or a bad address).
std::string_view entity_text(const QvmProgram& q, int e, int word);
// The word a string field's text address lives in (the field itself, or the word its reference
// names); -1 when the reference is out of range.
int32_t text_slot(const QvmProgram& q, int e, int word);

// Inside a call: a new entity in the loop, its record zeroed and the module's clear edict run on
// it (-1 when the pool is full); an entity cleared for reuse (a client's set-up); an entity freed
// with the free rule in its record.
int qvm_spawn_entity(QvmProgram& q);
void qvm_clear_entity(QvmProgram& q, int e);
void qvm_free_entity(QvmProgram& q, int e);

}  // namespace shooters_edge::sim
