// The catalog written and read (catalog.hpp).
//
// voice: agent (the catalog is read by the player CLI, whose errors are for agents;
// docs/architecture.md § Player text).
//
// ge:summary The cvar catalog's writer and reader: one tab-split line a cvar
// ge:keywords cvar catalog catalog_text read_catalog add_catalog kind_name kind_of flags_of
// ge:skill console agent-interface
// ge:test tests/gamecli_settings_test.cpp
#include "console/catalog.hpp"

#include <cstdlib>
#include <limits>

#include "console/cfg.hpp"
#include "core/num_text.hpp"

namespace ge::console {

namespace {

// A field kept on its line: a tab or a line break read as a space.
std::string flat(std::string_view s) {
  std::string out(s);
  for (char& c : out)
    if (c == '\t' || c == '\n' || c == '\r') c = ' ';
  return out;
}

std::string bound(double v) {
  constexpr double kInf = std::numeric_limits<double>::infinity();
  return v == kInf || v == -kInf ? "-" : core::num::to_string(v);
}

bool read_bound(std::string_view w, double fallback, double& out) {
  if (w == "-") {
    out = fallback;
    return true;
  }
  const std::string s(w);
  char* end = nullptr;
  out = std::strtod(s.c_str(), &end);
  return end != s.c_str() && *end == '\0';
}

}  // namespace

const char* kind_name(CvarKind kind) {
  switch (kind) {
    case CvarKind::text:
      return "text";
    case CvarKind::number:
      return "number";
    case CvarKind::integer:
      return "integer";
    case CvarKind::boolean:
      return "boolean";
  }
  return "text";
}

bool kind_of(std::string_view word, CvarKind& out) {
  for (const CvarKind k : {CvarKind::text, CvarKind::number, CvarKind::integer, CvarKind::boolean})
    if (word == kind_name(k)) {
      out = k;
      return true;
    }
  return false;
}

uint32_t flags_of(std::string_view letters) {
  uint32_t f = 0;
  for (const char c : letters) {
    switch (c) {
      case 'A':
        f |= kArchive;
        break;
      case 'U':
        f |= kUserInfo;
        break;
      case 'S':
        f |= kServerInfo;
        break;
      case 'V':
        f |= kServer;
        break;
      case 'C':
        f |= kCheat;
        break;
      case 'L':
        f |= kLatch;
        break;
      case 'R':
        f |= kReadOnly;
        break;
      case '*':
        f |= kCreated;
        break;
      default:
        break;
    }
  }
  return f;
}

std::string catalog_text(const Cvars& cvars) {
  std::string out =
      "// every setting this program registers: name, kind, flags, default, min, max, help "
      "(tab-split; console/catalog.hpp)\n";
  for (const Cvar* v : cvars.sorted()) {
    if ((v->flags & kCreated) != 0) continue;
    const bool ranged = v->kind == CvarKind::number || v->kind == CvarKind::integer;
    out += v->name + '\t' + kind_name(v->kind) + '\t' + flags_text(v->flags) + '\t' +
           flat(v->fallback) + '\t' + (ranged ? bound(v->min) : "-") + '\t' +
           (ranged ? bound(v->max) : "-") + '\t' + flat(v->help) + '\n';
  }
  return out;
}

bool read_catalog(std::string_view text, std::vector<CatalogEntry>& out, std::string& error) {
  out.clear();
  int line_no = 0;
  while (!text.empty()) {
    const size_t nl = text.find('\n');
    std::string_view line = text.substr(0, nl);
    text = nl == std::string_view::npos ? std::string_view{} : text.substr(nl + 1);
    line_no++;
    if (!line.empty() && line.back() == '\r') line.remove_suffix(1);
    if (line.empty() || line.starts_with("//")) continue;
    std::vector<std::string_view> f;
    size_t at = 0;
    while (f.size() < 6) {
      const size_t tab = line.find('\t', at);
      if (tab == std::string_view::npos) break;
      f.push_back(line.substr(at, tab - at));
      at = tab + 1;
    }
    f.push_back(line.substr(at));
    CatalogEntry e;
    const auto bad = [&](const std::string& what) {
      error = "the catalog's line " + std::to_string(line_no) + " " + what +
              "; next: start the game once so it writes its catalog again (console/catalog.hpp)";
      return false;
    };
    if (f.size() != 7) return bad("has " + std::to_string(f.size()) + " fields, not 7");
    if (!valid_name(f[0])) return bad("names no cvar ('" + std::string(f[0]) + "')");
    if (!kind_of(f[1], e.kind)) return bad("has no kind ('" + std::string(f[1]) + "')");
    if (!read_bound(f[4], e.min, e.min) || !read_bound(f[5], e.max, e.max))
      return bad("has a range that is not a number");
    e.name = std::string(f[0]);
    e.flags = flags_of(f[2]) & ~static_cast<uint32_t>(kCreated);
    e.fallback = std::string(f[3]);
    e.help = std::string(f[6]);
    out.push_back(std::move(e));
  }
  return true;
}

void add_catalog(Cvars& cvars, const std::vector<CatalogEntry>& entries) {
  for (const CatalogEntry& e : entries)
    cvars.add({.name = e.name,
               .fallback = e.fallback,
               .flags = e.flags,
               .help = e.help,
               .kind = e.kind,
               .min = e.min,
               .max = e.max});
}

}  // namespace ge::console
