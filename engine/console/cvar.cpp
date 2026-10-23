// The cvar registry: storing a value through its kind and range into its field, the tiers'
// refusals, latching and the info strings.
//
// ge:summary The cvar registry's definitions: parse, clamp, write the bound field, refuse by tier
// ge:keywords cvar set store clamp parse_number number_text info_string apply_info apply_latched
// ge:skill console
// ge:test tests/console_cvar_test.cpp
#include "console/cvar.hpp"

#include <algorithm>
#include <cmath>
#include <cstdlib>

#include "core/num_text.hpp"

namespace ge::console {

std::string lower(std::string_view s) {
  std::string out(s);
  for (char& c : out)
    if (c >= 'A' && c <= 'Z') c = static_cast<char>(c - 'A' + 'a');
  return out;
}

bool valid_name(std::string_view name) {
  if (name.empty() || name.size() > 63) return false;
  if (name[0] >= '0' && name[0] <= '9') return false;
  for (char c : name) {
    const bool ok = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') ||
                    c == '_' || c == '+' || c == '-' || c == '.';
    if (!ok) return false;
  }
  return true;
}

bool parse_number(std::string_view s, double& out) {
  while (!s.empty() && (s.front() == ' ' || s.front() == '\t')) s.remove_prefix(1);
  while (!s.empty() && (s.back() == ' ' || s.back() == '\t')) s.remove_suffix(1);
  if (!s.empty() && s.front() == '+') s.remove_prefix(1);
  if (s.empty()) return false;
  // Plain decimal only ("1", "-0.5", "2e3"): no hex, inf or nan. strtod rounds correctly on every
  // platform's C library, and the engine never changes the C locale, so the point is always '.'.
  // (Floating-point from_chars is missing from some standard libraries.)
  bool digit = false;
  for (size_t i = 0; i < s.size(); ++i) {
    const char c = s[i];
    if (c >= '0' && c <= '9') {
      digit = true;
      continue;
    }
    const bool sign = (c == '-' || c == '+') && (i == 0 || s[i - 1] == 'e' || s[i - 1] == 'E');
    if (!sign && c != '.' && c != 'e' && c != 'E') return false;
  }
  if (!digit) return false;
  const std::string text(s);
  char* end = nullptr;
  const double v = std::strtod(text.c_str(), &end);
  if (end != text.c_str() + text.size() || !std::isfinite(v)) return false;
  out = v;
  return true;
}

std::string number_text(double x) {
  if (x == 0) return "0";  // -0 too
  return core::num::to_string(x);
}

int32_t Cvar::integer() const {
  if (!(number == number)) return 0;
  if (number >= 2147483647.0) return 2147483647;
  if (number <= -2147483648.0) return -2147483647 - 1;
  return static_cast<int32_t>(number);
}

std::string set_result_text(SetResult r, std::string_view name) {
  const std::string n(name);
  switch (r) {
    case SetResult::changed:
    case SetResult::same:
    case SetResult::latched:
      return {};
    case SetResult::unknown:
      return "unknown cvar '" + n + "': see `cvarlist " + n.substr(0, 3) +
             "` for the names, or `set` to create one";
    case SetResult::read_only:
      return n + " is read-only: only the game sets it";
    case SetResult::server:
      return n + " is the server's: set it in the server's cfg (a client follows the server)";
    case SetResult::cheat:
      return n + " is a cheat: it changes only while the server's sv_cheats is 1";
    case SetResult::refused:
      return "the server may not set " + n + ": it is this player's own setting";
    case SetResult::bad_name:
      return "'" + n + "' is not a cvar name: use 1-63 of a-z 0-9 _ + - . , no leading digit";
  }
  return {};
}

Cvars::Cvars(bool is_server) : is_server_(is_server) {
  sv_cheats_ = &add({.name = "sv_cheats",
                     .fallback = "0",
                     .flags = kServer | kServerInfo,
                     .help = "1 lets the cheat-flagged cvars change (development, experiments)",
                     .kind = CvarKind::boolean});
}

Cvar* Cvars::find(std::string_view name) {
  const std::string key = lower(name);
  for (auto& c : all_)
    if (c->name == key) return c.get();
  return nullptr;
}

const Cvar* Cvars::find(std::string_view name) const {
  return const_cast<Cvars*>(this)->find(name);
}

bool Cvars::cheats() const { return sv_cheats_ != nullptr && sv_cheats_->on(); }

Cvar& Cvars::make(const CvarSpec& spec, CvarTarget target) {
  Cvar* c = find(spec.name);
  if (c != nullptr && (c->flags & kCreated) == 0) return *c;
  std::string keep;
  const bool kept = c != nullptr;
  if (c == nullptr) {
    all_.push_back(std::make_unique<Cvar>());
    c = all_.back().get();
  } else {
    keep = c->value;
  }
  c->name = lower(spec.name);
  c->fallback = std::string(spec.fallback);
  c->help = std::string(spec.help);
  c->flags = spec.flags | (kept ? (c->flags & kArchive) : 0u);
  c->kind = spec.kind;
  c->min = spec.min;
  c->max = spec.max;
  c->target = target;
  if (c->kind == CvarKind::boolean) {
    c->min = 0;
    c->max = 1;
  }
  c->value.clear();
  store(*c, spec.fallback);
  c->fallback = c->value;  // the fallback as stored (clamped, normalised)
  if (kept) store(*c, keep);
  c->revision = 0;
  return *c;
}

Cvar& Cvars::add(const CvarSpec& spec) { return make(spec, std::monostate{}); }
Cvar& Cvars::bind(const CvarSpec& spec, double* field) { return make(spec, field); }
Cvar& Cvars::bind(const CvarSpec& spec, float* field) { return make(spec, field); }
Cvar& Cvars::bind(const CvarSpec& spec, int32_t* field) { return make(spec, field); }
Cvar& Cvars::bind(const CvarSpec& spec, uint32_t* field) { return make(spec, field); }
Cvar& Cvars::bind(const CvarSpec& spec, bool* field) { return make(spec, field); }
Cvar& Cvars::bind(const CvarSpec& spec, std::string* field) { return make(spec, field); }

// The bound field, if any, from the cvar's value.
static void write_field(const Cvar& c) {
  const double v = c.number;
  if (auto* p = std::get_if<double*>(&c.target)) **p = v;
  if (auto* p = std::get_if<float*>(&c.target)) **p = static_cast<float>(v);
  if (auto* p = std::get_if<int32_t*>(&c.target)) **p = c.integer();
  if (auto* p = std::get_if<uint32_t*>(&c.target))
    **p = v <= 0 ? 0u : v >= 4294967295.0 ? 4294967295u : static_cast<uint32_t>(v);
  if (auto* p = std::get_if<bool*>(&c.target)) **p = v != 0;
  if (auto* p = std::get_if<std::string*>(&c.target)) **p = c.value;
}

// The value through its kind: a number clamped, an integer rounded, a boolean made 0 or 1; text
// that is not a number reads as 0 for every kind but text.
void Cvars::store(Cvar& c, std::string_view text) {
  double v = 0;
  std::string out;
  if (c.kind == CvarKind::text) {
    out = std::string(text);
    if (!parse_number(text, v)) v = 0;
  } else {
    const bool num = parse_number(text, v);
    if (c.kind == CvarKind::boolean) {
      const std::string t = lower(text);
      v = num ? (v != 0 ? 1 : 0) : (t == "true" || t == "on" || t == "yes" ? 1 : 0);
    } else {
      if (!num) v = 0;
      if (c.kind == CvarKind::integer) v = v < 0 ? -std::floor(-v + 0.5) : std::floor(v + 0.5);
      v = std::clamp(v, c.min, c.max);
    }
    out = number_text(v);
  }
  const bool moved = out != c.value;
  c.value = std::move(out);
  c.number = v;
  if (moved) ++c.revision;
  write_field(c);
}

SetResult Cvars::set(std::string_view name, std::string_view value, Source source, bool create,
                     uint32_t extra_flags) {
  if (!valid_name(name)) return SetResult::bad_name;
  Cvar* c = find(name);
  if (c == nullptr) {
    if (!create) return SetResult::unknown;
    c = &add(
        {.name = name, .fallback = "", .flags = kCreated | extra_flags, .kind = CvarKind::text});
    store(*c, value);
    return SetResult::changed;
  }
  c->flags |= extra_flags & kArchive;
  return set(*c, value, source);
}

SetResult Cvars::set(Cvar& c, std::string_view value, Source source) {
  if (source != Source::code) {
    if ((c.flags & kReadOnly) != 0) return SetResult::read_only;
    if (source == Source::server) {
      if ((c.flags & (kServer | kServerInfo)) == 0 && (c.flags & kCreated) == 0)
        return SetResult::refused;
    } else {
      if ((c.flags & kServer) != 0 && !is_server_) return SetResult::server;
      if ((c.flags & kCheat) != 0 && !cheats()) return SetResult::cheat;
    }
  }
  if ((c.flags & kLatch) != 0 && source != Source::code) {
    if (value == c.value) {
      c.has_latched = false;
      c.latched.clear();
      return SetResult::same;
    }
    c.latched = std::string(value);
    c.has_latched = true;
    return SetResult::latched;
  }
  const uint32_t before = c.revision;
  store(c, value);
  return c.revision != before ? SetResult::changed : SetResult::same;
}

SetResult Cvars::reset(Cvar& c, Source source) { return set(c, c.fallback, source); }

int Cvars::apply_latched() {
  int moved = 0;
  for (auto& c : all_) {
    if (!c->has_latched) continue;
    const uint32_t before = c->revision;
    store(*c, c->latched);
    c->latched.clear();
    c->has_latched = false;
    if (c->revision != before) ++moved;
  }
  return moved;
}

std::string Cvars::info_string(uint32_t flag) const {
  std::string out;
  for (const Cvar* c : sorted())
    if ((c->flags & flag) != 0) out += "\\" + c->name + "\\" + c->value;
  return out;
}

int Cvars::apply_info(std::string_view info, Source source) {
  int moved = 0;
  if (!info.empty() && info.front() == '\\') info.remove_prefix(1);
  while (!info.empty()) {
    const size_t k = info.find('\\');
    if (k == std::string_view::npos) break;
    const std::string_view key = info.substr(0, k);
    info.remove_prefix(k + 1);
    const size_t v = info.find('\\');
    const std::string_view value = info.substr(0, v);
    info.remove_prefix(v == std::string_view::npos ? info.size() : v + 1);
    if (!valid_name(key)) continue;
    const uint32_t extra = source == Source::server ? kServerInfo : kUserInfo;
    Cvar* c = find(key);
    if (c == nullptr) {
      set(key, value, source, true, extra);
      ++moved;
      continue;
    }
    if (set(*c, value, source) == SetResult::changed) ++moved;
  }
  return moved;
}

std::vector<const Cvar*> Cvars::sorted() const {
  std::vector<const Cvar*> out;
  out.reserve(all_.size());
  for (const auto& c : all_) out.push_back(c.get());
  std::sort(out.begin(), out.end(), [](const Cvar* a, const Cvar* b) { return a->name < b->name; });
  return out;
}

std::vector<Cvar*> Cvars::sorted() {
  std::vector<Cvar*> out;
  out.reserve(all_.size());
  for (auto& c : all_) out.push_back(c.get());
  std::sort(out.begin(), out.end(), [](const Cvar* a, const Cvar* b) { return a->name < b->name; });
  return out;
}

}  // namespace ge::console
