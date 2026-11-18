// The cvar registry's layers (cvar.hpp § LAYERS AND ONE NAME, ONE MEANING): a name meeting one an
// earlier layer registered, by what its spec declares (`same`, `own`, or a clash), and a layer
// taken away again.
//
// ge:summary Cvar layers: same-named settings met by declaration, clashes recorded, a layer dropped
// ge:keywords cvar-layer layer drop_layer overlap clash same own set-aside retired one-name
// ge:keywords per-game-cvar game-scope shadow meet
// ge:skill console
// ge:test tests/console_cvar_layers_test.cpp
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
    c.earlier.push_back({layer_, std::move(was)});
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
  earlier.push_back({layer_, std::move(was)});
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
      std::vector<CvarEarlier> rest = std::move(c->earlier);
      rest.pop_back();
      const uint32_t revision = c->revision;
      *c = *was;
      c->earlier = std::move(rest);
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

}  // namespace ge::console
