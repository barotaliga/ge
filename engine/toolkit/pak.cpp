// The pak group's body (pak.hpp): an archive opened by its magic through engine/vfs, its files
// listed, unpacked, written out, packed, converted and summed, each answer a table, findings and
// one JSON result.
//
// ge:summary The pak group's body: archives opened through engine/vfs, listed, packed, summed; rows
// ge:keywords cli toolkit pak pk3 pak_rows archive open sniff magic extract build convert sum check
// ge:skill agent-interface vfs
// ge:test tests/toolkit_pak_test.cpp
#include "toolkit/pak.hpp"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iterator>

#include "assets/sha256.hpp"
#include "core/json_write.hpp"
#include "vfs/pak.hpp"
#include "vfs/zip.hpp"

namespace ge::toolkit {

using core::json_arr;
using core::json_num;
using core::json_obj;
using core::json_str;
using core::json_strs;

namespace fs = std::filesystem;

namespace {

// One file of an opened archive, as listed.
struct Item {
  std::string name;
  uint32_t size = 0, packed = 0, crc = 0;
  uint16_t method = 0;  // a zip's: 0 stored, 8 deflated; a pack's always 0
  size_t index = 0;     // its entry in the directory
};

// An archive read whole and its directory checked, either kind.
struct Archive {
  std::vector<uint8_t> bytes;
  bool zip = false;
  vfs::Pak pak;
  vfs::Zip dir;
  std::vector<Item> items;
};

std::string hex32(uint32_t v) {
  static const char* k = "0123456789abcdef";
  std::string s(8, '0');
  for (int i = 7; i >= 0; i--, v >>= 4) s[static_cast<size_t>(i)] = k[v & 15];
  return s;
}

bool read_bytes(const std::string& file, std::vector<uint8_t>* out) {
  std::ifstream in(file, std::ios::binary);
  if (!in) return false;
  out->assign(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
  return true;
}

bool write_bytes(const fs::path& file, const std::vector<uint8_t>& bytes) {
  std::error_code ec;
  if (file.has_parent_path()) fs::create_directories(file.parent_path(), ec);
  std::ofstream out(file, std::ios::binary | std::ios::trunc);
  if (!out) return false;
  out.write(reinterpret_cast<const char*>(bytes.data()),
            static_cast<std::streamsize>(bytes.size()));
  return static_cast<bool>(out);
}

std::string lower_ext(const std::string& file) {
  std::string e = fs::path(file).extension().string();
  for (char& c : e) c = static_cast<char>(c >= 'A' && c <= 'Z' ? c + 32 : c);
  return e;
}

// Opens `file` by its magic (`PACK`, or a zip's `PK`), its directory checked; an error finding
// naming the reason otherwise.
bool open_archive(const std::string& file, Archive* a, Outcome* o) {
  if (!read_bytes(file, &a->bytes)) {
    o->findings.push_back({Severity::error, "cannot read " + file, file, "check the path"});
    return false;
  }
  std::string error;
  const auto& b = a->bytes;
  if (b.size() >= 4 && b[0] == 'P' && b[1] == 'A' && b[2] == 'C' && b[3] == 'K') {
    if (!vfs::read_pak(b, &a->pak, &error)) {
      o->findings.push_back({Severity::error, "a damaged pack: " + error, file,
                             "fetch the archive again; its directory is not whole"});
      return false;
    }
    for (size_t i = 0; i < a->pak.entries.size(); i++) {
      const vfs::PakEntry& e = a->pak.entries[i];
      a->items.push_back({e.path, e.size, e.size, vfs::crc32_of(vfs::pak_file(b, e)), 0, i});
    }
    return true;
  }
  if (b.size() >= 2 && b[0] == 'P' && b[1] == 'K') {
    a->zip = true;
    if (!vfs::read_zip(b, &a->dir, &error)) {
      o->findings.push_back(
          {Severity::error, "a damaged or unsupported zip: " + error, file,
           "fetch the archive again; zip64, encryption and spanning are refused"});
      return false;
    }
    for (size_t i = 0; i < a->dir.entries.size(); i++) {
      const vfs::ZipEntry& e = a->dir.entries[i];
      if (!e.name.empty() && e.name.back() == '/') continue;  // a directory's own entry
      a->items.push_back({e.name, e.size, e.packed, e.crc, e.method, i});
    }
    return true;
  }
  o->findings.push_back(
      {Severity::error, "neither a pack (PACK) nor a zip (PK)", file, "name a .pak or .pk3 file"});
  return false;
}

// The file's bytes, unpacked (and, for a zip, checked against its CRC-32).
bool file_bytes(const Archive& a, const Item& it, std::vector<uint8_t>* out, std::string* error) {
  if (a.zip) return vfs::zip_unpack(a.bytes, a.dir.entries[it.index], out, error);
  const auto view = vfs::pak_file(a.bytes, a.pak.entries[it.index]);
  out->assign(view.begin(), view.end());
  return true;
}

// The items `names` picks (all of them when empty), each name matched the archive's own way;
// a name it does not hold is an error finding.
std::vector<const Item*> pick(const Call& args, const Archive& a,
                              const std::vector<std::string>& names, const std::string& file,
                              Outcome* o) {
  std::vector<const Item*> out;
  if (names.empty()) {
    for (const Item& it : a.items) out.push_back(&it);
    return out;
  }
  for (const std::string& n : names) {
    const Item* hit = nullptr;
    for (const Item& it : a.items) {
      if (a.zip ? vfs::same_name(it.name, n) : it.name == n) {
        hit = &it;
        break;
      }
    }
    if (hit) {
      out.push_back(hit);
    } else {
      o->findings.push_back({Severity::error, "not in the archive: " + n, file,
                             args.program + " pak ls " + file + " lists its files"});
    }
  }
  return out;
}

}  // namespace

std::string pak_table(const std::vector<std::vector<std::string>>& rows) {
  std::vector<size_t> width;
  for (const auto& r : rows) {
    if (width.size() < r.size()) width.resize(r.size(), 0);
    for (size_t i = 0; i < r.size(); i++) width[i] = std::max(width[i], r[i].size());
  }
  std::string out;
  for (const auto& r : rows) {
    std::string line = " ";
    for (size_t i = 0; i < r.size(); i++) {
      line += ' ';
      line += r[i];
      if (i + 1 < r.size()) line += std::string(width[i] - r[i].size() + 1, ' ');
    }
    while (!line.empty() && line.back() == ' ') line.pop_back();
    out += line;
    out += '\n';
  }
  return out;
}

namespace {

Outcome pak_ls(const Call& args) {
  Outcome o;
  const std::string& file = args.words[0];
  std::error_code ec;
  if (fs::is_directory(file, ec)) return pak_ls_game(args);
  if (!flag_value(args, "mods").empty() || has_flag(args, "hidden")) {
    o.findings.push_back({Severity::error, "--mods and --hidden read a game directory", file,
                          "name the game directory (ge pak ls <root>/<game>) instead"});
    return o;
  }
  Archive a;
  if (!open_archive(file, &a, &o)) return o;
  const bool sha = has_flag(args, "sha256");
  std::vector<std::vector<std::string>> rows;
  rows.push_back({"path", "bytes", "crc32"});
  if (a.zip) rows[0].insert(rows[0].end(), {"stored", "packed"});
  if (sha) rows[0].emplace_back("sha256");
  std::vector<std::string> json;
  long long total = 0;
  for (const Item& it : a.items) {
    std::vector<std::string> row = {it.name, std::to_string(it.size), hex32(it.crc)};
    std::vector<std::pair<std::string, std::string>> fields = {{"path", json_str(it.name)},
                                                               {"bytes", json_num(it.size)},
                                                               {"crc32", json_str(hex32(it.crc))}};
    if (a.zip) {
      const char* how = it.method == 8 ? "deflated" : "stored";
      row.insert(row.end(), {how, std::to_string(it.packed)});
      fields.emplace_back("method", json_str(how));
      fields.emplace_back("packed", json_num(it.packed));
    }
    if (sha) {
      std::vector<uint8_t> bytes;
      std::string error;
      std::string digest;
      if (file_bytes(a, it, &bytes, &error)) {
        digest = assets::sha256_hex(bytes);
      } else {
        o.findings.push_back({Severity::error, it.name + ": " + error, file,
                              "the file is damaged inside the archive; fetch it again"});
      }
      row.push_back(digest);
      fields.emplace_back("sha256", json_str(digest));
    }
    rows.push_back(std::move(row));
    json.push_back(json_obj(fields));
    total += it.size;
  }
  o.text = pak_table(rows);
  o.summary = std::to_string(a.items.size()) + " file(s), " + std::to_string(total) +
              " bytes, in a " + (a.zip ? "zip" : "pack");
  o.next = args.program + " pak extract " + file + " <file> --out=<dir>";
  o.result = json_obj({{"archive", json_str(file)},
                       {"kind", json_str(a.zip ? "zip" : "pak")},
                       {"bytes", json_num(total)},
                       {"files", json_arr(json)}});
  return o;
}

Outcome pak_extract(const Call& args) {
  Outcome o;
  const std::string& file = args.words[0];
  const std::string out_dir = flag_value(args, "out");
  if (out_dir.empty()) {
    o.findings.push_back({Severity::error, "no --out directory", args.program + " pak extract",
                          args.program + " pak extract " + file + " --out=<dir>"});
    return o;
  }
  Archive a;
  if (!open_archive(file, &a, &o)) return o;
  const std::vector<std::string> names(args.words.begin() + 1, args.words.end());
  long long total = 0;
  size_t count = 0;
  for (const Item* it : pick(args, a, names, file, &o)) {
    std::string name = it->name;
    std::replace(name.begin(), name.end(), '\\', '/');
    if (!safe_entry_path(name)) {
      o.findings.push_back(
          {Severity::error, "refused, it would leave --out: " + it->name, file,
           "the archive names a path outside its root; read it with " + args.program + " pak ls"});
      continue;
    }
    std::vector<uint8_t> bytes;
    std::string error;
    if (!file_bytes(a, *it, &bytes, &error)) {
      o.findings.push_back({Severity::error, it->name + ": " + error, file,
                            "the file is damaged inside the archive; fetch it again"});
      continue;
    }
    const fs::path dest = fs::path(out_dir) / fs::path(name);
    if (!write_bytes(dest, bytes)) {
      o.findings.push_back({Severity::error, "cannot write " + dest.generic_string(),
                            dest.generic_string(), "check --out is writable"});
      continue;
    }
    o.written.push_back(dest.generic_string());
    total += static_cast<long long>(bytes.size());
    count++;
  }
  o.summary =
      std::to_string(count) + " file(s), " + std::to_string(total) + " bytes, into " + out_dir;
  o.result = json_obj({{"archive", json_str(file)},
                       {"out", json_str(out_dir)},
                       {"files", json_num(static_cast<long long>(count))},
                       {"bytes", json_num(total)}});
  return o;
}

// Writes `files` as the kind `out`'s extension names; false with a finding.
bool write_archive(const std::string& out, const PakFiles& files, Outcome* o) {
  const std::string ext = lower_ext(out);
  std::vector<uint8_t> bytes;
  if (ext == ".pak") {
    std::string error;
    if (!vfs::write_pak(files.files, &bytes, &error)) {
      o->findings.push_back({Severity::error, "cannot pack: " + error, out,
                             "shorten the path to 55 bytes or fewer, or write a .pk3"});
      return false;
    }
  } else if (ext == ".pk3" || ext == ".zip") {
    bytes = vfs::write_stored_zip(files.files);
  } else {
    o->findings.push_back({Severity::error, "unknown archive kind '" + ext + "'", out,
                           "name the output .pak or .pk3"});
    return false;
  }
  if (!write_bytes(out, bytes)) {
    o->findings.push_back({Severity::error, "cannot write " + out, out, "check the path"});
    return false;
  }
  o->written.push_back(out);
  return true;
}

long long bytes_of(const PakFiles& f) {
  long long n = 0;
  for (const auto& [name, bytes] : f.files) n += static_cast<long long>(bytes.size());
  return n;
}

Outcome pak_build(const Call& args) {
  Outcome o;
  const std::string& dir = args.words[0];
  const std::string out = flag_value(args, "out");
  std::error_code ec;
  if (out.empty() || !fs::is_directory(dir, ec)) {
    o.findings.push_back({Severity::error, out.empty() ? "no --out archive" : "not a directory",
                          dir, args.program + " pak build <dir> --out=<file.pak|file.pk3>"});
    return o;
  }
  std::vector<std::string> paths;
  for (auto it = fs::recursive_directory_iterator(dir, ec);
       !ec && it != fs::recursive_directory_iterator(); it.increment(ec)) {
    if (it->is_regular_file(ec))
      paths.push_back(fs::relative(it->path(), dir, ec).generic_string());
  }
  std::sort(paths.begin(), paths.end());
  PakFiles files;
  for (const std::string& p : paths) {
    std::vector<uint8_t> bytes;
    if (!read_bytes((fs::path(dir) / p).string(), &bytes)) {
      o.findings.push_back({Severity::error, "cannot read " + p, dir, "check its permissions"});
      return o;
    }
    files.files.emplace_back(p, std::move(bytes));
  }
  if (!write_archive(out, files, &o)) return o;
  o.summary = std::to_string(files.files.size()) + " file(s), " + std::to_string(bytes_of(files)) +
              " bytes, packed into " + out;
  o.next = args.program + " pak ls " + out;
  o.result = json_obj({{"out", json_str(out)},
                       {"files", json_num(static_cast<long long>(files.files.size()))},
                       {"bytes", json_num(bytes_of(files))}});
  return o;
}

// Every file of an opened archive, unpacked in its order; false with a finding.
bool unpack_all(const Archive& a, const std::string& file, PakFiles* out, Outcome* o) {
  out->zip = a.zip;
  for (const Item& it : a.items) {
    std::vector<uint8_t> bytes;
    std::string error;
    if (!file_bytes(a, it, &bytes, &error)) {
      o->findings.push_back({Severity::error, it.name + ": " + error, file,
                             "the file is damaged inside the archive; fetch it again"});
      return false;
    }
    out->files.emplace_back(it.name, std::move(bytes));
  }
  return true;
}

Outcome pak_convert(const Call& args) {
  Outcome o;
  const std::string& in = args.words[0];
  const std::string& out = args.words[1];
  Archive a;
  if (!open_archive(in, &a, &o)) return o;
  PakFiles files;
  if (!unpack_all(a, in, &files, &o)) return o;
  if (lower_ext(out) == ".pak") {
    for (auto& [name, bytes] : files.files) std::replace(name.begin(), name.end(), '\\', '/');
  }
  if (!write_archive(out, files, &o)) return o;
  o.summary = std::string(a.zip ? "a zip" : "a pack") + " of " +
              std::to_string(files.files.size()) + " file(s) rewritten as " + out;
  o.next = args.program + " pak sum " + in + " --out=<sums>, then " + args.program + " pak sum " +
           out + " --check=<sums> to prove every file came across";
  o.result = json_obj({{"in", json_str(in)},
                       {"out", json_str(out)},
                       {"files", json_num(static_cast<long long>(files.files.size()))},
                       {"bytes", json_num(bytes_of(files))}});
  return o;
}

Outcome pak_sum(const Call& args) {
  Outcome o;
  const std::string& file = args.words[0];
  Archive a;
  if (!open_archive(file, &a, &o)) return o;
  const std::vector<std::string> names(args.words.begin() + 1, args.words.end());
  const std::string check = flag_value(args, "check");
  // name -> digest, for --check's lookups (the archive's own matching).
  std::vector<std::pair<std::string, std::string>> sums;
  std::string lines;
  std::vector<const Item*> items;
  if (check.empty()) {
    items = pick(args, a, names, file, &o);
  } else {
    items = pick(args, a, {}, file, &o);
  }
  for (const Item* it : items) {
    std::vector<uint8_t> bytes;
    std::string error;
    if (!file_bytes(a, *it, &bytes, &error)) {
      o.findings.push_back({Severity::error, it->name + ": " + error, file,
                            "the file is damaged inside the archive; fetch it again"});
      continue;
    }
    sums.emplace_back(it->name, assets::sha256_hex(bytes));
  }
  if (check.empty()) {
    std::vector<std::string> json;
    for (const auto& [name, digest] : sums) {
      lines += digest;
      lines += "  ";
      lines += name;
      lines += '\n';
      json.push_back(json_obj({{"path", json_str(name)}, {"sha256", json_str(digest)}}));
    }
    o.text = lines;
    const std::string out = flag_value(args, "out");
    if (!out.empty()) {
      if (write_bytes(out, std::vector<uint8_t>(lines.begin(), lines.end()))) {
        o.written.push_back(out);
      } else {
        o.findings.push_back({Severity::error, "cannot write " + out, out, "check the path"});
      }
    }
    o.summary = std::to_string(sums.size()) + " file(s) summed";
    o.next = out.empty()
                 ? args.program + " pak sum " + file + " --out=<sums> keeps them"
                 : args.program + " pak sum <archive> --check=" + out + " holds an archive to them";
    o.result = json_obj({{"archive", json_str(file)}, {"files", json_arr(json)}});
    return o;
  }
  std::vector<uint8_t> text;
  if (!read_bytes(check, &text)) {
    o.findings.push_back({Severity::error, "cannot read " + check, check, "check the path"});
    return o;
  }
  size_t ok = 0, bad = 0, missing = 0;
  std::vector<std::string> failed;
  const std::string listed(text.begin(), text.end());
  for (std::string_view line : split_lines(listed)) {
    while (!line.empty() && line.back() == '\r') line.remove_suffix(1);
    std::string want, path;
    if (line.empty() || line[0] == '#') continue;
    if (!parse_sum_line(line, &want, &path)) {
      o.findings.push_back(
          {Severity::warn, "not a sum line: " + std::string(line), check,
           "lines are \"<sha256>  <path>\", as " + args.program + " pak sum --out writes them"});
      continue;
    }
    if (!names.empty() && std::none_of(names.begin(), names.end(), [&](const std::string& n) {
          return a.zip ? vfs::same_name(n, path) : n == path;
        })) {
      continue;
    }
    const auto hit = std::find_if(sums.begin(), sums.end(), [&](const auto& s) {
      return a.zip ? vfs::same_name(s.first, path) : s.first == path;
    });
    if (hit == sums.end()) {
      missing++;
      failed.push_back(path);
      o.findings.push_back({Severity::error, "missing from the archive: " + path, file,
                            "the archive is not the one the sums were taken of"});
    } else if (hit->second != want) {
      bad++;
      failed.push_back(path);
      std::string what = "changed: ";
      what += path;
      what += " is ";
      what += hit->second;
      what += ", the sums say ";
      what += want;
      o.findings.push_back({Severity::error, std::move(what), file,
                            "the file differs from the one the sums were taken of"});
    } else {
      ok++;
    }
  }
  o.summary = std::to_string(ok) + " file(s) match " + check;
  if (bad + missing) {
    o.summary += "; " + std::to_string(bad) + " changed, " + std::to_string(missing) + " missing";
  }
  o.result = json_obj({{"archive", json_str(file)},
                       {"check", json_str(check)},
                       {"ok", json_num(static_cast<long long>(ok))},
                       {"changed", json_num(static_cast<long long>(bad))},
                       {"missing", json_num(static_cast<long long>(missing))},
                       {"failed", json_strs(failed)}});
  return o;
}

}  // namespace

bool parse_sum_line(std::string_view line, std::string* hex, std::string* path) {
  if (line.size() < 67 || (line[64] != ' ') || (line[65] != ' ' && line[65] != '*')) return false;
  for (size_t i = 0; i < 64; i++) {
    const char c = line[i];
    if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F'))) return false;
  }
  hex->assign(line.substr(0, 64));
  for (char& c : *hex) c = static_cast<char>(c >= 'A' && c <= 'F' ? c + 32 : c);
  path->assign(line.substr(66));
  return !path->empty();
}

bool safe_entry_path(std::string_view name) {
  if (name.empty() || name.front() == '/' || name.find(':') != std::string_view::npos) {
    return false;
  }
  size_t start = 0;
  while (start <= name.size()) {
    const size_t end = std::min(name.find('/', start), name.size());
    const std::string_view part = name.substr(start, end - start);
    if (part == "..") return false;
    start = end + 1;
  }
  return name.back() != '/';
}

namespace {

constexpr Flag kLsFlags[] = {
    {"sha256", "", "each file's SHA-256 beside its CRC-32"},
    {"mods", "<a,b>",
     "a game directory: the mods' directories beside it, mounted over it in order"},
    {"hidden", "", "a game directory: only the files that hide one in an earlier source"}};
constexpr Flag kOutFlags[] = {{"out", "<path>", "where the result is written"}};
constexpr Flag kSumFlags[] = {
    {"out", "<sums>", "the sums written to this file too (sha256sum's format)"},
    {"check", "<sums>", "the archive held to a sums file: a changed or missing file an error"}};

constexpr Row kRows[] = {
    {"pak ls", "<archive|gamedir>", 1, 1, "see",
     "a .pak or .pk3 archive's files (path, bytes, CRC-32, how each is stored), or a game "
     "directory mounted as the game mounts it: every file, the source that wins it and what it "
     "hides; --sha256 adds each file's SHA-256",
     kLsFlags, "pak ls ~/classic/id1/pak0.pak --sha256", "assets", pak_ls},
    {"pak extract", "<archive> [<file>...]", 1, -1, "create",
     "every file of an archive, or the ones named, written under --out; a path that would leave "
     "it is refused",
     kOutFlags, "pak extract pak0.pak gfx/palette.lmp --out=scratch/pak0", "assets", pak_extract},
    {"pak build", "<dir>", 1, 1, "create",
     "every file under a directory packed by path into --out, a .pak or a (stored) .pk3 by its "
     "extension",
     kOutFlags, "pak build scratch/pak0 --out=scratch/pak0.pk3", "assets", pak_build},
    {"pak convert",
     "<in> <out>",
     2,
     2,
     "create",
     "an archive rewritten as the kind <out>'s extension names (.pak or .pk3), every file and "
     "its order kept",
     {},
     "pak convert pak0.pak pak0.pk3",
     "assets",
     pak_convert},
    {"pak sum", "<archive> [<file>...]", 1, -1, "see",
     "each file's SHA-256 in sha256sum's format (--out keeps them); --check holds the archive to "
     "a sums file, naming every changed or missing file",
     kSumFlags, "pak sum pak0.pk3 --check=pak0.sums", "assets", pak_sum},
};

}  // namespace

std::span<const Row> pak_rows() { return kRows; }

}  // namespace ge::toolkit
