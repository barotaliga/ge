// The cvar registry's layers (cvar.hpp § LAYERS AND ONE NAME, ONE MEANING): a name meeting one an
// earlier layer registered, by what its spec declares (`same`, `own`, or a clash), and a layer
// taken away again; and SCOPES, a setting's value per game over its common one.
// voice: agent (a clash is read by tests and agents, never printed to a player)
//
// ge:summary Cvar layers: same-named settings met by declaration, clashes recorded, a layer dropped
// ge:keywords cvar-layer layer drop_layer overlap clash same own set-aside retired one-name
// ge:keywords per-game-cvar game-scope shadow meet scope scoped set_scope set_scoped override
// ge:keywords per-game-override unset_scoped split_scoped
// ge:skill console
// ge:test tests/console_cvar_layers_test.cpp
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "console/cvar.hpp"

namespace ge::console {

void Cvars::set_layer(std::string_view layer) { layer_ = lower(layer); }

std::string clash_text(const CvarClash& c) {
  const auto named = [](const std::string& l) { return l.empty() ? std::string("(none)") : l; };
  return "cvar " + c.name + ": layer " + named(c.layer) + " registered the name layer " +
         named(c.earlier_layer) + " already has (" + c.why + "), so it was not merged: \"" +
         c.earlier_help + "\" stays, \"" + c.help +
         "\" is not reachable; next: give the later spec .overlap = Overlap::same if both are one "
         "setting, Overlap::own if it is the game's own meaning of the name, or rename it "
         "(docs/porting-games.md § One name, one meaning)";
}

Cvar& Cvars::meet(Cvar& c, const CvarSpec& spec, CvarTarget target) {
  const auto clash = [&](std::string why) -> Cvar& {
    clashes_.push_back({.name = c.name,
                        .earlier_layer = c.layer,
                        .layer = layer_,
                        .earlier_help = c.help,
                        .help = std::string(spec.help),
                        .why = std::move(why)});
    return c;
  };
  if (spec.overlap == Overlap::none) return clash("no overlap declared");
  // A copy of the definition as it stands, to come back when this layer is dropped.
  auto was = std::make_shared<Cvar>(c);
  was->earlier.clear();
  if (spec.overlap == Overlap::same) {
    // One setting: it must read its text the same way, or it is not the same.
    if (spec.kind != c.kind) return clash("same declared, but another kind");
    c.earlier.push_back({layer_, std::move(was), Overlap::same});
    const std::string value = c.value;
    const bool keep = c.explicit_set;
    c.fallback = std::string(spec.fallback);
    c.help = std::string(spec.help);
    c.values = std::string(spec.values);
    c.flags = spec.flags | (c.flags & kArchive);
    c.min = c.kind == CvarKind::boolean ? 0 : spec.min;
    c.max = c.kind == CvarKind::boolean ? 1 : spec.max;
    if (target.index() != 0) c.also.push_back(target);
    c.layer = layer_;
    store(c, spec.fallback);
    c.fallback = c.value;
    if (keep) store(c, value);  // the player's own value stays, under the new range
    c.below = c.value;
    ++c.revision;
    return c;
  }
  // The game's own meaning: the earlier setting waits, unreachable, and its field keeps its value.
  std::vector<CvarEarlier> earlier = std::move(c.earlier);
  earlier.push_back({layer_, std::move(was), Overlap::own});
  const uint32_t revision = c.revision;
  c.also.clear();
  c.layer = layer_;
  define(c, spec, target);
  c.earlier = std::move(earlier);
  c.below = c.value;
  c.revision = revision + 1;
  return c;
}

int Cvars::drop_layer(std::string_view layer) {
  const std::string key = lower(layer);
  int touched = 0;
  for (auto& c : all_) {
    bool hit = false;
    // What the layer replaced or set aside comes back, newest first, with its value and fields.
    while (!c->earlier.empty() && c->earlier.back().layer == key) {
      std::shared_ptr<Cvar> was = std::move(c->earlier.back().was);
      // One setting (`same`): a value the player set stays, under the earlier range and default.
      const bool keep = c->earlier.back().how == Overlap::same && c->explicit_set;
      const std::string value = c->value;
      std::vector<CvarEarlier> rest = std::move(c->earlier);
      rest.pop_back();
      const uint32_t revision = c->revision;
      std::map<std::string, std::string> scoped = std::move(c->scoped);  // the player's, kept
      std::string common = std::move(c->common);
      *c = *was;
      c->scoped = std::move(scoped);
      c->common = std::move(common);
      c->earlier = std::move(rest);
      if (keep) {
        store(*c, value);
        c->explicit_set = true;
      }
      c->revision = revision + 1;
      write_fields(*c);
      hit = true;
    }
    if (!hit && c->layer == key && !c->retired) {
      c->retired = true;
      c->has_latched = false;
      c->latched.clear();
      hit = true;
    }
    if (hit) ++touched;
  }
  // Its other names for settings (a game's own spelling of one) go with it.
  const size_t names = synonyms_.size();
  std::erase_if(synonyms_, [&key](const Synonym& s) { return s.layer == key; });
  return touched + static_cast<int>(names - synonyms_.size());
}

// SCOPES (cvar.hpp).

bool Cvars::split_scoped(std::string_view name, std::string& scope, std::string& base) const {
  const size_t colon = name.find(':');
  if (colon == std::string_view::npos || colon == 0 || colon + 1 >= name.size()) return false;
  scope = lower(name.substr(0, colon));
  base = lower(name.substr(colon + 1));
  if (scope == "game") scope = scope_;
  return !scope.empty() && valid_name(scope) && valid_name(base);
}

void Cvars::set_scope(std::string_view scope) {
  scope_ = lower(scope);
  for (auto& c : all_) {
    if (c->scoped.empty()) continue;
    const auto it = c->scoped.find(scope_);
    const std::string& want = it != c->scoped.end() ? it->second : c->common;
    if (want == c->value) continue;
    store(*c, want);
    c->below = c->value;
  }
}

SetResult Cvars::set_scoped(std::string_view scope, std::string_view name, std::string_view value,
                            Source source) {
  Cvar* c = find(name);
  if (c == nullptr) return SetResult::unknown;
  const std::string key = lower(scope);
  if (c->scoped.empty()) c->common = c->value;
  c->scoped[key] = std::string(value);
  if (key != scope_) return SetResult::changed;  // kept for when that game loads
  applying_ = true;
  const SetResult r = set(*c, value, source);
  applying_ = false;
  c->scoped[key] = c->value;  // as stored: clamped, normalised
  return r;
}

bool Cvars::unset_scoped(std::string_view scope, std::string_view name) {
  Cvar* c = find(name);
  if (c == nullptr || c->scoped.erase(lower(scope)) == 0) return false;
  if (lower(scope) == scope_) {
    store(*c, c->common);
    c->below = c->value;
  }
  return true;
}

}  // namespace ge::console
