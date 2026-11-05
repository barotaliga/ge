// The screenshot gallery: the folder listed and ordered, the modes and the presses, the worker
// that reads and shrinks the pictures, and the deletes (gallery.hpp).
//
// ge:summary The gallery's list, presses, loading worker and deletes
// ge:keywords gallery screenshot list sort newest worker thumbnail shrink delete remove
// ge:skill replay ui
// ge:test tests/capture_gallery_test.cpp
#include "capture/gallery.hpp"

#include <algorithm>
#include <fstream>
#include <iterator>
#include <system_error>
#include <utility>

#include "core/num_text.hpp"
#include "lab/gif.hpp"

namespace ge::capture {

namespace fs = std::filesystem;

namespace {

bool digits(std::string_view s, size_t at, size_t n) {
  if (at + n > s.size()) return false;
  for (size_t i = at; i < at + n; ++i)
    if (s[i] < '0' || s[i] > '9') return false;
  return true;
}

// Where `YYYY-MM-DD-HHMMSS` starts in `stem` (the last one), or npos.
size_t stamp_at(std::string_view stem) {
  constexpr size_t kLen = 17;
  if (stem.size() < kLen) return std::string_view::npos;
  for (size_t i = stem.size() - kLen + 1; i-- > 0;) {
    if (digits(stem, i, 4) && stem[i + 4] == '-' && digits(stem, i + 5, 2) && stem[i + 7] == '-' &&
        digits(stem, i + 8, 2) && stem[i + 10] == '-' && digits(stem, i + 11, 6))
      return i;
  }
  return std::string_view::npos;
}

// The order key of a still: its stamp's digits and its count, so "-10" sorts after "-9".
struct Key {
  std::string stamp;  // "" when the name holds no time
  int count = 1;
};

Key key_of(std::string_view stem) {
  Key k;
  const size_t at = stamp_at(stem);
  if (at == std::string_view::npos) return k;
  k.stamp = std::string(stem.substr(at, 17));
  const std::string_view rest = stem.substr(at + 17);
  if (rest.size() >= 2 && rest[0] == '-' && digits(rest, 1, rest.size() - 1) && rest.size() <= 8)
    k.count = std::stoi(std::string(rest.substr(1)));
  return k;
}

bool read_bytes(const std::string& path, std::vector<uint8_t>& out) {
  std::ifstream f(path, std::ios::binary);
  if (!f) return false;
  out.assign(std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>());
  return !f.bad();
}

}  // namespace

std::string taken_of(std::string_view stem) {
  const size_t at = stamp_at(stem);
  if (at == std::string_view::npos) return {};
  const std::string_view s = stem.substr(at, 17);
  std::string out;
  out.reserve(19);
  out.append(s.substr(0, 10));
  out += ' ';
  out.append(s.substr(11, 2));
  out += ':';
  out.append(s.substr(13, 2));
  out += ':';
  out.append(s.substr(15, 2));
  return out;
}

std::string size_text(uint64_t bytes) {
  if (bytes < 1024) return std::to_string(bytes) + " B";
  if (bytes < 1024 * 1024) return std::to_string((bytes + 512) / 1024) + " KB";
  return core::num::to_fixed(static_cast<double>(bytes) / (1024.0 * 1024.0), 1) + " MB";
}

std::vector<GalleryEntry> list_stills(const std::string& dir) {
  std::vector<GalleryEntry> out;
  std::vector<Key> keys;
  std::error_code ec;
  if (dir.empty() || !fs::is_directory(dir, ec)) return out;
  for (fs::directory_iterator it(dir, ec), end; !ec && it != end; it.increment(ec)) {
    std::error_code fec;
    if (!it->is_regular_file(fec)) continue;
    const fs::path& p = it->path();
    std::string ext = p.extension().string();
    if (ext.size() < 2) continue;
    ext.erase(0, 1);
    for (char& c : ext) c = static_cast<char>(c >= 'A' && c <= 'Z' ? c - 'A' + 'a' : c);
    StillFormat f{};
    if (!still_format_of(ext, f)) continue;
    GalleryEntry e;
    e.name = p.filename().string();
    e.path = p.string();
    e.format = f;
    e.bytes = static_cast<uint64_t>(it->file_size(fec));
    const std::string stem = p.stem().string();
    e.taken = taken_of(stem);
    keys.push_back(key_of(stem));
    out.push_back(std::move(e));
  }
  std::vector<size_t> order(out.size());
  for (size_t i = 0; i < order.size(); ++i) order[i] = i;
  std::sort(order.begin(), order.end(), [&](size_t a, size_t b) {
    const Key &ka = keys[a], &kb = keys[b];
    if (ka.stamp.empty() != kb.stamp.empty()) return kb.stamp.empty();  // timed ones first
    if (ka.stamp != kb.stamp) return ka.stamp > kb.stamp;               // newest first
    if (ka.count != kb.count) return ka.count > kb.count;
    return out[a].name < out[b].name;
  });
  std::vector<GalleryEntry> sorted;
  sorted.reserve(out.size());
  for (size_t i : order) sorted.push_back(std::move(out[i]));
  return sorted;
}

GalleryCells gallery_cells(float area_w, float area_h, float min_cell_w, float aspect, float gap) {
  GalleryCells c;
  c.gap = gap;
  if (min_cell_w <= 0 || aspect <= 0) return c;
  c.columns = std::max(1, static_cast<int>((area_w + gap) / (min_cell_w + gap)));
  c.cell_w = std::max(
      0.0f, (area_w - gap * static_cast<float>(c.columns - 1)) / static_cast<float>(c.columns));
  c.cell_h = c.cell_w / aspect;
  c.rows = std::max(1, static_cast<int>((area_h + gap) / (c.cell_h + gap)));
  if (c.cell_h > area_h) {  // one row taller than the area: the cells shrink to fit it
    c.cell_h = std::max(0.0f, area_h);
    c.cell_w = c.cell_h * aspect;
  }
  return c;
}

Gallery::Gallery(std::string dir, int thumb_px)
    : dir_(std::move(dir)), thumb_px_(std::max(16, thumb_px)) {}

Gallery::~Gallery() {
  {
    std::lock_guard lock(mutex_);
    stop_ = true;
    queue_.clear();
  }
  wake_.notify_all();
  if (worker_.joinable()) worker_.join();
}

void Gallery::set_dir(std::string dir) {
  dir_ = std::move(dir);
  entries_.clear();
  listed_ = false;
  selected_ = -1;
  mode_ = GalleryMode::grid;
  thumbs_.clear();
  fulls_.clear();
  std::lock_guard lock(mutex_);
  queue_.clear();
  done_.clear();
}

fs::file_time_type Gallery::dir_time() const {
  std::error_code ec;
  const fs::file_time_type t = fs::last_write_time(dir_, ec);
  return ec ? fs::file_time_type{} : t;
}

void Gallery::scan() {
  const std::string keep = current() != nullptr ? current()->path : std::string();
  const int at = selected_;
  // What a load already read stays known.
  std::map<std::string, GalleryEntry> known;
  for (GalleryEntry& e : entries_) known.emplace(e.path, std::move(e));
  listed_time_ = dir_time();
  entries_ = list_stills(dir_);
  listed_ = true;
  for (GalleryEntry& e : entries_) {
    auto k = known.find(e.path);
    if (k == known.end() || k->second.bytes != e.bytes) continue;
    e.width = k->second.width;
    e.height = k->second.height;
    e.error = k->second.error;
  }
  selected_ = -1;
  for (int i = 0; i < count(); ++i)
    if (entries_[static_cast<size_t>(i)].path == keep) selected_ = i;
  if (selected_ < 0 && count() > 0) selected_ = std::clamp(at, 0, count() - 1);
  if (count() == 0) mode_ = GalleryMode::grid;
  // A file that changed under the same name is read again.
  for (auto it = thumbs_.begin(); it != thumbs_.end();) {
    auto k = known.find(it->first);
    const bool same = std::any_of(entries_.begin(), entries_.end(), [&](const GalleryEntry& e) {
      return e.path == it->first && k != known.end() && k->second.bytes == e.bytes;
    });
    it = same ? std::next(it) : thumbs_.erase(it);
  }
  for (auto it = fulls_.begin(); it != fulls_.end();) {
    auto k = known.find(it->first);
    const bool same = std::any_of(entries_.begin(), entries_.end(), [&](const GalleryEntry& e) {
      return e.path == it->first && k != known.end() && k->second.bytes == e.bytes;
    });
    it = same ? std::next(it) : fulls_.erase(it);
  }
}

const GalleryEntry* Gallery::current() const {
  return selected_ >= 0 && selected_ < count() ? &entries_[static_cast<size_t>(selected_)]
                                               : nullptr;
}

void Gallery::select(int index) {
  selected_ = count() == 0 ? -1 : std::clamp(index, 0, count() - 1);
}

void Gallery::step(int by) {
  if (count() > 0) select(selected_ + by);
}

void Gallery::set_grid(int columns, int rows) {
  columns_ = std::max(1, columns);
  rows_ = std::max(1, rows);
}

int Gallery::page() const { return selected_ <= 0 ? 0 : selected_ / per_page(); }

int Gallery::pages() const { return count() == 0 ? 1 : (count() + per_page() - 1) / per_page(); }

void Gallery::reset() {
  mode_ = GalleryMode::grid;
  error_.clear();
  select(0);
}

bool Gallery::remove_selected() {
  const GalleryEntry* e = current();
  if (e == nullptr) {
    error_ = "no screenshot is selected";
    return false;
  }
  // Only a still in the gallery's own folder is ever removed.
  std::error_code ec;
  const fs::path p(e->path);
  StillFormat f{};
  std::string ext = p.extension().string();
  if (!ext.empty()) ext.erase(0, 1);
  for (char& c : ext) c = static_cast<char>(c >= 'A' && c <= 'Z' ? c - 'A' + 'a' : c);
  if (!still_format_of(ext, f) || !fs::equivalent(p.parent_path(), dir_, ec)) {
    error_ = "gallery: " + e->path + " is not a still in " + dir_ + "; nothing was deleted";
    return false;
  }
  if (!fs::remove(p, ec) || ec) {
    error_ = "gallery: " + e->name + " could not be deleted (" +
             (ec ? ec.message() : std::string("it is gone already")) +
             "); next: check the folder's permissions: " + dir_;
    scan();
    return false;
  }
  error_.clear();
  thumbs_.erase(e->path);
  fulls_.erase(e->path);
  entries_.erase(entries_.begin() + selected_);
  listed_time_ = dir_time();
  select(selected_);
  return true;
}

GalleryAction Gallery::input(const GalleryInput& in) {
  if (mode_ == GalleryMode::confirm) {
    if (in.accept) {
      mode_ = before_confirm_;
      if (!remove_selected()) return GalleryAction::failed;
      if (count() == 0) mode_ = GalleryMode::grid;
      return GalleryAction::deleted;
    }
    if (in.back) {
      mode_ = before_confirm_;
      return GalleryAction::kept;
    }
    return GalleryAction::none;
  }
  if (in.del && current() != nullptr) {
    before_confirm_ = mode_;
    mode_ = GalleryMode::confirm;
    return GalleryAction::asked;
  }
  const int was = selected_;
  if (mode_ == GalleryMode::viewer) {
    if (in.back || in.accept) {
      mode_ = GalleryMode::grid;
      return GalleryAction::closed;
    }
    if (in.left) step(-1);
    if (in.right) step(1);
    return selected_ != was ? GalleryAction::moved : GalleryAction::none;
  }
  if (in.back) return GalleryAction::leave;
  if (count() == 0) return GalleryAction::none;
  if (in.pointed >= 0 && in.pointed < count()) {
    select(in.pointed);
    if (in.click) {
      mode_ = GalleryMode::viewer;
      return GalleryAction::opened;
    }
  }
  if (in.accept) {
    mode_ = GalleryMode::viewer;
    return GalleryAction::opened;
  }
  if (in.left && selected_ % columns_ > 0) step(-1);
  if (in.right && selected_ % columns_ < columns_ - 1) step(1);
  if (in.up && selected_ >= columns_) step(-columns_);
  if (in.down) {
    // Down from above the last row lands in it, on the last entry when the column is short.
    if (selected_ + columns_ < count())
      step(columns_);
    else if (selected_ / columns_ < (count() - 1) / columns_)
      select(count() - 1);
  }
  return selected_ != was ? GalleryAction::moved : GalleryAction::none;
}

const GalleryPicture* Gallery::thumb(int index) const {
  if (index < 0 || index >= count()) return nullptr;
  auto it = thumbs_.find(entries_[static_cast<size_t>(index)].path);
  return it == thumbs_.end() ? nullptr : &it->second;
}

const GalleryPicture* Gallery::picture() const {
  const GalleryEntry* e = current();
  if (e == nullptr) return nullptr;
  auto it = fulls_.find(e->path);
  return it == fulls_.end() ? nullptr : &it->second;
}

bool Gallery::held(const std::string& path, bool full) const {
  return full ? fulls_.count(path) != 0 : thumbs_.count(path) != 0;
}

void Gallery::want(std::vector<Load>& out, int index, bool full) const {
  if (index < 0 || index >= count()) return;
  const GalleryEntry& e = entries_[static_cast<size_t>(index)];
  if (!e.error.empty() || held(e.path, full)) return;
  out.push_back({e.path, full});
}

void Gallery::forget_far() {
  // Thumbnails of the page and the pages either side are kept; full pictures of the selected one
  // and its neighbours in the viewer, none on the grid.
  const int per = per_page();
  const int from = first_on_page() - per, to = first_on_page() + 2 * per;
  std::map<std::string, int> at;
  for (int i = 0; i < count(); ++i) at.emplace(entries_[static_cast<size_t>(i)].path, i);
  for (auto it = thumbs_.begin(); it != thumbs_.end();) {
    auto a = at.find(it->first);
    const bool keep = a != at.end() && a->second >= from && a->second < to;
    it = keep ? std::next(it) : thumbs_.erase(it);
  }
  for (auto it = fulls_.begin(); it != fulls_.end();) {
    auto a = at.find(it->first);
    const bool keep = mode_ != GalleryMode::grid && a != at.end() && a->second >= selected_ - 1 &&
                      a->second <= selected_ + 1;
    it = keep ? std::next(it) : fulls_.erase(it);
  }
}

bool Gallery::poll() {
  if (!listed_ || dir_time() != listed_time_) scan();
  std::vector<Loaded> got;
  {
    std::lock_guard lock(mutex_);
    got.swap(done_);
  }
  bool arrived = false;
  for (Loaded& l : got) {
    auto e = std::find_if(entries_.begin(), entries_.end(),
                          [&](const GalleryEntry& x) { return x.path == l.path; });
    if (e == entries_.end()) continue;  // deleted or listed away meanwhile
    if (!l.error.empty()) {
      e->error = l.error;
      continue;
    }
    e->width = l.width;
    e->height = l.height;
    (l.full ? fulls_ : thumbs_)[l.path] = {std::move(l.picture), ++stamp_};
    arrived = true;
  }
  forget_far();
  // Most wanted first: the viewer's picture, this page's thumbnails, the neighbours' pictures,
  // then the pages either side.
  std::vector<Load> wanted;
  const int per = per_page(), first = first_on_page();
  if (mode_ != GalleryMode::grid) want(wanted, selected_, true);
  for (int i = first; i < first + per; ++i) want(wanted, i, false);
  if (mode_ != GalleryMode::grid) {
    want(wanted, selected_ + 1, true);
    want(wanted, selected_ - 1, true);
  }
  for (int i = first + per; i < first + 2 * per; ++i) want(wanted, i, false);
  for (int i = first - per; i < first; ++i) want(wanted, i, false);
  {
    std::lock_guard lock(mutex_);
    queue_.clear();
    for (Load& l : wanted)
      if (!(l.path == busy_path_ && l.full == busy_full_)) queue_.push_back(std::move(l));
    if (!queue_.empty() && !worker_.joinable()) worker_ = std::thread([this] { work(); });
  }
  if (!wanted.empty()) wake_.notify_one();
  return arrived;
}

void Gallery::settle() {
  for (int round = 0; round < 1000; ++round) {
    poll();
    std::unique_lock lock(mutex_);
    idle_.wait(lock, [this] { return queue_.empty() && busy_path_.empty(); });
    if (done_.empty()) return;
  }
}

void Gallery::work() {
  std::unique_lock lock(mutex_);
  for (;;) {
    wake_.wait(lock, [this] { return stop_ || !queue_.empty(); });
    if (stop_) return;
    Load job = std::move(queue_.front());
    queue_.pop_front();
    busy_path_ = job.path;
    busy_full_ = job.full;
    lock.unlock();

    Loaded out;
    out.path = job.path;
    out.full = job.full;
    std::vector<uint8_t> bytes;
    if (!read_bytes(job.path, bytes)) {
      out.error = "the file does not open";
    } else if (std::optional<lab::Picture> p = decode_still(bytes, out.error)) {
      out.width = p->width;
      out.height = p->height;
      out.picture = job.full ? std::move(*p) : lab::shrink(*p, thumb_px_);
    } else if (out.error.empty()) {
      out.error = "the picture does not read";
    }

    lock.lock();
    busy_path_.clear();
    done_.push_back(std::move(out));
    if (queue_.empty()) idle_.notify_all();
  }
}

}  // namespace ge::capture
