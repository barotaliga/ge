// LAB `se-hub-cards`: THE HUB'S MATCH CARDS LOOKED AT (docs/hub.md § Match cards). Each map's
// still drawn from its intermission view in the classic look with classic enhance off
// (hub/still.hpp), then a match's card over it (engine/hub/card.hpp) for three kinds of match: a
// duel (no team column), a 4on4 (the team column, the players grouped by team) and a free-for-all
// (no team column, many rows: the board drawn at twice its pixels, not three times), each board in
// the game's own charset and look: Quake's frags on the players' colours, Quake II's bare with its
// words in green. The stills' key is held too: the same map and build give the same key, and the
// key is written beside each still.
//
// Without `--content` the cards are drawn over each map's hue with a charset of the lab's own
// glyphs (so CI needs no game data and the board's layout is digested); with
// `--content=<dir>` (holding `q1/id1/pak0.pak` and `q2/baseq2/pak0.pak`, as the hub's --content
// does) and `--maps=q1:dm2,q2:q2dm1` the stills are drawn from the packs, for PRIVATE pictures
// only: the packs and anything drawn from them are never committed.
//
// ge:summary Lab: the hub's match cards: each map's intermission still and the live board over it
// ge:keywords lab se-hub-cards hub match-card still intermission classic-look charset conchars
// ge:keywords scoreboard team-column duel 4on4 ffa key cache
// ge:skill net render labs
#include <chrono>
#include <string>
#include <vector>

#include "core/num_text.hpp"
#include "gpu/device.hpp"
#include "hub/card.hpp"
#include "lab/lab.hpp"
#include "lab/labs/liquid_scene.hpp"
#include "lab/raster.hpp"
#include "shooters_edge/hub/cards.hpp"
#include "shooters_edge/hub/still.hpp"

namespace {

namespace gl = ge::lab;
namespace gh = ge::hub;
namespace gn = ge::net;
namespace sh = shooters_edge::hub;
namespace ls = ge::lab::liquid_scene;

// The lab's own charset: its raster's glyphs in 8 px cells, the second set a warm yellow.
gh::CardLook own_look() {
  gh::CardLook l;
  gl::Picture sheet = gl::create_picture(128, 128, {0, 0, 0, 0});
  for (int g = 32; g < 256; g++) {
    const int c = g & 127;
    if (c < 32 || c == 127) continue;
    const std::string s(1, char(c));
    const gl::Rgba ink = g >= 128 ? gl::Rgba{232, 196, 96, 255} : gl::Rgba{226, 226, 226, 255};
    gl::draw_text(sheet, (g % 16) * 8 + 1, (g / 16) * 8, s, ink);
  }
  // The rule's glyphs: a line across the cell.
  for (int g : {0x1d, 0x1e, 0x1f})
    for (int x = (g == 0x1d ? 2 : 0); x < (g == 0x1f ? 6 : 8); x++) {
      uint8_t* p = &sheet.rgba[(size_t((g / 16) * 8 + 4) * 128 + size_t((g % 16) * 8 + x)) * 4];
      p[0] = p[1] = p[2] = 180;
      p[3] = 255;
    }
  l.charset.width = sheet.width;
  l.charset.height = sheet.height;
  l.charset.rgba = std::move(sheet.rgba);
  return l;
}

gn::StatusPlayer player(const char* name, const char* team, int frags, int ping, uint32_t colour,
                        uint8_t flags = 0, int top = gn::kNoColourRow,
                        int bottom = gn::kNoColourRow) {
  gn::StatusPlayer p;
  p.top = uint8_t(top);
  p.bottom = uint8_t(bottom);
  p.name = name;
  p.team = team;
  p.score = frags;
  p.ping_ms = uint16_t(ping);
  p.colour = colour;
  p.flags = flags;
  return p;
}

// The three matches every map is shown with.
std::vector<std::pair<std::string, gn::Status>> matches(const std::string& map,
                                                        const std::string& game) {
  std::vector<std::pair<std::string, gn::Status>> out;
  gn::Status duel;
  duel.map = map;
  duel.mode = "1on1";
  duel.phase = gn::StatusPhase::playing;
  duel.time_left_s = 312;
  // The duellists' own colour rows (drawn in the game's shades where it has them).
  duel.players = {player("Milton", "", 17, 25, 0x4f6fbf, 0, 3, 11),
                  player("wimdowlicker", "", 14, 25, 0xbf3f3f, 0, 0, 4),
                  player("stepcop", "", 0, 31, 0, gn::kPlayerSpectator),
                  player("hub", "", 0, 0, 0, gn::kPlayerSpectator)};
  out.emplace_back("duel", duel);
  gn::Status team;
  team.map = map;
  team.mode = "4on4";
  team.phase = gn::StatusPhase::playing;
  team.time_left_s = 724;
  team.teams = {{"blue", 61, 0x3f5fcf}, {"red", 48, 0xcf3f3f}};
  team.players = {player("Bernkaoch", "blue", 22, 25, 0x3f5fcf),
                  player("peppe", "red", 19, 38, 0xcf3f3f),
                  player("zero", "blue", 18, 12, 0x3f5fcf),
                  player("locktar", "red", 14, 44, 0xcf3f3f),
                  player("milton", "blue", 12, 27, 0x3f5fcf),
                  player("Ihminen", "red", 9, 61, 0xcf3f3f),
                  player("devious", "blue", 9, 19, 0x3f5fcf),
                  player("kip", "red", 6, 33, 0xcf3f3f),
                  player("caster", "", 0, 12, 0, gn::kPlayerSpectator)};
  out.emplace_back("4on4", team);
  gn::Status ffa;
  ffa.map = map;
  ffa.mode = "ffa";
  ffa.phase = gn::StatusPhase::intermission;
  const char* names[] = {"jmh",    "Thresh", "avenger", "spice",  "wart",    "mrlame",
                         "camper", "blixem", "crit",    "mirage", "povohat", "dirtbox"};
  int frags = 31;
  for (const char* n : names) {
    ffa.players.push_back(
        player(n, "", frags, 13 + frags % 7 * 9, 0x7f7f3f + uint32_t(frags) * 0x0503));
    frags -= 3;
  }
  ffa.players.back().flags = gn::kPlayerBot;
  out.emplace_back("ffa", ffa);
  (void)game;
  return out;
}

gl::Picture picture(const gh::CardPicture& c) { return gl::Picture{c.width, c.height, c.rgba}; }

void run(gl::Lab& lab) {
  const auto size = lab.arg_nums("size");
  const int w = size.size() > 0 ? int(size[0]) : 960;
  const int h = size.size() > 1 ? int(size[1]) : 540;
  const std::string content = lab.arg("content");
  gh::HubSpec spec;
  spec.score = "frags";

  // The maps: "<game>:<map>" each.
  std::vector<std::pair<std::string, std::string>> maps;
  for (size_t at = 0, asked = 0; at <= lab.arg("maps").size(); at = asked + 1) {
    const std::string& all = lab.arg("maps");
    asked = std::min(all.find(',', at), all.size());
    const std::string one = all.substr(at, asked - at);
    const size_t colon = one.find(':');
    if (colon != std::string::npos) maps.emplace_back(one.substr(0, colon), one.substr(colon + 1));
  }
  if (content.empty()) {
    // Our own: the cards over each map's hue, in the lab's charset; the board's layout digested.
    const gh::CardLook look = own_look();
    for (const auto& [game, map] : maps) {
      for (const auto& [kind, s] : matches(map, game)) {
        const gh::CardPicture card = gh::compose_card(spec, s, look, {}, w, h);
        lab.shot({map + "-" + kind,
                  map + ": a " + kind + "'s card over the map's hue",
                  "The card a match on " + map +
                      " gets with no still (no content, or a map the "
                      "content lacks): the map's hue, the board in the lab's own charset.",
                  "the board centred, the team column only in the 4on4, the spectators at the foot",
                  {{"map", map}, {"match", kind}, {"game", "none"}}},
                 picture(card));
      }
    }
    return;
  }

  auto device = ge::gpu::create_device({.label = "se-hub-cards"});
  if (!device && ls::gpu_required())
    lab.fail("no GPU device, and GE_GPU_REQUIRE=1",
             "Install Mesa's Vulkan drivers (apt install mesa-vulkan-drivers). Skill: gpu.");
  device.reset();  // the still makes its own, as the hub does
  for (const auto& [game, map] : maps) {
    gh::CardLook look;
    std::string error;
    if (!sh::hub_card_look(content, game, look, error)) {
      lab.fail(game + ": " + error, "Pass --content=<dir> holding " + game +
                                        "'s install (q1/id1, q2/baseq2). Skill: net.");
      continue;
    }
    const std::string key = sh::still_key(content, game, map, w, h);
    lab.metric(map + ".key-same", key == sh::still_key(content, game, map, w, h) ? 1 : 0, "bool");
    if (key.empty()) {
      lab.fail(map + ": no such map in " + game + "'s packs", "Check --maps. Skill: net.");
      continue;
    }
    gh::CardPicture still;
    still.width = w;
    still.height = h;
    const auto t0 = std::chrono::steady_clock::now();
    if (!sh::draw_still(content, game, map, w, h, still.rgba, error)) {
      lab.fail(map + ": " + error, "Read the gpu: lines above. Skill: render.");
      continue;
    }
    const double ms =
        std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
    lab.metric(map + ".still-ms", ms, "ms", gl::Det::measured);
    lab.shot({map + "-still",
              map + "'s still from its intermission view (" + game + ")",
              "The map drawn once from where its intermission looks, in the classic look with "
              "classic enhance off (the packs' own texels and lightmaps). Kept by the hub as " +
                  map + "-" + key + ".jpg; drawn in " + ge::core::num::to_fixed(ms, 0) + " ms.",
              "the map's own view, every wall textured, the light as the map was lit",
              {{"map", map}, {"game", game}, {"key", key}}},
             picture(still), gl::Det::measured);
    for (const auto& [kind, s] : matches(map, game)) {
      const gh::CardPicture card = gh::compose_card(spec, s, look, still, w, h);
      lab.shot({map + "-" + kind,
                map + ": a " + kind + "'s card",
                "The hub's card of a " + kind + " on " + map +
                    ": the still darkened, the board "
                    "in the game's own glyph sheet at whole multiples of its pixels.",
                "ping, frags, the team only in the 4on4, the name; the game's own letters",
                {{"map", map}, {"match", kind}, {"game", game}}},
               picture(card), gl::Det::measured);
    }
  }
}

const gl::LabSpec kSpec{
    "se-hub-cards",
    gl::LabKind::picture,
    "game:shooters_edge",
    "The hub's match cards (each map's intermission still, the live board in the game's charset)",
    "net",
    "Does each map's still show its intermission view in the classic look, and does each match's "
    "board read in the game's own letters, with the team column only in a team match?",
    {{"size", "960,540", "a card's width and height, px"},
     {"content", "",
      "the folder holding each game's install (q1/id1, q2/baseq2), as the hub's --content "
      "(private pictures only); none: the cards over the maps' hues in the lab's charset"},
     {"maps", "q1:dm2,q2:q2dm1", "the maps, <game>:<map>, comma-separated"}},
    "--size=480,270",
    run,
};
GE_LAB(kSpec);

}  // namespace
