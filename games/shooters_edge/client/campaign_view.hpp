// WHAT THE CLIENT KEEPS OF A CAMPAIGN: the level's end, the statistics and the program's other
// client messages, read off the reliable streams a single-player program writes
// (porting/shooters-edge/campaign.md § The level's end, § The campaign's statistics), for the
// window's screens (menu/level_screens.hpp) to draw. No GPU, no window: the listen client hands
// it the bytes of each frame's all-clients stream and its own slot's stream, in that order.
//
// THE MESSAGES, by their byte: 30 intermission (none), 31 finale (a string), 34 cutscene (a
// string), 33 sellscreen (none), 32 cdtrack (track and loop bytes: recorded, never played: the
// game has no music), 27 killedmonster and 28 foundsecret (none: one more of statistic 14 or 13),
// 10 setangle (three angles, a byte each of 360/256 degrees: the view snaps to them). A byte it
// does not know ends the read of that buffer (what it cannot size it cannot step over): counted in
// `unread` and the first one named in `unknown`.
//
// THE STATISTICS are the four a joining client is sent (11 total secrets, 12 total monsters, 13
// secrets found, 14 monsters killed): `join_stats` sets them, and each find or kill adds one.
//
// THE EFFECTS (`read_effects`): what the program sends every client on the unreliable stream,
// the temporary entities (message 23, then a kind byte): a point for the spikes, the gunshot,
// the explosions, the teleport and the lava splash (kinds 0 1 2 3 4 7 8 10 11); an entity and two
// points for the beams (5 6 9 13); a point and a colour range for the coloured explosion (12).
// Each becomes an `Effect` the window draws; so does a particle burst the program asks for
// (`particle`: blood, or a big burst as an explosion), which the server hands over as an event. A
// message it does not know ends the read as above.
//
// THE TEXT of a finale or a cutscene is typed out a character at a time at `scr_printspeed`
// characters a second from when it arrived (`typed_chars`), and held until the next message.
//
// ge:summary The client's record of a campaign: the level's end, the finale's text, the statistics
// ge:keywords campaign intermission finale cutscene sellscreen cdtrack killedmonster foundsecret
// ge:keywords setangle statistics total-secrets total-monsters typed-text scr_printspeed solo
// ge:keywords temp-entity temporary-entities effects gunshot spike explosion teleport lava beam
// ge:keywords read_effects unreliable
// ge:skill shooter ui
// ge:test tests/shooters_edge_campaign_view_test.cpp
#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <vector>

#include "shooter/vec.hpp"

namespace shooters_edge::client {

// The single-player program's client messages, by their first byte.
namespace campaign_svc {
constexpr uint8_t setangle = 10, killedmonster = 27, foundsecret = 28, intermission = 30;
constexpr uint8_t finale = 31, cdtrack = 32, sellscreen = 33, cutscene = 34;
}  // namespace campaign_svc

namespace campaign_svc {
constexpr uint8_t temp_entity = 23;
}  // namespace campaign_svc

// What a temporary entity shows.
enum class EffectKind : uint8_t { impact, spike, explosion, teleport, lava, beam, blood };
struct Effect {
  EffectKind kind = EffectKind::impact;
  int code = 0;            // the temporary entity's kind byte (-1: a particle burst)
  int entity = 0;          // a beam's owner
  ge::shooter::Vec3f at;   // map units; a beam's start
  ge::shooter::Vec3f end;  // a beam's end
};

inline constexpr double kPrintSpeed = 8;  // scr_printspeed's default, characters a second

enum class LevelEnd : uint8_t { none, intermission, finale, cutscene };

struct CampaignView {
  LevelEnd end = LevelEnd::none;
  double ended_at = 0;      // the server's time the intermission came: the completed time, s
  std::string text;         // the finale's or the cutscene's
  double text_at = 0;       // the server's time it came, s
  bool sellscreen = false;  // asked for: the window opens the help pages and clears it
  int cdtrack = -1, cdloop = -1;
  float total_secrets = 0, total_monsters = 0, secrets = 0, monsters = 0;
  bool turned = false;  // a setangle came: `angles` is the view (the window takes it and clears it)
  ge::shooter::Vec3f angles;  // degrees
  int unread = 0;             // bytes left unread after a message it does not know
  std::string unknown;        // the first such message, for a diagnostic
};

// The statistics a joining client is sent (the program's four globals).
void join_stats(CampaignView& v, float total_secrets, float total_monsters, float secrets,
                float monsters);

// Every message in `bytes` (one stream's worth for a frame), the server's time `time`.
void read_campaign(CampaignView& v, std::span<const uint8_t> bytes, double time);

// Every temporary entity in `bytes` (one frame of the unreliable stream) appended to `out`; a
// message it does not know is counted in `v.unread` and named in `v.unknown`.
void read_effects(CampaignView& v, std::span<const uint8_t> bytes, std::vector<Effect>& out);

// How many characters of the finale's text are shown at the server's time `time`.
size_t typed_chars(const CampaignView& v, double time, double printspeed = kPrintSpeed);

// The completed time as the overlay shows it: whole minutes and seconds.
void completed_time(const CampaignView& v, double start_time, int& minutes, int& seconds);

}  // namespace shooters_edge::client
