// The episodes in order (campaign_episodes.hpp states the rules).
//
// ge:summary The start map's bars kept shut for episodes not yet open; the episodes open
// ge:keywords episodes_open shut_episode_bars episode_bars_message entity-text rewrite
// ge:skill shooter progs
// ge:test tests/shooters_edge_campaign_episodes_test.cpp
#include "shooters_edge/server/campaign_episodes.hpp"

#include <cstdlib>

#include "brush/read.hpp"

namespace shooters_edge::server {

namespace {

constexpr float kBarsGap = 4;  // units: a bar this close to a named one is one of the same set

// A brush entity's model number ("*12" → 12), or -1.
int model_number(const ge::brush::Entity& e) {
  const std::string& m = e.get("model");
  if (m.size() < 2 || m[0] != '*') return -1;
  return std::atoi(m.c_str() + 1);
}

// The centre of a brush entity's model, false when it has none on this map.
bool centre(const MapSpec& map, const ge::brush::Entity& e, float out[3]) {
  const int n = model_number(e);
  if (n <= 0 || n >= static_cast<int>(map.models.size())) return false;
  const auto& m = map.models[static_cast<size_t>(n)];
  out[0] = (m.mins.x + m.maxs.x) * 0.5f;
  out[1] = (m.mins.y + m.maxs.y) * 0.5f;
  out[2] = (m.mins.z + m.maxs.z) * 0.5f;
  return true;
}

void set_key(ge::brush::Entity& e, const std::string& key, const std::string& value) {
  for (auto& [k, v] : e.pairs)
    if (k == key) {
      v = value;
      return;
    }
  e.pairs.emplace_back(key, value);
}

void drop_key(ge::brush::Entity& e, const std::string& key) {
  for (size_t i = 0; i < e.pairs.size();)
    if (e.pairs[i].first == key)
      e.pairs.erase(e.pairs.begin() + static_cast<std::ptrdiff_t>(i));
    else
      i++;
}

// Within `gap` units of each other on every axis (the brushes' bounds as the map file stores them).
bool beside(const MapSpec& map, const ge::brush::Entity& a, const ge::brush::Entity& b, float gap) {
  const int na = model_number(a), nb = model_number(b);
  const int count = static_cast<int>(map.models.size());
  if (na <= 0 || nb <= 0 || na >= count || nb >= count) return false;
  const auto& ma = map.models[static_cast<size_t>(na)];
  const auto& mb = map.models[static_cast<size_t>(nb)];
  const auto apart = [gap](float lo_a, float hi_a, float lo_b, float hi_b) {
    return lo_a > hi_b + gap || lo_b > hi_a + gap;
  };
  return !apart(ma.mins.x, ma.maxs.x, mb.mins.x, mb.maxs.x) &&
         !apart(ma.mins.y, ma.maxs.y, mb.mins.y, mb.maxs.y) &&
         !apart(ma.mins.z, ma.maxs.z, mb.mins.z, mb.maxs.z);
}

// Every unnamed door beside a door named `name` gets that name, so it opens only when fired.
void shut_neighbours(const MapSpec& map, std::vector<ge::brush::Entity>& ents,
                     const std::string& name, EpisodeBars& out) {
  for (size_t j = 0; j < ents.size(); j++) {
    ge::brush::Entity& d = ents[j];
    if (d.get("classname") != "func_door" || !d.get("targetname").empty() ||
        !d.get("health").empty())
      continue;
    for (const ge::brush::Entity& named : ents)
      if (named.get("classname") == "func_door" && named.get("targetname") == name &&
          beside(map, d, named, kBarsGap)) {
        set_key(d, "targetname", name);
        out.named.push_back(static_cast<int>(j));
        break;
      }
  }
}

std::string entity_text(const std::vector<ge::brush::Entity>& ents) {
  std::string out;
  for (const ge::brush::Entity& e : ents) {
    out += "{\n";
    for (const auto& [k, v] : e.pairs) {
      out += "\"";
      out += k;
      out += "\" \"";
      out += v;
      out += "\"\n";
    }
    out += "}\n";
  }
  return out;
}

}  // namespace

uint32_t episodes_open(uint32_t done, int episodes) {
  uint32_t open = 0;
  for (int k = 0; k < episodes && k < 31; k++) {
    const uint32_t bit = 1u << k;
    const bool before_done = k == 0 || (done & (bit >> 1)) != 0;
    if ((done & bit) == 0 && before_done) open |= bit;
  }
  return open;
}

std::string episode_bars_message(int episode) {
  return "These bars open once\\nepisode " + std::to_string(episode) + " is done.";
}

bool shut_episode_bars(MapSpec& map, const EpisodeOrder& order, uint32_t done, bool registered,
                       EpisodeBars& out, std::string& error) {
  out = EpisodeBars{};
  if (!registered || order.start_map.empty() || map.name != order.start_map) return true;
  std::vector<ge::brush::Entity> ents;
  std::string why;
  if (!ge::brush::parse_entities(map.entities, &ents, &why)) {
    error = "episodes: the start map's entities do not parse (" + why + ")";
    return false;
  }
  const int count = static_cast<int>(order.first_maps.size());
  // Each episode's way in: the level change to its first map.
  std::vector<std::vector<float>> ways(static_cast<size_t>(count));
  for (const ge::brush::Entity& e : ents) {
    if (e.get("classname") != "trigger_changelevel") continue;
    for (int k = 0; k < count; k++) {
      float c[3];
      if (e.get("map") == order.first_maps[static_cast<size_t>(k)] && centre(map, e, c))
        ways[static_cast<size_t>(k)].assign(c, c + 3);
    }
  }
  const uint32_t open = episodes_open(done, count);
  bool changed = false;
  for (size_t i = 0; i < ents.size(); i++) {
    ge::brush::Entity& e = ents[i];
    // Only the full-game triggers that open something (the bars); the ones that take away the
    // shareware signs (a killtarget alone) stay.
    if (e.get("classname") != "trigger_onlyregistered" || e.get("target").empty()) continue;
    float c[3];
    int best = -1;
    float best_d = 0;
    if (centre(map, e, c)) {
      for (int k = 0; k < count; k++) {
        const std::vector<float>& w = ways[static_cast<size_t>(k)];
        if (w.empty()) continue;
        const float dx = w[0] - c[0], dy = w[1] - c[1], dz = w[2] - c[2];
        const float d = dx * dx + dy * dy + dz * dz;
        if (best < 0 || d < best_d) {
          best = k;
          best_d = d;
        }
      }
    }
    if (best < 0 || (open & (1u << best)) != 0 || (done & (1u << best)) != 0) {
      out.left_open.push_back(static_cast<int>(i));
      continue;
    }
    // Not open yet: a message trigger with nothing to fire, so the bars stay shut. An unnamed
    // bar beside the ones it opened (one that would open on a player's approach) is given their
    // name, so it waits to be fired as they do, and never is.
    shut_neighbours(map, ents, e.get("target"), out);
    set_key(e, "classname", "trigger_multiple");
    drop_key(e, "target");
    drop_key(e, "killtarget");
    drop_key(e, "delay");
    set_key(e, "message", episode_bars_message(best));
    set_key(e, "wait", "2");
    out.shut.push_back(static_cast<int>(i));
    out.episode.push_back(best);
    changed = true;
  }
  if (changed) map.entities = entity_text(ents);
  return true;
}

}  // namespace shooters_edge::server
