// The store's writer thread (store_writer.hpp).
//
// ge:summary The save store's writer thread: the queue, the done events, flush and join
// ge:keywords store-writer thread queue done flush
// ge:skill save
// ge:test tests/save_store_test.cpp
#include "save/store_writer.hpp"

namespace ge::save {

StoreWriter::StoreWriter(Disk& disk, StoreSpec spec)
    : store_(disk, std::move(spec)), thread_([this] { run(); }) {}

StoreWriter::~StoreWriter() {
  {
    std::lock_guard<std::mutex> lock(queue_mutex_);
    stop_ = true;
  }
  wake_.notify_all();
  thread_.join();
}

bool StoreWriter::open(StoreOpen* report, std::string& problem) {
  std::lock_guard<std::mutex> lock(store_mutex_);
  const bool ok = store_.open(report);
  problem = store_.problem();
  return ok;
}

uint64_t StoreWriter::post(std::string slot, std::vector<uint8_t> file) {
  std::lock_guard<std::mutex> lock(queue_mutex_);
  const uint64_t id = next_id_++;
  jobs_.push_back({id, std::move(slot), std::move(file), 0});
  wake_.notify_one();
  return id;
}

uint64_t StoreWriter::post_autosave(std::vector<uint8_t> file, double now_s) {
  std::lock_guard<std::mutex> lock(queue_mutex_);
  const uint64_t id = next_id_++;
  jobs_.push_back({id, "", std::move(file), now_s});
  wake_.notify_one();
  return id;
}

std::vector<StoreDone> StoreWriter::poll() {
  std::lock_guard<std::mutex> lock(queue_mutex_);
  std::vector<StoreDone> out;
  out.swap(done_);
  return out;
}

void StoreWriter::flush() {
  std::unique_lock<std::mutex> lock(queue_mutex_);
  idle_.wait(lock, [this] { return jobs_.empty() && !busy_; });
}

void StoreWriter::run() {
  for (;;) {
    Job job;
    {
      std::unique_lock<std::mutex> lock(queue_mutex_);
      wake_.wait(lock, [this] { return stop_ || !jobs_.empty(); });
      if (jobs_.empty()) return;  // stopped with nothing left
      job = std::move(jobs_.front());
      jobs_.pop_front();
      busy_ = true;
    }
    StoreDone d;
    d.id = job.id;
    {
      std::lock_guard<std::mutex> lock(store_mutex_);
      if (job.slot.empty()) {
        d.slot = store_.autosave_slot();
        d.skipped = !store_.autosave_due(job.now_s);
        d.ok = store_.autosave(job.file, job.now_s);
      } else {
        d.slot = job.slot;
        d.ok = store_.write(job.slot, job.file);
      }
      if (!d.ok) d.problem = store_.problem();
    }
    {
      std::lock_guard<std::mutex> lock(queue_mutex_);
      done_.push_back(std::move(d));
      busy_ = false;
    }
    idle_.notify_all();
  }
}

}  // namespace ge::save
