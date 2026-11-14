// The disk a save store writes (disk.hpp): a real directory and a simulated one.
//
// ge:summary A save store's disk: the directory's calls flushed to the device,
// ge:summary the simulated disk's budget
// ge:keywords dir-disk sim-disk fsync flushfilebuffers movefileex rename budget
// ge:skill save
// ge:test tests/save_store_test.cpp
#include "save/disk.hpp"

#include <algorithm>
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <system_error>

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#else
#include <fcntl.h>
#include <unistd.h>
#endif

namespace ge::save {

namespace fs = std::filesystem;

// --- the directory ---

DirDisk::DirDisk(fs::path dir) : dir_(std::move(dir)) {
  std::error_code ec;
  fs::create_directories(dir_, ec);
  if (ec) fail("the directory " + dir_.string() + " cannot be made: " + ec.message());
}

bool DirDisk::fail(const std::string& what) {
  problem_ = what;
  return false;
}

bool DirDisk::read(const std::string& name, std::vector<uint8_t>& out, size_t most) {
  problem_.clear();
  out.clear();
  const fs::path p = dir_ / name;
  std::FILE* f = std::fopen(p.string().c_str(), "rb");
  if (f == nullptr) return fail(p.string() + " does not open: " + std::strerror(errno));
  uint8_t buf[1 << 16];
  while (out.size() < most) {
    const size_t want = std::min(sizeof buf, most - out.size());
    const size_t got = std::fread(buf, 1, want, f);
    out.insert(out.end(), buf, buf + got);
    if (got < want) break;
  }
  const bool bad = std::ferror(f) != 0;
  std::fclose(f);
  return bad ? fail(p.string() + " did not read to its end") : true;
}

bool DirDisk::write(const std::string& name, std::span<const uint8_t> bytes) {
  problem_.clear();
  const fs::path p = dir_ / name;
#if defined(_WIN32)
  HANDLE h = CreateFileW(p.wstring().c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS,
                         FILE_ATTRIBUTE_NORMAL, nullptr);
  if (h == INVALID_HANDLE_VALUE) return fail(p.string() + " cannot be made");
  size_t at = 0;
  while (at < bytes.size()) {
    const DWORD want = static_cast<DWORD>(std::min<size_t>(bytes.size() - at, 1u << 30));
    DWORD put = 0;
    if (!WriteFile(h, bytes.data() + at, want, &put, nullptr) || put == 0) {
      CloseHandle(h);
      return fail(p.string() + " stopped writing at byte " + std::to_string(at) +
                  " (is the disk full?)");
    }
    at += put;
  }
  const bool flushed = FlushFileBuffers(h) != 0;
  CloseHandle(h);
  if (!flushed) return fail(p.string() + " did not flush to the disk");
  return true;
#else
  const int fd = ::open(p.string().c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
  if (fd < 0) return fail(p.string() + " cannot be made: " + std::strerror(errno));
  size_t at = 0;
  while (at < bytes.size()) {
    const ssize_t put = ::write(fd, bytes.data() + at, bytes.size() - at);
    if (put < 0 && errno == EINTR) continue;
    if (put <= 0) {
      const std::string why = std::strerror(errno);
      ::close(fd);
      return fail(p.string() + " stopped writing at byte " + std::to_string(at) + ": " + why +
                  " (is the disk full?)");
    }
    at += static_cast<size_t>(put);
  }
  const bool flushed = ::fsync(fd) == 0;
  ::close(fd);
  if (!flushed) return fail(p.string() + " did not flush to the disk: " + std::strerror(errno));
  return true;
#endif
}

bool DirDisk::rename(const std::string& from, const std::string& to) {
  problem_.clear();
  const fs::path a = dir_ / from, b = dir_ / to;
#if defined(_WIN32)
  if (!MoveFileExW(a.wstring().c_str(), b.wstring().c_str(),
                   MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
    return fail(a.string() + " cannot be renamed to " + to);
#else
  if (std::rename(a.string().c_str(), b.string().c_str()) != 0)
    return fail(a.string() + " cannot be renamed to " + to + ": " + std::strerror(errno));
#endif
  return true;
}

bool DirDisk::remove(const std::string& name) {
  problem_.clear();
  std::error_code ec;
  if (!fs::remove(dir_ / name, ec) || ec)
    return fail((dir_ / name).string() + " cannot be removed" +
                (ec ? ": " + ec.message() : std::string(" (it is not there)")));
  return true;
}

bool DirDisk::sync() {
  problem_.clear();
#if !defined(_WIN32)
  const int fd = ::open(dir_.string().c_str(), O_RDONLY);
  if (fd < 0) return fail(dir_.string() + " does not open: " + std::strerror(errno));
  const bool ok = ::fsync(fd) == 0;
  ::close(fd);
  if (!ok) return fail(dir_.string() + " did not flush: " + std::strerror(errno));
#endif
  return true;
}

std::vector<DiskFile> DirDisk::list() {
  problem_.clear();
  std::vector<DiskFile> out;
  std::error_code ec;
  for (const fs::directory_entry& e : fs::directory_iterator(dir_, ec)) {
    if (!e.is_regular_file(ec)) continue;
    out.push_back({e.path().filename().string(), static_cast<uint64_t>(e.file_size(ec))});
  }
  if (ec) fail(dir_.string() + " does not list: " + ec.message());
  std::sort(out.begin(), out.end(),
            [](const DiskFile& a, const DiskFile& b) { return a.name < b.name; });
  return out;
}

// --- the simulated disk ---

bool SimDisk::fail(const std::string& what) {
  problem_ = what;
  return false;
}

bool SimDisk::spend(int64_t units) {
  if (dead) return false;
  if (budget - spent < units) {
    spent = budget;
    dead = true;
    return false;
  }
  spent += units;
  return true;
}

bool SimDisk::read(const std::string& name, std::vector<uint8_t>& out, size_t most) {
  problem_.clear();
  out.clear();
  if (dead) return fail("the disk is dead");
  const auto it = files.find(name);
  if (it == files.end()) return fail(name + " is not on the disk");
  const size_t n = std::min(most, it->second.size());
  out.assign(it->second.begin(), it->second.begin() + static_cast<std::ptrdiff_t>(n));
  reads++;
  bytes_read += n;
  return true;
}

bool SimDisk::write(const std::string& name, std::span<const uint8_t> bytes) {
  problem_.clear();
  if (dead || !spend(1)) return fail("the disk died opening " + name);
  std::vector<uint8_t>& f = files[name];
  f.clear();
  const int64_t left = budget - spent;
  if (left < static_cast<int64_t>(bytes.size())) {  // dies with part of it written
    f.assign(bytes.begin(), bytes.begin() + static_cast<std::ptrdiff_t>(left));
    spent = budget;
    dead = true;
    return fail("the disk died writing " + name + " at byte " + std::to_string(left));
  }
  f.assign(bytes.begin(), bytes.end());
  spent += static_cast<int64_t>(bytes.size());
  return true;
}

bool SimDisk::rename(const std::string& from, const std::string& to) {
  problem_.clear();
  if (!spend(1)) return fail("the disk died renaming " + from);
  const auto it = files.find(from);
  if (it == files.end()) return fail(from + " is not on the disk");
  std::vector<uint8_t> bytes = std::move(it->second);
  files.erase(it);
  files[to] = std::move(bytes);
  return true;
}

bool SimDisk::remove(const std::string& name) {
  problem_.clear();
  if (!spend(1)) return fail("the disk died removing " + name);
  return files.erase(name) == 1 ? true : fail(name + " is not on the disk");
}

bool SimDisk::sync() {
  problem_.clear();
  return spend(1) ? true : fail("the disk died flushing");
}

std::vector<DiskFile> SimDisk::list() {
  problem_.clear();
  std::vector<DiskFile> out;
  if (dead) {
    fail("the disk is dead");
    return out;
  }
  for (const auto& [name, bytes] : files) out.push_back({name, bytes.size()});
  return out;
}

SimDisk SimDisk::restarted() const {
  SimDisk d;
  d.files = files;
  return d;
}

}  // namespace ge::save
