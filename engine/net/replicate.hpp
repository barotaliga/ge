// REPLICATION, THE SERVER'S WORD: the server steps the world and sends each client SNAPSHOTS of
// what it may see; each client predicts its own entity from its own inputs and draws everyone
// else a little in the past, interpolated between snapshots. It is the model of a fast action
// game, a survival sandbox, an open world, and Fall Line's free ride (docs/networking.md).
//
// THE WORLD is entities, each a row of up to kMaxFields numbers the game names in a `Schema`:
// each field quantized to `bits` over [min, max] (an angle wraps), so a snapshot carries bits,
// not doubles. The server keeps every entity's state QUANTIZED after each tick, so a client that
// replays the same inputs from a snapshot reaches the server's own doubles: with no loss, a
// prediction never needs correcting.
//
// SNAPSHOTS go on an unreliable channel every `send_every` ticks, each a DELTA against the newest
// snapshot the client has acked (each field that changed, bit-packed), or whole when that one has
// fallen out of the ring. Each client is sent only the entities INTEREST lets it see (its own
// always; others within `interest_radius` of it on two fields, and as `visible` allows), the
// stalest first when they do not all fit in `snapshot_budget` (a row's priority is how many ticks
// the client's copy of it is behind). A client acks the newest snapshot it decoded on every input.
//
// INPUTS go up an unreliable channel every tick, each carrying the last `redundancy` inputs so a
// lost packet costs nothing; the server applies one a tick in order through the game's `step`
// (the same function the client predicts with) and says which it applied last. A tick with no
// input waiting steps the last one again (the queue then holds one more: a buffer for the link's
// jitter); inputs lost past the redundancy are taken to be the one before them, less those the
// repeats already stood in for, so the server steps a client's entity as often as the client did.
// The client PREDICTS its entity by replaying every input the server has not yet applied over the
// newest snapshot; a correction is shown smoothly, the error decaying by `smoothing` a tick.
//
// OTHERS are drawn `interp_ticks` behind the server's newest tick the client has heard of,
// interpolated between the two snapshots around that moment. LAG COMPENSATION: each input names
// the tick the client was drawing, and `replicate_rewind` returns any entity's state at that tick
// from the server's history, so a hit is judged against what the player saw.
//
// Like the node, nothing here reads a clock or draws randomness; every call takes its tick, and
// the whole exchange over the simulated network replays to the bit.
//
//   server: replicate_server_init(s, schema, config, node)   spawn, set, despawn entities
//           every tick: replicate_server_receive (after node_update), the game's own moves,
//                       replicate_server_tick (steps the owned entities, records, sends)
//   client: replicate_client_init(c, schema, config, node, server_peer)
//           every tick: replicate_client_receive (after node_update), replicate_client_input
//           drawing: replicate_client_predicted, replicate_client_sample
//
// ge:summary Replication: snapshots and deltas, interest, prediction, interpolation, rewind
// ge:keywords net replicate replication snapshot delta quantize interest prediction reconcile
// ge:keywords interpolation lag-compensation rewind server-authoritative input redundancy
// ge:keywords priority bandwidth entity schema
// ge:skill net
// ge:test tests/net_replicate_test.cpp
#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

#include "net/node.hpp"

namespace ge::net {

constexpr int kMaxFields = 16;
constexpr uint16_t kNoEntity = 0xffff;
constexpr int kMaxInputSize = 32;

struct FieldSpec {
  double min = 0, max = 1;
  uint8_t bits = 16;   // 1..32
  bool angle = false;  // wraps over [min, max): interpolated the short way round
};

// The game's step for an owned entity: its state (the schema's fields) moved by one tick's input.
// The server and the predicting client call the same one, so it must be deterministic.
using StepFn = void (*)(void* ctx, double* state, const uint8_t* input);
// Whether `client` (a peer slot) may see entity `index`; null: interest_radius alone decides.
using VisibleFn = bool (*)(void* ctx, int client, int index);

struct Schema {
  int fields = 0;
  FieldSpec spec[kMaxFields];
  int input_size = 0;  // bytes an input carries, at most kMaxInputSize
  StepFn step = nullptr;
  void* ctx = nullptr;
};

struct ReplicateConfig {
  int snapshot_channel = 0;  // unreliable
  int input_channel = 1;     // unreliable
  int max_entities = 128;
  int send_every = 2;  // a snapshot every this many ticks
  int ring = 32;       // snapshots remembered per client, and ticks of history for rewind
  int redundancy = 4;  // inputs repeated in every input message
  int interest_x = -1, interest_z = -1;  // the fields interest measures distance on (-1: none)
  double interest_radius = 0;            // 0: everyone sees everything
  VisibleFn visible = nullptr;
  void* visible_ctx = nullptr;
  int interp_ticks = 6;        // how far behind the newest snapshot others are drawn
  double smoothing = 0.85;     // a correction's error left after each tick
  size_t snapshot_budget = 0;  // bytes a snapshot may take (0: the largest message)
};

struct ReplicateStats {
  uint64_t snapshots = 0, full = 0, bytes = 0;  // sent (server) or decoded (client)
  uint64_t dropped = 0;               // client: snapshots whose baseline was gone, or stale
  uint64_t starved = 0;               // server: entities left out of a snapshot for its budget
  uint64_t inputs = 0, repeated = 0;  // server: inputs applied, ticks that reused the last one
  uint64_t corrections = 0;           // client: snapshots that moved its prediction
};

// One entity's quantized row.
struct Row {
  uint16_t id = kNoEntity;
  uint32_t fresh = 0;  // server: the tick this value was current (a starved row keeps its own)
  uint32_t q[kMaxFields] = {};
};

// A snapshot as a client was sent it (server) or decoded it (client): its tick and its rows.
struct Snap {
  uint32_t tick = 0;
  bool valid = false;
  std::vector<Row> rows;  // by id, ascending
};

struct ReplicateClientSlot {
  bool active = false;
  uint16_t entity = kNoEntity;  // the entity its inputs move
  std::vector<Snap> sent;       // the ring of what it was sent
  uint32_t acked = 0;           // the newest snapshot it decoded (0: none yet)
  bool has_ack = false;
  std::vector<uint8_t> inputs;  // queued inputs, input_size each, oldest first
  std::vector<uint32_t>
      queue_seq;          // their numbers, ascending (a gap: inputs the repeats stood in for)
  uint32_t next_seq = 1;  // the next input it is owed while the queue is empty
  uint32_t applied = 0;   // the last input applied
  uint32_t repeats = 0;   // ticks that stepped the last input again, not yet set off
  uint8_t last[kMaxInputSize] = {};
  uint32_t view_tick = 0;        // the tick it said it was drawing (x256)
  std::vector<double> priority;  // per entity index: its place in the last snapshot's order
};

struct ReplicateServer {
  Schema schema;
  ReplicateConfig config;
  Node* node = nullptr;
  uint32_t tick = 0;
  uint16_t next_id = 0;
  std::vector<uint16_t> ids;      // per index: the entity's id, kNoEntity when free
  std::vector<int> owner;         // per index: the peer whose inputs move it, -1: the game
  std::vector<int> order;         // scratch: a snapshot's entities by priority
  std::vector<double> state;      // per index: fields doubles (quantized values)
  std::vector<uint32_t> history;  // ring x max_entities x fields, quantized
  std::vector<uint32_t> history_tick;
  std::vector<uint8_t> history_alive;        // ring x max_entities
  std::vector<ReplicateClientSlot> clients;  // per peer slot
  ReplicateStats stats;
};

void replicate_server_init(ReplicateServer& s, const Schema& schema, const ReplicateConfig& config,
                           Node& node);
// A new entity, moved by `owner`'s inputs (a peer slot) or by the game (-1); its id, or
// kNoEntity when the table is full.
uint16_t replicate_spawn(ReplicateServer& s, int owner, const double* values);
void replicate_set(ReplicateServer& s, uint16_t id, const double* values);
const double* replicate_get(const ReplicateServer& s, uint16_t id);
void replicate_despawn(ReplicateServer& s, uint16_t id);
// Read the clients' inputs and acks out of the node's events; a peer gone frees its slot.
void replicate_server_receive(ReplicateServer& s);
// Step every owned entity by its client's next input, record the tick, send what is due.
void replicate_server_tick(ReplicateServer& s);
// Entity `id`'s state at `tick` (fractional, x256) from the history; false when out of the ring.
bool replicate_rewind(const ReplicateServer& s, uint16_t id, uint32_t tick_x256, double* out);

struct ReplicateClient {
  Schema schema;
  ReplicateConfig config;
  Node* node = nullptr;
  int server = -1;
  std::vector<Snap> ring;  // decoded snapshots, by tick
  uint32_t newest = 0;     // the newest snapshot decoded
  bool has_snapshot = false;
  uint16_t own = kNoEntity;
  uint32_t applied = 0;          // the server's last applied input, as of the newest snapshot
  uint32_t seq = 0;              // the last input made
  std::vector<uint8_t> history;  // inputs by seq, ring x input_size
  double predicted[kMaxFields] = {};
  double error[kMaxFields] = {};  // a correction still being smoothed away
  double last_error = 0;          // the size of the last correction (the schema's units)
  uint32_t local_tick = 0;        // ticks since the newest snapshot arrived
  bool predicting = false;        // `predicted` holds a reconciled state to step on from
  std::vector<Row> scratch;       // a snapshot being decoded
  ReplicateStats stats;
};

void replicate_client_init(ReplicateClient& c, const Schema& schema, const ReplicateConfig& config,
                           Node& node, int server_peer);
// Decode the snapshots in the node's events and reconcile the prediction with the newest.
void replicate_client_receive(ReplicateClient& c);
// This tick's input: predicted at once and sent with the last `redundancy` (call each tick);
// ignored until the first snapshot has arrived.
void replicate_client_input(ReplicateClient& c, const uint8_t* input);
// The own entity as drawn: the prediction plus what is left of the last correction.
void replicate_client_predicted(const ReplicateClient& c, double* out);
// The tick others are drawn at, x256: interp_ticks behind the newest heard of.
uint32_t replicate_client_render_tick(const ReplicateClient& c);
// Entity `id` at the render tick, interpolated; false when it is in no snapshot around it.
bool replicate_client_sample(const ReplicateClient& c, uint16_t id, double* out);

// The schema's quantization, exposed for tests and tools.
uint32_t quantize(const FieldSpec& f, double v);
double dequantize(const FieldSpec& f, uint32_t q);

}  // namespace ge::net
