// The client's record of a campaign (campaign_view.hpp).
//
// ge:summary Reads a single-player program's client messages into the campaign's record
// ge:keywords read_campaign join_stats typed_chars completed_time intermission finale
// ge:skill shooter ui
// ge:test tests/shooters_edge_campaign_view_test.cpp
#include "shooters_edge/client/campaign_view.hpp"

#include <algorithm>
#include <cmath>

namespace shooters_edge::client {

void join_stats(CampaignView& v, float total_secrets, float total_monsters, float secrets,
                float monsters) {
  v.total_secrets = total_secrets;
  v.total_monsters = total_monsters;
  v.secrets = secrets;
  v.monsters = monsters;
}

void read_campaign(CampaignView& v, std::span<const uint8_t> bytes, double time) {
  namespace svc = campaign_svc;
  size_t i = 0;
  // A string from `i`: up to its NUL (or the buffer's end), the NUL stepped over.
  const auto string = [&] {
    std::string s;
    while (i < bytes.size() && bytes[i] != 0) s.push_back(static_cast<char>(bytes[i++]));
    if (i < bytes.size()) i++;
    return s;
  };
  while (i < bytes.size()) {
    const uint8_t id = bytes[i++];
    switch (id) {
      case svc::intermission:
        v.end = LevelEnd::intermission;
        v.ended_at = time;
        break;
      case svc::finale:
      case svc::cutscene:
        v.end = id == svc::finale ? LevelEnd::finale : LevelEnd::cutscene;
        v.text = string();
        v.text_at = time;
        break;
      case svc::sellscreen:
        v.sellscreen = true;
        break;
      case svc::cdtrack:
        if (i + 2 > bytes.size()) {
          i = bytes.size();
          break;
        }
        v.cdtrack = bytes[i];
        v.cdloop = bytes[i + 1];
        i += 2;
        break;
      case svc::killedmonster:
        v.monsters += 1;
        break;
      case svc::foundsecret:
        v.secrets += 1;
        break;
      case svc::setangle:
        if (i + 3 > bytes.size()) {
          i = bytes.size();
          break;
        }
        for (int k = 0; k < 3; k++)
          v.angles[k] = static_cast<float>(bytes[i + static_cast<size_t>(k)]) * (360.0f / 256.0f);
        v.turned = true;
        i += 3;
        break;
      default:
        v.unread += static_cast<int>(bytes.size() - i + 1);
        if (v.unknown.empty())
          v.unknown = "message " + std::to_string(id) + " (" +
                      std::to_string(bytes.size() - i + 1) + " bytes left unread)";
        return;
    }
  }
}

void read_effects(CampaignView& v, std::span<const uint8_t> bytes, std::vector<Effect>& out) {
  size_t i = 0;
  const auto fits = [&](size_t n) { return i + n <= bytes.size(); };
  // A coordinate: an i16 of eighths of a unit.
  const auto coord = [&] {
    const auto raw = static_cast<int16_t>(bytes[i] | (bytes[i + 1] << 8));
    i += 2;
    return static_cast<float>(raw) / 8.0f;
  };
  const auto point = [&] {
    const float x = coord(), y = coord();
    return ge::shooter::Vec3f{x, y, coord()};
  };
  const auto stop = [&](size_t from, const std::string& what) {
    v.unread += static_cast<int>(bytes.size() - from);
    if (v.unknown.empty())
      v.unknown = what + " (" + std::to_string(bytes.size() - from) + " bytes left unread)";
    i = bytes.size();
  };
  while (i < bytes.size()) {
    const size_t at = i;
    if (bytes[i] != campaign_svc::temp_entity || !fits(2)) {
      stop(at, "unreliable message " + std::to_string(bytes[at]));
      return;
    }
    const int code = bytes[i + 1];
    i += 2;
    Effect e;
    e.code = code;
    switch (code) {
      case 0:
      case 1:
      case 7:
      case 8:
        e.kind = EffectKind::spike;
        break;
      case 2:
        e.kind = EffectKind::impact;
        break;
      case 3:
      case 4:
      case 12:
        e.kind = EffectKind::explosion;
        break;
      case 10:
        e.kind = EffectKind::lava;
        break;
      case 11:
        e.kind = EffectKind::teleport;
        break;
      case 5:
      case 6:
      case 9:
      case 13:
        e.kind = EffectKind::beam;
        break;
      default:
        stop(at, "temporary entity " + std::to_string(code));
        return;
    }
    const bool beam = e.kind == EffectKind::beam;
    const size_t body = beam ? 2 + 12 : code == 12 ? 6 + 2 : 6;
    if (!fits(body)) {
      stop(at, "temporary entity " + std::to_string(code) + " cut short");
      return;
    }
    if (beam) {
      e.entity = static_cast<int16_t>(bytes[i] | (bytes[i + 1] << 8));
      i += 2;
    }
    e.at = point();
    e.end = beam ? point() : e.at;
    if (code == 12) i += 2;  // the colour range
    out.push_back(e);
  }
}

size_t typed_chars(const CampaignView& v, double time, double printspeed) {
  const double n = std::max(0.0, (time - v.text_at) * printspeed);
  return std::min(v.text.size(), static_cast<size_t>(std::floor(n)));
}

void completed_time(const CampaignView& v, double start_time, int& minutes, int& seconds) {
  const double s = std::max(0.0, v.ended_at - start_time);
  const int whole = static_cast<int>(std::floor(s));
  minutes = whole / 60;
  seconds = whole % 60;
}

}  // namespace shooters_edge::client
