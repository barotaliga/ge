// Classic remix's settings, its check and the map's items swapped (remix.hpp).
//
// ge:summary Classic remix: the cr_ settings, the two-full-games check, the entity text rewrite
// ge:keywords remix cr_enabled cr_items remix_check remix_entities entity-text spawnflags
// ge:skill shooter classicenhance
// ge:test tests/shooters_edge_remix_test.cpp
#include "shooters_edge/client/remix.hpp"

#include <cstdlib>

#include "classicenhance/profile.hpp"
#include "progs/entity_text.hpp"
#include "shooters_edge/profiles/remix.hpp"

namespace shooters_edge::client {

namespace {

namespace ce = ge::classicenhance;
namespace pf = shooters_edge::profiles;

// The value of `key` in a block ("" when it has none).
std::string_view value_of(const ge::progs::EntityBlock& b, std::string_view key) {
  for (const auto& [k, v] : b.pairs)
    if (k == key) return v;
  return {};
}

void count(RemixReport& r, std::string_view from, std::string_view to) {
  for (RemixSwap& s : r.swaps)
    if (s.from == from && s.to == to) {
      s.count++;
      return;
    }
  r.swaps.push_back({std::string(from), std::string(to), 1});
}

void write_block(const ge::progs::EntityBlock& b, std::string& out) {
  out += "{\n";
  for (const auto& [k, v] : b.pairs) {
    out += '"';
    out += k;
    out += "\" \"";
    out += v;
    out += "\"\n";
  }
  out += "}\n";
}

}  // namespace

void bind_remix(ge::console::Cvars& cvars, RemixSettings* out) {
  cvars.bind({.name = "cr_enabled",
              .fallback = "0",
              .flags = ge::console::kArchive,
              .help = "Whether classic remix is on: one classic game played with another's maps "
                      "and items. It needs two classic games loaded as full games.",
              .kind = ge::console::CvarKind::boolean,
              .values = "0: each game plays with its own maps and items alone (the default); 1: "
                        "the other full game's maps load too, and cr_items may swap the items"},
             &out->enabled);
  cvars.bind({.name = "cr_items",
              .fallback = "0",
              .flags = ge::console::kArchive,
              .help = "Whose items a map is played with under classic remix; the HUD follows "
                      "them.",
              .kind = ge::console::CvarKind::integer,
              .min = 0,
              .max = static_cast<double>(std::size(ce::kProfiles)),
              .values = "0: the played game's own (the default); 1: the classic game's; 2: the "
                        "sequel's"},
             &out->items);
}

RemixState remix_check(const RemixSettings& s, std::string_view played,
                       std::span<const KnownGame> games) {
  RemixState st;
  if (!s.enabled) return st;
  bool played_full = false;
  const KnownGame* other = nullptr;
  for (const KnownGame& g : games) {
    if (g.id == played) {
      played_full = played_full || g.edition == Edition::full;
    } else if (g.edition == Edition::full && other == nullptr) {
      other = &g;
    }
  }
  const ce::Profile* mine = ce::find_profile(played);
  const std::string name = mine != nullptr ? std::string(mine->what) : std::string(played);
  if (!played_full) {
    st.why = "Classic remix needs the full game: " + name + " is loaded from its " +
             "shareware or demo files. Load its full game in SELECT GAME.";
    return st;
  }
  if (other == nullptr) {
    st.why =
        "Classic remix needs a second classic game loaded as a full game. Load one in SELECT "
        "GAME.";
    return st;
  }
  st.on = true;
  st.other = other->id;
  st.other_basedir = other->basedir;
  return st;
}

std::string_view items_game(const RemixSettings& s, std::string_view played) {
  const auto n = static_cast<size_t>(s.items);
  if (s.items <= 0 || n > std::size(ce::kProfiles)) return played;
  return ce::kProfiles[n - 1].id;
}

bool remix_entities(std::string_view text, std::string_view from, std::string_view to,
                    std::string& out, RemixReport* report, std::string& error) {
  RemixReport mine;
  RemixReport& r = report != nullptr ? *report : mine;
  r = {};
  if (from == to) {
    out.assign(text);
    return true;
  }
  std::vector<ge::progs::EntityBlock> blocks;
  const std::string owned(text);
  if (!ge::progs::parse_entity_text(owned.c_str(), blocks, error)) return false;
  out.clear();
  out.reserve(text.size());
  for (ge::progs::EntityBlock& b : blocks) {
    const std::string_view cls = value_of(b, "classname");
    if (!pf::is_item_class(cls)) {
      write_block(b, out);
      continue;
    }
    r.items++;
    const int32_t flags = std::atoi(std::string(value_of(b, "spawnflags")).c_str());
    const pf::RemixRow* row = pf::remix_row(from, to, cls, flags);
    if (row == nullptr || row->becomes.empty()) {
      r.dropped++;
      count(r, cls, {});
      continue;
    }
    r.swapped++;
    count(r, cls, row->becomes);
    const int32_t kept = flags & ~pf::kItemBits;
    const int32_t now = kept | row->bits;
    bool has_flags = false;
    for (auto& [k, v] : b.pairs) {
      if (k == "classname") v = std::string(row->becomes);
      if (k == "spawnflags") {
        v = std::to_string(now);
        has_flags = true;
      }
    }
    if (!has_flags && now != 0) b.pairs.emplace_back("spawnflags", std::to_string(now));
    write_block(b, out);
  }
  return true;
}

std::string remix_line(const RemixReport& r, std::string_view to) {
  const ce::Profile* p = ce::find_profile(to);
  const std::string whose = p != nullptr ? std::string(p->what) : std::string(to);
  std::string line = "Classic remix: " + std::to_string(r.swapped) + " of the map's " +
                     std::to_string(r.items) + " items swapped for " + whose + "'s";
  if (r.dropped > 0) line += ", " + std::to_string(r.dropped) + " with no match are left out";
  line += ".";
  return line;
}

}  // namespace shooters_edge::client
