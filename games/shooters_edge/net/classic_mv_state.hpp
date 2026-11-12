// A MATCH KEPT FROM ITS MULTIVIEW STREAM, SO A LATE VIEWER CAN JOIN (docs/tv.md § The classic
// face): a relay reads every record it forwards, keeps what a viewer who joins now would need to
// see the match as it is (the level's signon, the settings and names that changed since, every
// player and every entity as they stand), and writes that as the viewer's catch-up, so the next
// record it forwards reads on from it.
//
// WHAT IS KEPT, a level at a time (a `serverdata` starts the next and forgets the last):
//
// - The signon as it came: every record before the first with milliseconds that are not 0 (the
//   server data, the lists, the baselines, the statics, the stuffed `fullserverinfo` and `skins`).
//   The baselines are read from it, and the extension words (float coordinates) the level's
//   messages are read under.
// - THE NAME: a relay that sets `name` passes the match on under its own: the server info's
//   `hostname` becomes `<name> (live: <server's>)` and `*qtv` 1, so a viewer's browser and
//   scoreboard tell the relay from the server and a relay downstream knows it is one.
// - The state messages since, the last of each kind and key: light styles by index, the server
//   info by key, each slot's user info (which drops the slot's `setinfo` keys), `setinfo` by slot
//   and key, frags, ping, loss and entry time by slot, the music track, the pause; statics and
//   static sounds after the signon in order.
// - Each player as his last `playerinfo` left him (the recorder's form: a field left out keeps its
//   last value), and whether he was written in this frame or the last (one who was not has left).
// - Each player's stats, from his stats records and the records sent to him alone.
// - The entity list: a full list read against the baselines, a delta list against the list before
//   it (a multiview stream deltas against the last list written, whatever its sequence byte).
//
// THE CATCH-UP (`mv_catch_up`): the signon's records, then read records with time 0 of under
// `kMostCatchUpBody` bytes each: the state messages, every player present in the recorder's form
// with all his fields, a stats record per player, a full entity list from the baselines. A stream
// whose level could not be read (a message no reader knows) has no catch-up past its signon until
// its next level: `broken` says so and why.
//
// ge:summary A match kept from its multiview stream, written as a late viewer's catch-up
// ge:keywords multiview mvd stream-state catch-up late-join relay signon baselines state-messages
// ge:keywords playerinfo packetentities deltapacketentities stats mv_apply mv_catch_up tv-node
// ge:skill net shooter
// ge:test tests/shooters_edge_classic_tv_test.cpp
#pragma once

#include <cstddef>
#include <cstdint>
#include <map>
#include <span>
#include <string>
#include <vector>

#include "shooters_edge/net/classic_svc.hpp"

namespace shooters_edge::net {

// The record kinds of a multiview stream.
inline constexpr int kMvRead = 1, kMvSet = 2, kMvMultiple = 3, kMvSingle = 4, kMvStats = 5;
inline constexpr int kMvAll = 6;
// A record's messages a relay takes at most (a viewer's client needs a margin under 8192), bytes.
inline constexpr size_t kMostRecordBody = 8092;
// The catch-up's own records are cut under this, bytes.
inline constexpr size_t kMostCatchUpBody = 8000;

struct MvRecord {
  int ms = 0, kind = 0, target = 0;
  uint32_t mask = 0;
  size_t body = 0, size = 0;  // the messages' offset in the record and their bytes
  size_t whole = 0;           // the record's bytes
};

enum class MvSplit : uint8_t { more, ok, bad };
// The record at the front of `bytes`: `more` until it is whole, `bad` for a kind outside 1..6 or a
// body over `kMostRecordBody`.
MvSplit mv_record(std::span<const uint8_t> bytes, MvRecord& out);
// A record written onto `out`.
void mv_put_record(std::vector<uint8_t>& out, int ms, int kind, int target, uint32_t mask,
                   std::span<const uint8_t> body);

struct MvPlayer {
  int64_t frame = -1;  // the frame he was last written in (-1: not this level)
  int anim = 0;        // his model's frame
  sh::Vec3f origin, angles;
  int model = 0, skin = 0, effects = 0, weaponframe = 0;
  bool dead = false, gib = false;
};

struct MvState {
  std::string name;          // a relay's own: the server's name rewritten (empty: left as it came)
  bool upstream_tv = false;  // this level's server info came through a relay already (`*qtv`)
  bool level = false;        // a level's signon has begun
  bool signon = false;       // still in it (no frame yet)
  bool broken = false;       // a message could not be read: no catch-up past the signon
  std::string why;           // what broke it
  std::vector<uint8_t> signon_records;
  Ext ext;
  std::vector<EntityState> baselines;                 // by number
  std::vector<EntityState> ents;                      // the list as it stands, by number
  std::map<std::string, std::vector<uint8_t>> keyed;  // a state message's bytes by its key
  std::vector<uint8_t> statics;                       // statics after the signon, as messages
  MvPlayer players[32];
  int32_t stats[32][kStats] = {};
  bool stats_known[32][kStats] = {};
  int64_t frames = 0;  // frames this level
  int64_t records = 0, levels = 0;
};

// One whole record (`mv_record` split it) applied. False, with `broken` set, when its messages
// could not be read; the stream goes on (a relay forwards what it cannot read). `sent`, when not
// null, is the record as a relay passes it on: with `name` set, the server info's `hostname` in
// the stuffed `fullserverinfo` and in every `serverinfo` change is `mv_hostname`'s, and `*qtv` is
// 1; the kept match and its catch-up hold the same. A level that could not be read passes its
// records as they came until the next level.
bool mv_apply(MvState& s, std::span<const uint8_t> record, std::vector<uint8_t>* sent = nullptr);
// A relay's server name: `own (live: theirs)`, `own` alone when the server has none. When the
// stream came through a relay already (`upstream_tv`), that relay's name gives way to this one.
std::string mv_hostname(const std::string& own, const std::string& theirs, bool upstream_tv);
// The catch-up for a viewer who joins now (empty before the first level).
std::vector<uint8_t> mv_catch_up(const MvState& s);

}  // namespace shooters_edge::net
