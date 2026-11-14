// Classic remix's settings, its check and the map's items swapped (remix.hpp).
//
// ge:summary Classic remix: the cr_ settings, the two-full-games check, the entity text rewrite
// ge:keywords remix cr_enabled cr_items remix_check remix_entities entity-text spawnflags
// ge:skill shooter classicenhance
// ge:test tests/shooters_edge_remix_test.cpp
#include "shooters_edge/client/remix.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>

#include "brush/cells.hpp"
#include "brush/read.hpp"
#include "classicenhance/profile.hpp"
#include "progs/entity_text.hpp"
#include "shooters_edge/client/game_folder.hpp"
#include "shooters_edge/client/listen.hpp"
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
  cvars.bind({.name = "cr_strong_min",
              .fallback = "1",
              .flags = ge::console::kArchive,
              .help = "How many strong weapons (the rocket launcher's kind) a remixed map keeps at "
                      "least, when it has two weapons or more.",
              .kind = ge::console::CvarKind::integer,
              .min = 0,
              .max = 8,
              .values = "0: as the item table gives them; 1: one at least (the default); more"},
             &out->balance.strong_min);
  cvars.bind({.name = "cr_strong_share",
              .fallback = "0.4",
              .flags = ge::console::kArchive,
              .help = "The share of a remixed map's weapons that may be strong at most; the "
                      "rest are lowered.",
              .kind = ge::console::CvarKind::number,
              .min = 0,
              .max = 1,
              .values = "0 to 1: 0.4 the default (two strong of five weapons); 1 no limit"},
             &out->balance.strong_share);
  cvars.bind({.name = "cr_strong_same",
              .fallback = "2",
              .flags = ge::console::kArchive,
              .help = "How many copies of one strong weapon a remixed map keeps at most; the rest "
                      "are lowered, the one nearest a strong armour first.",
              .kind = ge::console::CvarKind::integer,
              .min = 0,
              .max = 8,
              .values = "0: no limit; 2: two of a kind at most (the default)"},
             &out->balance.strong_same);
  cvars.bind({.name = "cr_pool_radius",
              .fallback = "160",
              .flags = ge::console::kArchive,
              .help = "How near one another a map's small pickups lie to be gathered into the "
                      "other game's larger ones.",
              .kind = ge::console::CvarKind::number,
              .min = 0,
              .max = 1024,
              .values = "units; 0 gathers none (each laid down alone), 160 the default"},
             &out->balance.pool_radius);
}

std::vector<KnownGame> known_games(const std::string& prefs, std::string_view played,
                                   const std::string& played_basedir) {
  std::vector<KnownGame> out;
  const auto kept = remembered_installs(prefs);
  for (const ce::Profile& p : ce::kProfiles) {
    KnownGame g;
    g.id = std::string(p.id);
    if (p.id == played)
      g.basedir = played_basedir;
    else if (const auto it = kept.find(p.id); it != kept.end())
      g.basedir = it->second;
    std::string base, why;
    if (g.basedir.empty() || !check_game_folder(g.basedir, &base, &why, p)) continue;
    g.basedir = base;
    g.edition = edition_of(base, p);
    out.push_back(std::move(g));
  }
  return out;
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

namespace {

// One item of the map as the remix carries it.
struct Item {
  size_t block = 0;
  std::string cls;  // the map's class name
  int32_t flags = 0;
  float at[3] = {0, 0, 0};
  std::string becomes;  // "" dropped
  int32_t bits = 0;
  const pf::PoolItem* pool = nullptr;
};

std::string where(const Item& it) {
  char b[64];
  std::snprintf(b, sizeof b, "(%.0f %.0f %.0f)", static_cast<double>(it.at[0]),
                static_cast<double>(it.at[1]), static_cast<double>(it.at[2]));
  return b;
}

// The weapons of the map balanced in `to`'s tiers (profiles/remix.hpp).
void balance_weapons(std::vector<Item>& items, std::string_view to, const pf::RemixBalance& bal,
                     RemixReport& r) {
  std::vector<size_t> weapons;
  for (size_t i = 0; i < items.size(); i++)
    if (!items[i].becomes.empty() && pf::weapon_tier(to, items[i].becomes) > 0)
      weapons.push_back(i);
  const auto tier = [&](size_t i) { return pf::weapon_tier(to, items[i].becomes); };
  const auto strong_count = [&] {
    int n = 0;
    for (size_t i : weapons) n += tier(i) >= pf::kStrongTier ? 1 : 0;
    return n;
  };
  r.weapons = static_cast<int>(weapons.size());
  r.strong_by_role = strong_count();
  const int n = static_cast<int>(weapons.size());
  const int floor = n >= 2 ? std::min(bal.strong_min, n) : 0;
  const int cap = std::max(floor, static_cast<int>(std::ceil(bal.strong_share * float(n))));
  // Lowered: to the target's best weak weapon the map has fewest of (the first in the table on a
  // tie), the strong copy nearest a strong pickup first (`kStrongPickups`), else the last in it.
  int weak_tier = 0;
  for (const pf::WeaponTier& w : pf::kWeaponTiers)
    if (w.game == to && w.tier < pf::kStrongTier) weak_tier = std::max(weak_tier, w.tier);
  const auto weak_pick = [&] {
    std::string_view pick;
    int fewest = 1 << 30;
    for (const pf::WeaponTier& w : pf::kWeaponTiers) {
      if (w.game != to || w.tier != weak_tier) continue;
      int there = 0;
      for (size_t i : weapons) there += items[i].becomes == w.class_name ? 1 : 0;
      if (there < fewest) {
        fewest = there;
        pick = w.class_name;
      }
    }
    return pick;
  };
  const auto near_strong = [&](size_t i) {  // squared distance to the nearest strong pickup
    float best = -1;
    for (const Item& p : items) {
      bool strong = false;
      for (const pf::StrongPickup& s : pf::kStrongPickups)
        strong = strong || (s.game == to && p.becomes == s.class_name &&
                            (s.bits < 0 || (p.bits & pf::kItemBits) == s.bits));
      if (!strong) continue;
      float d2 = 0;
      for (int a = 0; a < 3; a++) {
        const float d = p.at[a] - items[i].at[a];
        d2 += d * d;
      }
      if (best < 0 || d2 < best) best = d2;
    }
    return best;
  };
  // One strong copy lowered, of `only` ("" any strong one); false when there is none.
  const auto lower = [&](std::string_view only, const std::string& why) {
    size_t pick = weapons.size();
    float pick_d2 = -1;
    for (size_t k = weapons.size(); k-- > 0;) {
      const Item& it = items[weapons[k]];
      if (tier(weapons[k]) < pf::kStrongTier || (!only.empty() && it.becomes != only)) continue;
      const float d2 = near_strong(weapons[k]);
      if (pick == weapons.size() || (d2 >= 0 && (pick_d2 < 0 || d2 < pick_d2))) {
        pick = k;
        pick_d2 = d2;
      }
    }
    const std::string_view weak = weak_pick();
    if (pick == weapons.size() || weak.empty()) return false;
    Item& it = items[weapons[pick]];
    r.notes.push_back(it.cls + " " + where(it) + ": " + it.becomes + " lowered to " +
                      std::string(weak) + ", " + why +
                      (pick_d2 >= 0 ? ", the copy nearest a strong armour" : ""));
    it.becomes = std::string(weak);
    return true;
  };
  // Too many of one kind: the copies past `strong_same` lowered.
  for (const pf::WeaponTier& w : pf::kWeaponTiers) {
    if (w.game != to || w.tier < pf::kStrongTier || bal.strong_same <= 0) continue;
    const auto copies = [&] {
      int c = 0;
      for (size_t i : weapons) c += items[i].becomes == w.class_name ? 1 : 0;
      return c;
    };
    while (copies() > bal.strong_same &&
           lower(w.class_name,
                 "the map had more than " + std::to_string(bal.strong_same) + " of it")) {
    }
  }
  // Raised: the best weak weapon (first in the map on a tie) to the strong one fewest lie as.
  while (strong_count() < floor) {
    size_t best = weapons.size();
    for (size_t k = 0; k < weapons.size(); k++)
      if (tier(weapons[k]) < pf::kStrongTier &&
          (best == weapons.size() || tier(weapons[k]) > tier(weapons[best])))
        best = k;
    if (best == weapons.size()) break;
    std::string_view pick;
    int fewest = 1 << 30;
    for (const pf::WeaponTier& w : pf::kWeaponTiers) {
      if (w.game != to || w.tier != pf::kStrongTier) continue;
      int there = 0;
      for (size_t i : weapons) there += items[i].becomes == w.class_name ? 1 : 0;
      if (there < fewest) {
        fewest = there;
        pick = w.class_name;
      }
    }
    Item& it = items[weapons[best]];
    r.notes.push_back(it.cls + " " + where(it) + ": " + it.becomes + " raised to " +
                      std::string(pick) + ", the map had too few strong weapons");
    it.becomes = std::string(pick);
  }
  while (strong_count() > cap && lower("", "the map had too many strong weapons")) {
  }
  r.strong = strong_count();
}

// The pools: small pickups near one another added up and laid down as the target's own sizes.
void lay_pools(std::vector<Item>& items, std::string_view to, const pf::RemixBalance& bal,
               RemixReport& r) {
  const float r2 = bal.pool_radius * bal.pool_radius;
  std::vector<bool> taken(items.size(), false);
  for (size_t i = 0; i < items.size(); i++) {
    if (items[i].pool == nullptr || taken[i]) continue;
    // The pool: every pickup of its kind within the radius of one already in it.
    std::vector<size_t> members{i};
    taken[i] = true;
    for (size_t m = 0; m < members.size(); m++)
      for (size_t j = i + 1; j < items.size(); j++) {
        if (taken[j] || items[j].pool == nullptr || items[j].pool->pool != items[i].pool->pool)
          continue;
        float d2 = 0;
        for (int a = 0; a < 3; a++) {
          const float d = items[j].at[a] - items[members[m]].at[a];
          d2 += d * d;
        }
        if (d2 <= r2) {
          taken[j] = true;
          members.push_back(j);
        }
      }
    int total = 0;
    for (size_t m : members) total += items[m].pool->amount;
    r.pooled += static_cast<int>(members.size());
    for (size_t m : members) items[m].becomes.clear();
    std::vector<const pf::PoolOutput*> outs;  // the target's sizes, largest first (table order)
    for (const pf::PoolOutput& o : pf::kPoolOutputs)
      if (o.to == to && o.pool == items[i].pool->pool) outs.push_back(&o);
    if (outs.empty()) continue;
    const int smallest = outs.back()->amount;
    std::string laid;
    int left = total;
    for (size_t m = 0; m < members.size() && 2 * left >= smallest; m++) {
      const pf::PoolOutput* pick = outs.back();
      for (const pf::PoolOutput* o : outs)
        if (o->amount <= left) {
          pick = o;
          break;
        }
      items[members[m]].becomes = std::string(pick->class_name);
      items[members[m]].bits = pick->bits;
      left -= pick->amount;
      r.laid++;
      laid += (laid.empty() ? "" : " + ") + std::to_string(pick->amount);
    }
    r.notes.push_back(std::to_string(members.size()) + " " + std::string(items[i].pool->pool) +
                      " pickups near " + where(items[i]) + " worth " + std::to_string(total) +
                      " laid down as " + (laid.empty() ? "nothing" : laid));
  }
}

}  // namespace

bool remix_entities(std::string_view text, std::string_view from, std::string_view to,
                    std::string& out, RemixReport* report, std::string& error,
                    const pf::RemixBalance& balance) {
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
  // By role, then the pools, then the weapons weighed.
  std::vector<Item> items;
  for (size_t bi = 0; bi < blocks.size(); bi++) {
    const std::string_view cls = value_of(blocks[bi], "classname");
    if (!pf::is_item_class(cls)) continue;
    Item it;
    it.block = bi;
    it.cls = std::string(cls);
    it.flags = std::atoi(std::string(value_of(blocks[bi], "spawnflags")).c_str());
    std::sscanf(std::string(value_of(blocks[bi], "origin")).c_str(), "%f %f %f", &it.at[0],
                &it.at[1], &it.at[2]);
    it.pool = pf::pool_item(from, to, cls);
    if (it.pool == nullptr) {
      if (const pf::RemixRow* row = pf::remix_row(from, to, cls, it.flags)) {
        it.becomes = std::string(row->becomes);
        it.bits = row->bits;
      }
    }
    items.push_back(std::move(it));
  }
  r.items = static_cast<int>(items.size());
  lay_pools(items, to, balance, r);
  balance_weapons(items, to, balance, r);
  std::vector<const Item*> by_block(blocks.size(), nullptr);
  for (const Item& it : items) {
    by_block[it.block] = &it;
    count(r, it.cls, it.becomes);
    (it.becomes.empty() ? r.dropped : r.swapped)++;
  }
  out.clear();
  out.reserve(text.size());
  for (size_t bi = 0; bi < blocks.size(); bi++) {
    ge::progs::EntityBlock& b = blocks[bi];
    const Item* it = by_block[bi];
    if (it == nullptr) {
      write_block(b, out);
      continue;
    }
    if (it->becomes.empty()) continue;
    const int32_t now = (it->flags & ~pf::kItemBits) | it->bits;
    bool has_flags = false;
    for (auto& [k, v] : b.pairs) {
      if (k == "classname") v = it->becomes;
      if (k == "spawnflags") {
        v = std::to_string(now);
        has_flags = true;
      }
    }
    if (!has_flags && now != 0) b.pairs.emplace_back("spawnflags", std::to_string(now));
    // The middle of its floor kept (profiles::item_foot), a little above it.
    const float* was = pf::item_foot(from, it->cls);
    const float* is = pf::item_foot(to, it->becomes);
    if (was != nullptr && is != nullptr) {
      float at[3];
      for (int a = 0; a < 3; a++) at[a] = it->at[a] + was[a] - is[a];
      at[2] += pf::kFootLift;
      char place[96];
      std::snprintf(place, sizeof place, "%g %g %g", static_cast<double>(at[0]),
                    static_cast<double>(at[1]), static_cast<double>(at[2]));
      for (auto& [k, v] : b.pairs)
        if (k == "origin") v = place;
    }
    write_block(b, out);
  }
  return true;
}

std::string remix_line(const RemixReport& r, std::string_view to) {
  const ce::Profile* p = ce::find_profile(to);
  const std::string whose = p != nullptr ? std::string(p->what) : std::string(to);
  const int unmatched = r.dropped - (r.pooled - r.laid);
  std::string line =
      "Classic remix: the map's " + std::to_string(r.items) + " items swapped for " + whose + "'s";
  if (r.pooled > 0)
    line +=
        ", " + std::to_string(r.pooled) + " small pickups gathered into " + std::to_string(r.laid);
  if (unmatched > 0) line += ", " + std::to_string(unmatched) + " with no match left out";
  if (r.strong != r.strong_by_role)
    line += ", the weapons rebalanced to " + std::to_string(r.strong) + " strong of " +
            std::to_string(r.weapons);
  line += ".";
  return line;
}

bool open_remix(RemixFiles& r, const RemixState& state, std::string_view played,
                const std::string& basedir, const std::string& game, std::string& error) {
  const pf::RemixBalance keep = r.balance;
  r = RemixFiles{};
  r.balance = keep;
  r.played = std::string(played);
  if (!state.on) return true;
  const ce::Profile* other = ce::find_profile(state.other);
  const ce::Profile* mine = ce::find_profile(played);
  if (other == nullptr || mine == nullptr) {
    error = "Classic remix does not know the game " + state.other + ".";
    return false;
  }
  std::string why;
  SearchPath own;
  if (!open_game_dir(state.other_basedir, "", r.files, why, *other) ||
      !open_game_dir(basedir, game, own, why, *mine)) {
    error = "Classic remix could not open the games' files: " + why;
    r.files = {};
    return false;
  }
  for (ge::vfs::Source& src : own.vfs.sources) {  // the played game's over the other's
    const bool ok =
        src.kind == ge::vfs::SourceKind::directory
            ? ge::vfs::add_directory(&r.files.vfs, src.path, src.game, &why)
            : ge::vfs::add_archive(&r.files.vfs, src.path, std::move(src.bytes), src.game, &why);
    if (!ok) {
      error = "Classic remix could not open the games' files: " + why;
      r.files = {};
      return false;
    }
  }
  r.files.folder = own.folder;
  r.state = state;
  return true;
}

std::unique_ptr<LoadedMap> load_remixed_map(const SearchPath& files, const std::string& name,
                                            const RemixFiles* remix, std::string& error,
                                            std::string* said) {
  std::unique_ptr<LoadedMap> own = load_named_map(files, name, error);
  if (own || remix == nullptr || !remix->state.on || name == "arena") return own;
  std::string why;
  std::unique_ptr<LoadedMap> m = load_named_map(remix->files, name, why);
  if (!m) return nullptr;  // the played game's own refusal stands
  const ce::Profile* mine = ce::find_profile(remix->played);
  const std::string whose = mine != nullptr ? std::string(mine->what) : remix->played;
  // The sequel's rules trace brushes: a map of the classic family is given them, its point
  // tree's cells made brushes (brush/cells.hpp).
  std::shared_ptr<ge::brush::Map> cells;
  if (m->spec.brushes == nullptr && remix->played != "classic") {
    if (!m->from_file) {
      error = "Classic remix cannot play " + name + " in " + whose + ".";
      return nullptr;
    }
    cells = std::make_shared<ge::brush::Map>(m->bsp.map);
    ge::brush::cells_as_brushes(*cells);
  }
  RemixReport rep;
  std::string text;
  if (!remix_entities(m->spec.entities, remix->state.other, remix->played, text, &rep, why,
                      remix->balance)) {
    error = "Classic remix could not read " + name + "'s items: " + why;
    return nullptr;
  }
  if (cells) {
    if (!ge::brush::parse_entities(text, &cells->entities, &why)) {
      error = "Classic remix could not read " + name + "'s items: " + why;
      return nullptr;
    }
    cells->entity_text = text;
    m->spec.brushes = std::move(cells);
  }
  m->spec.entities = std::move(text);
  m->from_game = remix->state.other;
  m->program_rules = m->spec.brushes != nullptr && remix->played == "classic";
  if (said != nullptr) *said = remix_line(rep, remix->played);
  return m;
}

}  // namespace shooters_edge::client
