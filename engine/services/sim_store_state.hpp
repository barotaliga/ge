// The simulated store's state, shared by its two halves: the calls (sim_store.cpp) and saving
// and loading it (sim_store_io.cpp). Not installed; nothing else includes it.
//
// ge:summary The simulated store's state: players, boards, blobs, the event queues, the journal
// ge:keywords services sim store state impl player board entry blob pending queue journal
// ge:keywords workshop item download install
// ge:skill services
// ge:test tests/services_sim_store_test.cpp
#pragma once

#include <cstdint>
#include <map>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "core/prng.hpp"
#include "services/sim_store.hpp"

namespace ge::services {

struct SimPending {
  int64_t due_us = 0;
  uint64_t seq = 0;  // ties at one time go in the order they were queued
  Event event;
};

struct SimInvite {
  UserId from = 0;
  std::string connect;
};

// A workshop item as the store keeps it: its latest version's files.
struct SimItem {
  ItemId id = 0;
  UserId author = 0;
  ItemDraft draft;
  uint32_t version = 0;
  std::vector<ItemFile> files;
};

// An item version on its way to a player's machine, landing at `due_us`.
struct SimDownload {
  int64_t due_us = 0;
  ItemId item = 0;
  uint32_t version = 0;
  std::vector<ItemFile> files;
};

struct SimInstall {
  uint32_t version = 0;
  std::vector<ItemFile> files;
};

struct SimPlayer {
  UserId id = 0;
  std::string name, language, country;
  bool online = true;
  Environment environment;
  std::vector<UserId> friends;
  std::map<AppId, bool> licences;     // app -> installed
  std::vector<double> stats;          // local, by catalog index
  std::vector<double> stats_stored;   // what the store took
  std::vector<bool> achieved;         // local
  std::vector<bool> achieved_stored;  // what the store took
  std::map<std::string, std::vector<uint8_t>> cloud;
  std::map<std::string, std::string> presence;
  std::vector<SimInvite> invites;  // received, latest last
  bool hooked = false;
  int screenshots = 0;
  Result fail_next = Result::ok;  // `ok`: no failure staged
  int64_t now_us = 0;             // the player's last poll
  std::vector<SimPending> queue;
  RequestId next_request = 1;
  std::vector<ItemId> subscriptions;
  std::map<ItemId, SimInstall> installs;  // what is on the player's machine
  std::vector<SimDownload> downloads;     // in flight
  uint32_t keyboard_max = 0;              // the open keyboard's limit, 0 when none is open
  bool asleep = false;
  int64_t asleep_at_us = 0;
  std::vector<TimelineMark> timeline;  // every mark, as the game made it
  TimelineMode mode = TimelineMode::menus;
  std::vector<UserId> remote_invited;
  std::vector<RemotePlayer> remote;  // in the player's game now
};

struct SimEntry {
  UserId user = 0;
  int32_t score = 0;
  uint64_t seq = 0;  // when it was set: the earlier of two equal scores ranks first
  std::vector<int32_t> details;
  BlobId blob = 0;
};

struct SimStore::Impl {
  Catalog catalog;
  SimConfig config;
  core::Rng rng;
  std::vector<SimPlayer> players;
  std::vector<std::vector<SimEntry>> boards;  // by catalog index, unsorted
  std::map<BlobId, std::vector<uint8_t>> blobs;
  uint64_t next_seq = 1;
  BlobId next_blob = 1;
  UserId next_user = 1001;
  uint64_t queued = 0;
  std::vector<std::string> journal;
  std::string save_error;      // the last failed save's line, "" after a good one
  std::vector<SimItem> items;  // the workshop, oldest first
  ItemId next_item = 1;

  SimPlayer* find(UserId id);
  const SimPlayer* find(UserId id) const;
  SimItem* find_item(ItemId id);
  const SimItem* find_item(ItemId id) const;
  void fresh_progress(SimPlayer& p) const;  // every stat at its initial value, nothing achieved
  void save_if_asked();

  // What a call to the store's servers answers now: offline, a staged failure, or ok.
  Result server_result(SimPlayer& p);
  // When an answer asked now reaches `p`: the latency and a jitter off the store's stream.
  int64_t answer_time(const SimPlayer& p);
  void queue(SimPlayer& to, Event e, int64_t due);
  // One journal line: the player's time, the call, what it was about, its answer.
  Result log(const SimPlayer& p, std::string_view call, std::string_view what, Result r);
};

// A cloud file's or an item file's name: a relative path of ASCII letters, digits and "._-/".
bool cloud_name_ok(std::string_view name);

// The workshop on the simulated store (sim_store_workshop.cpp), called by its backend.
RequestId workshop_query(SimStore::Impl& s, SimPlayer& p, const ItemQuery& query);
RequestId workshop_submit(SimStore::Impl& s, SimPlayer& p, ItemId item, const ItemDraft& draft,
                          std::span<const ItemFile> files);
RequestId workshop_subscribe(SimStore::Impl& s, SimPlayer& p, ItemId item, bool subscribe);
void workshop_subscribed(const SimStore::Impl& s, const SimPlayer& p, std::vector<ItemInfo>& out);
Result workshop_files(const SimPlayer& p, ItemId item, std::vector<CloudFile>& out);
Result workshop_read(const SimPlayer& p, ItemId item, std::string_view name,
                     std::vector<uint8_t>& out);
// Put every download due by the player's time on their machine.
void workshop_land(SimPlayer& p);

// The handheld and the extras on the simulated store (sim_store_extras.cpp).
Result extras_keyboard(SimStore::Impl& s, SimPlayer& p, KeyboardMode mode, std::string_view prompt,
                       std::string_view initial, uint32_t max_chars);
Result extras_timeline_mark(SimStore::Impl& s, SimPlayer& p, const TimelineMark& mark);
Result extras_timeline_mode(SimStore::Impl& s, SimPlayer& p, TimelineMode mode);
Result extras_invite_remote(SimStore::Impl& s, SimPlayer& p, UserId user);

// The store to and from a directory (sim_store_io.cpp).
bool save_store(const SimStore::Impl& s, const std::string& dir, std::string* error);
bool load_store(SimStore::Impl& s, const std::string& dir, std::string* error);

}  // namespace ge::services
