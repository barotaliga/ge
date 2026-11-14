// THE STORE'S WRITER THREAD (docs/saves.md § Slots and the store): the game thread only fills a
// buffer (`Run::save`) and posts it; a thread of the writer's own does the disk (store.hpp's
// safe write, the index), so a save never stalls a frame. Each post is answered by a DONE event
// once the save is safe on the disk (or why it is not), which the game turns into its "saved" mark.
//
// The writer owns its Store: every other call on the store (the list a menu shows, a load, a
// delete) goes through `with`, which waits for a write in progress and runs on the caller's
// thread. Posts are written in order; `flush` waits for all of them; the destructor flushes and
// joins. An autosave posted within the store's spacing of the last is answered at once as done and
// `skipped`.
//
// ge:summary The save store's writer thread: posts written in order off the game thread,
// ge:summary done events
// ge:keywords save store writer thread background post done event saved-mark flush queue
// ge:skill save
// ge:test tests/save_store_test.cpp
#pragma once

#include <condition_variable>
#include <cstdint>
#include <deque>
#include <mutex>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include "save/store.hpp"

namespace ge::save {

struct StoreDone {
  uint64_t id = 0;   // what `post` answered
  std::string slot;  // where it went
  bool ok = false;
  bool skipped = false;  // an autosave within the spacing: nothing written
  std::string problem;   // why not, when not ok
};

class StoreWriter {
 public:
  StoreWriter(Disk& disk, StoreSpec spec);
  ~StoreWriter();
  StoreWriter(const StoreWriter&) = delete;
  StoreWriter& operator=(const StoreWriter&) = delete;

  // The store opened (on the caller's thread). False with the reason.
  bool open(StoreOpen* report, std::string& problem);

  // `file` written into `slot` off this thread; the answer's id.
  uint64_t post(std::string slot, std::vector<uint8_t> file);
  // `file` written into the autosave ring at play time `now_s`, unless one was within the spacing.
  uint64_t post_autosave(std::vector<uint8_t> file, double now_s);

  // The writes finished since the last poll, oldest first.
  std::vector<StoreDone> poll();
  // Waits until every post so far is written.
  void flush();

  // `fn(store)` with no write running.
  template <class F>
  auto with(F&& fn) {
    std::lock_guard<std::mutex> lock(store_mutex_);
    return fn(store_);
  }

 private:
  struct Job {
    uint64_t id = 0;
    std::string slot;  // "" for an autosave
    std::vector<uint8_t> file;
    double now_s = 0;
  };
  void run();
  Store store_;
  std::mutex store_mutex_;  // held while the store is used
  std::mutex queue_mutex_;
  std::condition_variable wake_, idle_;
  std::deque<Job> jobs_;
  std::vector<StoreDone> done_;
  uint64_t next_id_ = 1;
  bool busy_ = false, stop_ = false;
  std::thread thread_;
};

}  // namespace ge::save
