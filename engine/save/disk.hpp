// WHERE A STORE'S FILES LIVE (docs/saves.md § Slots and the store): the few operations a save
// store needs from a disk, on one flat directory of files named by the store, so the store's
// rules are written once and run on a real directory or on a simulated one.
//
// - `DirDisk`: a directory of the platform's. A write creates or truncates the file, writes every
//   byte and flushes it to the device (fsync on POSIX, FlushFileBuffers on Windows) before it
//   answers; a rename replaces its target in one step (rename(2), MoveFileEx with
//   MOVEFILE_REPLACE_EXISTING); `sync` flushes the directory itself so a rename survives power loss
//   (a no-op where the platform has no such call).
// - `SimDisk`: files in memory with a BUDGET: each byte written and each rename, remove and flush
//   costs one unit, and when the budget runs out mid-operation the disk dies there (a write leaves
//   the bytes written so far, every later call fails) as a machine that lost power would. A lab
//   kills a store's write at every offset this way, then opens the files left with a new store.
//   It counts the reads and the bytes read, so a test can say a listing read one file.
//
// Names are plain file names inside the directory (no separators); every call answers false with
// nothing changed past the point it failed.
//
// ge:summary The disk a save store writes: a real directory flushed to the device,
// ge:summary or a simulated one that dies mid-write
// ge:keywords save store disk directory fsync flush rename atomic remove list sim-disk budget
// ge:keywords power-loss kill crash-test dir-disk read-count
// ge:skill save
// ge:test tests/save_store_test.cpp
#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <limits>
#include <map>
#include <span>
#include <string>
#include <vector>

namespace ge::save {

struct DiskFile {
  std::string name;
  uint64_t bytes = 0;
};

class Disk {
 public:
  virtual ~Disk() = default;
  // The first `most` bytes of `name` (all of it by default) into `out`. False when it is missing.
  virtual bool read(const std::string& name, std::vector<uint8_t>& out,
                    size_t most = std::numeric_limits<size_t>::max()) = 0;
  // `name` made to hold `bytes`, flushed to the device.
  virtual bool write(const std::string& name, std::span<const uint8_t> bytes) = 0;
  // `from` renamed to `to`, replacing it.
  virtual bool rename(const std::string& from, const std::string& to) = 0;
  virtual bool remove(const std::string& name) = 0;
  // The directory flushed, so the renames before it survive a power loss.
  virtual bool sync() = 0;
  // Every file, by name, with its size.
  virtual std::vector<DiskFile> list() = 0;
  // Why the last call failed ("" when it did not).
  virtual const std::string& problem() const = 0;
};

class DirDisk final : public Disk {
 public:
  // The directory, made (with its parents) when missing.
  explicit DirDisk(std::filesystem::path dir);
  bool read(const std::string& name, std::vector<uint8_t>& out, size_t most) override;
  using Disk::read;
  bool write(const std::string& name, std::span<const uint8_t> bytes) override;
  bool rename(const std::string& from, const std::string& to) override;
  bool remove(const std::string& name) override;
  bool sync() override;
  std::vector<DiskFile> list() override;
  const std::string& problem() const override { return problem_; }
  const std::filesystem::path& dir() const { return dir_; }

 private:
  bool fail(const std::string& what);
  std::filesystem::path dir_;
  std::string problem_;
};

class SimDisk final : public Disk {
 public:
  bool read(const std::string& name, std::vector<uint8_t>& out, size_t most) override;
  using Disk::read;
  bool write(const std::string& name, std::span<const uint8_t> bytes) override;
  bool rename(const std::string& from, const std::string& to) override;
  bool remove(const std::string& name) override;
  bool sync() override;
  std::vector<DiskFile> list() override;
  const std::string& problem() const override { return problem_; }

  // The units left before the disk dies (unlimited by default), and whether it has.
  int64_t budget = std::numeric_limits<int64_t>::max();
  bool dead = false;
  int64_t spent = 0;  // units spent so far
  int64_t reads = 0;  // files read
  uint64_t bytes_read = 0;
  std::map<std::string, std::vector<uint8_t>> files;

  // A disk holding what this one holds, alive, with no budget and no counts: the machine back on.
  SimDisk restarted() const;

 private:
  bool spend(int64_t units);
  bool fail(const std::string& what);
  std::string problem_;
};

}  // namespace ge::save
