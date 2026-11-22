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
#include <functional>
#include <string>
#include <utility>
#include <vector>

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
      return "unknown cvar '" + n + "' (type \"cvarlist " + n.substr(0, 3) +
             "\" for the names, or use set to create one)";
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
    case SetResult::locked:
      return n + " is locked";
  }
  return {};
}

std::string set_result_text(SetResult r, const Cvar& cvar) {
  if (r == SetResult::locked && cvar.lock && !cvar.lock->why.empty()) return cvar.lock->why;
  return set_result_text(r, cvar.name);
}

Cvars::Cvars(bool is_server) : is_server_(is_server) {
  sv_cheats_ = &add({.name = "sv_cheats",
                     .fallback = "0",
                     .flags = kServer | kServerInfo,
                     .help = "Whether the settings and commands marked as cheats may be used.",
                     .kind = CvarKind::boolean,
                     .values = "0: no cheats (the default); 1: cheats allowed, for development "
                               "and experiments"});
}

Cvar* Cvars::find(std::string_view name) {
  const std::string key = lower(name);
  for (auto& c : all_)
    if (c->name == key) return c->retired ? nullptr : c.get();
  for (const Synonym& s : synonyms_)
    if (s.part < 0 && s.name == key) return s.target->retired ? nullptr : s.target;
  return nullptr;
}

Cvar* Cvars::find_any(std::string_view name) {
  const std::string key = lower(name);
  for (auto& c : all_)
    if (c->name == key) return c.get();
  return nullptr;
}

bool Cvars::remove_created(std::string_view name) {
  Cvar* c = find(name);
  if (c == nullptr || (c->flags & kCreated) == 0 || synonym(name) != nullptr) return false;
  c->retired = true;  // `set` makes it again from nothing (make)
  c->has_latched = false;
  c->latched.clear();
  return true;
}

bool Cvars::add_synonym(std::string_view name, std::string_view target, int part) {
  if (!valid_name(name)) return false;
  const std::string key = lower(name);
  for (const auto& c : all_)
    if (c->name == key) return false;
  Cvar* t = find(target);
  if (t == nullptr) return false;
  for (const Synonym& s : synonyms_)
    if (s.name == key) return s.target == t && s.part == part;
  synonyms_.push_back({key, t, part < 0 ? -1 : part, layer_});
  return true;
}

const Synonym* Cvars::synonym(std::string_view name) const {
  const std::string key = lower(name);
  for (const Synonym& s : synonyms_)
    if (s.name == key) return &s;
  return nullptr;
}

namespace {

// The words of a value, split on spaces.
std::vector<std::string> words_of(std::string_view v) {
  std::vector<std::string> out;
  size_t at = 0;
  while (at < v.size()) {
    while (at < v.size() && v[at] == ' ') at++;
    const size_t start = at;
    while (at < v.size() && v[at] != ' ') at++;
    if (at > start) out.emplace_back(v.substr(start, at - start));
  }
  return out;
}

}  // namespace

std::string Cvars::synonym_value(const Synonym& s) const {
  if (s.part < 0) return s.target->value;
  const std::vector<std::string> w = words_of(s.target->value);
  return static_cast<size_t>(s.part) < w.size() ? w[static_cast<size_t>(s.part)] : std::string();
}

const Cvar* Cvars::find(std::string_view name) const {
  return const_cast<Cvars*>(this)->find(name);
}

bool Cvars::cheats() const { return sv_cheats_ != nullptr && sv_cheats_->on(); }

Cvar& Cvars::make(const CvarSpec& spec, CvarTarget target) {
  Cvar* c = find_any(spec.name);
  if (c != nullptr && !c->retired && (c->flags & kCreated) == 0) {
    if (c->layer == layer_) return *c;  // the same layer again: the first, as always
    return meet(*c, spec, target);
  }
  std::string keep;
  const bool kept = c != nullptr && !c->retired;  // a value `set` made before any code read it
  if (c == nullptr) {
    all_.push_back(std::make_unique<Cvar>());
    c = all_.back().get();
  } else if (kept) {
    keep = c->value;
  }
  const uint32_t archive = kept ? (c->flags & kArchive) : 0u;
  // A value set before the code registered the name keeps how it was set (explicit, implied,
  // latched): registering it only gives it a meaning.
  const bool was_explicit = kept && c->explicit_set;
  const bool had_latched = kept && c->has_latched;
  std::string latched = kept ? c->latched : std::string();
  auto implied_by = kept ? c->implied_by : decltype(c->implied_by){};
  c->retired = false;
  c->also.clear();
  c->earlier.clear();
  c->layer = layer_;
  define(*c, spec, target);
  c->flags |= archive;
  if (kept) {
    c->explicit_set = was_explicit;
    c->has_latched = had_latched;
    c->latched = std::move(latched);
    c->implied_by = std::move(implied_by);
  }
  if (kept) store(*c, keep);
  if (!kept) c->below = c->value;
  c->revision = 0;
  return *c;
}

// The spec's text, kind, range and field on `c`, at its default (no value of its own).
void Cvars::define(Cvar& c, const CvarSpec& spec, CvarTarget target) {
  c.name = lower(spec.name);
  c.fallback = std::string(spec.fallback);
  c.help = std::string(spec.help);
  c.values = std::string(spec.values);
  c.flags = spec.flags;
  c.kind = spec.kind;
  c.min = spec.min;
  c.max = spec.max;
  c.target = target;
  c.implied_by.clear();
  c.explicit_set = false;
  c.has_latched = false;
  c.latched.clear();
  if (c.kind == CvarKind::boolean) {
    c.min = 0;
    c.max = 1;
  }
  c.value.clear();
  store(c, spec.fallback);
  c.fallback = c.value;  // the fallback as stored (clamped, normalised)
}

Cvar& Cvars::add(const CvarSpec& spec) { return make(spec, std::monostate{}); }
Cvar& Cvars::bind(const CvarSpec& spec, double* field) { return make(spec, field); }
Cvar& Cvars::bind(const CvarSpec& spec, float* field) { return make(spec, field); }
Cvar& Cvars::bind(const CvarSpec& spec, int32_t* field) { return make(spec, field); }
Cvar& Cvars::bind(const CvarSpec& spec, uint32_t* field) { return make(spec, field); }
Cvar& Cvars::bind(const CvarSpec& spec, bool* field) { return make(spec, field); }
Cvar& Cvars::bind(const CvarSpec& spec, std::string* field) { return make(spec, field); }

namespace {

// One bound field from the cvar's value.
void write_target(const Cvar& c, const CvarTarget& t) {
  const double v = c.number;
  if (auto* p = std::get_if<double*>(&t)) **p = v;
  if (auto* p = std::get_if<float*>(&t)) **p = static_cast<float>(v);
  if (auto* p = std::get_if<int32_t*>(&t)) **p = c.integer();
  if (auto* p = std::get_if<uint32_t*>(&t))
    **p = v <= 0 ? 0u : v >= 4294967295.0 ? 4294967295u : static_cast<uint32_t>(v);
  if (auto* p = std::get_if<bool*>(&t)) **p = v != 0;
  if (auto* p = std::get_if<std::string*>(&t)) **p = c.value;
}

}  // namespace

void write_fields(const Cvar& c) {
  write_target(c, c.target);
  for (const CvarTarget& t : c.also) write_target(c, t);
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
  write_fields(c);
  // A meta-cvar's new level reaches its members (bounded: a level never sets a meta).
  if (moved && !metas_.empty() && meta_depth_ < 4)
    if (const Meta* m = meta(c.name)) apply_meta(*m);
}

namespace {

// Two values the same: as numbers when both are ("1" and "1.0"), else as lower-case text.
bool same_value(std::string_view a, std::string_view b) {
  double x = 0, y = 0;
  if (parse_number(a, x) && parse_number(b, y)) return x == y;
  return lower(a) == lower(b);
}

}  // namespace

bool Cvars::lock(std::string_view name, CvarLock lock) {
  Cvar* c = find(name);
  if (c == nullptr) return false;
  lock.wanted = c->lock ? c->lock->wanted : (c->has_latched ? c->latched : c->value);
  c->lock = std::make_shared<CvarLock>(std::move(lock));
  const CvarLock& l = *c->lock;
  std::string value = l.value;
  if (!l.fixed) {
    double v = 0;
    parse_number(l.wanted, v);
    value = number_text(std::clamp(v, l.min, l.max));
  }
  locking_ = true;
  const bool explicit_set = c->explicit_set;
  c->has_latched = false;  // a value waiting is the player's: kept as `wanted`
  c->latched.clear();
  set(*c, value, Source::code);
  c->explicit_set = explicit_set;
  locking_ = false;
  return true;
}

int Cvars::unlock(std::string_view owner) {
  int n = 0;
  for (auto& c : all_) {
    if (!c->lock || c->lock->owner != owner) continue;
    const std::string wanted = c->lock->wanted;
    c->lock.reset();
    const bool explicit_set = c->explicit_set;
    set(*c, wanted, Source::code);
    c->explicit_set = explicit_set;
    ++n;
  }
  return n;
}

SetResult Cvars::set(std::string_view name, std::string_view value, Source source, bool create,
                     uint32_t extra_flags) {
  if (std::string scope, base; split_scoped(name, scope, base))
    return set_scoped(scope, base, value, source);
  if (!valid_name(name)) return SetResult::bad_name;
  Cvar* c = find(name);
  if (c != nullptr && config_ && source != Source::code) {
    // A config: a game's spelling is that game's value; our name under it the common one.
    if (const Synonym* syn = synonym(name); syn != nullptr && syn->part < 0)
      return set_scoped(scope_, c->name, value, source);
    if (!scope_.empty() && c->scoped.count(scope_) != 0) {
      c->common = std::string(value);
      c->flags |= extra_flags & kArchive;
      return SetResult::same;
    }
  }
  if (c == nullptr) {
    if (const Synonym* s = synonym(name)) {  // a part synonym: its word of the target replaced
      std::vector<std::string> w = words_of(s->target->value);
      if (w.size() <= static_cast<size_t>(s->part)) w.resize(static_cast<size_t>(s->part) + 1, "0");
      w[static_cast<size_t>(s->part)] = std::string(value);
      std::string joined;
      for (const std::string& x : w) joined.append(joined.empty() ? "" : " ").append(x);
      return set(*s->target, joined, source);
    }
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
  std::string kept;  // a value clamped into a lock's range
  if (c.lock && !locking_) {
    // LOCKS: whatever asks, the lock decides; the value asked for is the player's, kept for later.
    c.lock->wanted = std::string(value);
    if (c.lock->fixed)
      return same_value(value, c.lock->value) ? SetResult::same : SetResult::locked;
    double v = 0;
    if (parse_number(value, v) && (v < c.lock->min || v > c.lock->max)) {
      kept = number_text(std::clamp(v, c.lock->min, c.lock->max));
      value = kept;
    }
  }
  if (source != Source::code) {
    if ((c.flags & kReadOnly) != 0) return SetResult::read_only;
    if (source == Source::server) {
      if ((c.flags & (kServer | kServerInfo)) == 0 && (c.flags & kCreated) == 0)
        return SetResult::refused;
    } else {
      if ((c.flags & kServer) != 0 && !is_server_) return SetResult::server;
      if ((c.flags & kCheat) != 0 && !cheats()) return SetResult::cheat;
    }
    if (watch_) watch_(c, value, source);
  }
  // Any set but a meta's is the layer under the metas: what a meta turned off restores. One from
  // outside code is explicit, and no meta's level overrides it again.
  c.implied_by.clear();
  if (source != Source::code) c.explicit_set = true;
  if ((c.flags & kLatch) != 0 && source != Source::code) {
    c.below = std::string(value);
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
  c.below = c.value;
  if (!c.scoped.empty() && !applying_) {
    // Typed: the common value, and the loaded scope's own given up for it (SCOPES).
    if (!config_ && source == Source::local) c.scoped.erase(scope_);
    if (c.scoped.count(scope_) == 0) c.common = c.value;
  }
  // sv_cheats turned off: every cheat goes back to its default, so none outlives the permission.
  if (&c == sv_cheats_ && !c.on())
    for (auto& k : all_)
      if ((k->flags & kCheat) != 0 && k->value != k->fallback) {
        store(*k, k->fallback);
        k->below = k->value;
      }
  if (!kept.empty()) return SetResult::locked;  // kept inside a lock's range: its words say so
  return c.revision != before ? SetResult::changed : SetResult::same;
}

namespace {

// Two levels' values the same: as numbers when both are ("1" and "1.0"), else as lower-case text.
bool same_level(std::string_view a, std::string_view b) {
  double x = 0, y = 0;
  if (parse_number(a, x) && parse_number(b, y)) return x == y;
  return lower(a) == lower(b);
}

const MetaLevel* level_of(const Meta& m, std::string_view value) {
  for (const MetaLevel& l : m.levels)
    if (same_level(l.value, value)) return &l;
  return nullptr;
}

struct Node {
  std::string name, value;
};

std::string path_text(const std::vector<Node>& path) {
  std::string out;
  for (const Node& n : path) out.append(out.empty() ? "" : " > ").append(n.name + " " + n.value);
  return out;
}

}  // namespace

const Meta* Cvars::meta(std::string_view name) const {
  const std::string key = lower(name);
  for (const Meta& m : metas_)
    if (m.name == key) return &m;
  return nullptr;
}

std::vector<MetaSet> Cvars::meta_members(const Meta& meta, std::string_view value) const {
  std::vector<MetaSet> out;
  // Depth-first through the includes (declared loop-free), each set in the order written.
  std::function<void(const Meta&, std::string_view, int)> open =
      [&](const Meta& m, std::string_view v, int depth) {
        const MetaLevel* l = level_of(m, v);
        if (l == nullptr || depth > 16) return;
        for (const MetaSet& s : l->sets) {
          if (const Meta* inner = this->meta(s.name)) {
            open(*inner, s.value, depth + 1);
            continue;
          }
          const std::string key = lower(s.name);
          std::erase_if(out, [&key](const MetaSet& o) { return o.name == key; });
          out.push_back({key, s.value});
        }
      };
  open(meta, value, 0);
  return out;
}

bool Cvars::add_meta(std::string_view name, std::vector<MetaLevel> levels, std::string& error) {
  if (!valid_name(name)) {
    error = "'" + std::string(name) + "' is not a cvar name: use 1-63 of a-z 0-9 _ + - . ";
    return false;
  }
  Meta fresh{lower(name), std::move(levels)};
  for (MetaLevel& l : fresh.levels)
    for (MetaSet& s : l.sets) s.name = lower(s.name);
  // Every include followed from every level, the new levels standing in for the old.
  auto find_meta = [&](const std::string& n) -> const Meta* {
    return n == fresh.name ? &fresh : meta(n);
  };
  std::vector<Node> path;
  std::function<bool(const Meta&, const std::string&)> walk = [&](const Meta& m,
                                                                  const std::string& v) -> bool {
    const MetaLevel* l = level_of(m, v);
    if (l == nullptr) {
      error = path_text(path) + ": " + m.name + " has no level " + v +
              " (declare it, or name a level it has)";
      return false;
    }
    for (const MetaSet& s : l->sets) {
      const Meta* inner = find_meta(s.name);
      if (inner == nullptr) continue;
      for (const Node& n : path)
        if (n.name == s.name && same_level(n.value, s.value)) {
          path.push_back({s.name, s.value});
          error = "its levels loop: " + path_text(path) + " (take one of those includes out)";
          return false;
        }
      path.push_back({s.name, s.value});
      if (!walk(*inner, s.value)) return false;
      path.pop_back();
    }
    return true;
  };
  for (const MetaLevel& l : fresh.levels) {
    path = {{fresh.name, l.value}};
    if (!walk(fresh, l.value)) return false;
  }
  Cvar* c = find(fresh.name);
  if (c == nullptr)
    c = &add({.name = fresh.name, .fallback = "0", .flags = kCreated, .kind = CvarKind::text});
  bool replaced = false;
  for (Meta& m : metas_)
    if (m.name == fresh.name) {
      m = fresh;
      replaced = true;
    }
  if (!replaced) metas_.push_back(std::move(fresh));
  apply_meta(*meta(c->name));
  return true;
}

void Cvars::put(Cvar& c, std::string_view text) {
  if ((c.flags & kLatch) != 0) {
    if (text == c.value) {
      c.has_latched = false;
      c.latched.clear();
    } else {
      c.latched = std::string(text);
      c.has_latched = true;
    }
    return;
  }
  store(c, text);
}

void Cvars::apply_meta(const Meta& meta) {
  const Cvar* self = find(meta.name);
  if (self == nullptr) return;
  ++meta_depth_;
  const std::string name = meta.name;  // `meta` may move while members are made
  const std::vector<MetaSet> want = meta_members(meta, self->value);
  // What this meta set and its level no longer does: back to the layers under it.
  for (auto& c : all_) {
    if (c->implied_by != name) continue;
    const bool kept =
        std::any_of(want.begin(), want.end(), [&c](const MetaSet& w) { return w.name == c->name; });
    if (kept) continue;
    c->implied_by.clear();
    put(*c, c->below);
  }
  for (const MetaSet& w : want) {
    Cvar* c = find(w.name);
    if (c == nullptr) {
      c = &add({.name = w.name, .fallback = "", .flags = kCreated, .kind = CvarKind::text});
      c->below.clear();
    }
    if (c->explicit_set) continue;
    c->implied_by = name;
    put(*c, w.value);
  }
  --meta_depth_;
}

SetResult Cvars::reset(Cvar& c, Source source) { return set(c, c.fallback, source); }

int Cvars::apply_latched() {
  int moved = 0;
  for (auto& c : all_) {
    if (!c->has_latched || c->retired) continue;
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
  for (const auto& c : all_)
    if (!c->retired) out.push_back(c.get());
  std::sort(out.begin(), out.end(), [](const Cvar* a, const Cvar* b) { return a->name < b->name; });
  return out;
}

std::vector<Cvar*> Cvars::sorted() {
  std::vector<Cvar*> out;
  out.reserve(all_.size());
  for (auto& c : all_)
    if (!c->retired) out.push_back(c.get());
  std::sort(out.begin(), out.end(), [](const Cvar* a, const Cvar* b) { return a->name < b->name; });
  return out;
}

size_t Cvars::size() const {
  return static_cast<size_t>(
      std::count_if(all_.begin(), all_.end(), [](const auto& c) { return !c->retired; }));
}

}  // namespace ge::console
