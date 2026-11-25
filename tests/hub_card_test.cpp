// engine/hub/card.hpp and stills.hpp: a match's card as a picture and the maps' stills. The team
// column shows only in a team match; the board is drawn in the charset's glyphs over the still (or
// the map's hue), scaled by a whole number; a map's name from the wire is a file name only when
// plain; a still is drawn once, kept on disk by its key, read back without drawing, and drawn
// again (the old file gone) when the key changes; the routes answer the card and the still.
//
// ge:summary Test: the hub's match cards (team column, board pixels, names) and the stills' cache
// ge:keywords test hub card match-card still stills cache key invalidate charset team-column
// ge:skill net testing
#include "hub/card.hpp"

#include <filesystem>
#include <string>

#include "capture/jpeg.hpp"
#include "check.hpp"
#include "hub/pages.hpp"
#include "hub/stills.hpp"

using namespace ge::net;
using namespace ge::hub;

namespace fs = std::filesystem;

namespace {

HubSpec spec() {
  HubSpec s;
  s.game = "Test Game";
  s.id = "test_game";
  s.score = "frags";
  return s;
}

// A charset whose every printable glyph is a solid white square (8 px cells).
CardLook solid_look() {
  CardLook l;
  l.charset.width = l.charset.height = 128;
  l.charset.rgba.assign(size_t{128} * 128 * 4, 0);
  for (int g = 33; g < 256; g++) {
    if (g >= 127 && g < 161) continue;
    for (int y = 0; y < 8; y++)
      for (int x = 0; x < 8; x++) {
        uint8_t* p =
            &l.charset.rgba[(size_t((g / 16) * 8 + y) * 128 + size_t((g % 16) * 8 + x)) * 4];
        p[0] = p[1] = p[2] = p[3] = 255;
      }
  }
  return l;
}

Status match(const std::string& mode, int players, bool teams) {
  Status s;
  s.mode = mode;
  s.map = "dm2";
  s.phase = StatusPhase::playing;
  s.time_left_s = 300;
  for (int i = 0; i < players; i++) {
    StatusPlayer p;
    p.name = "player" + std::to_string(i);
    p.team = teams ? (i % 2 ? "red" : "blue") : "";
    p.score = 10 - i;
    p.ping_ms = uint16_t(20 + i);
    s.players.push_back(p);
  }
  StatusPlayer spec_p;
  spec_p.name = "watcher";
  spec_p.flags = kPlayerSpectator;
  s.players.push_back(spec_p);
  return s;
}

int bright(const CardPicture& c) {
  int n = 0;
  for (size_t i = 0; i < c.rgba.size(); i += 4)
    if (c.rgba[i] == 255 && c.rgba[i + 1] == 255 && c.rgba[i + 2] == 255) n++;
  return n;
}

}  // namespace

TEST(team_column_only_in_team_matches) {
  CHECK(!card_teams(match("1on1", 2, false)));
  CHECK(!card_teams(match("ffa", 6, false)));
  CHECK(card_teams(match("4on4", 8, true)));
  CHECK(!card_teams(match("1on1", 2, true)));  // a duel with typed teams shows none
  Status two = match("ctf", 2, false);
  two.teams = {{"red", 3, 0xff0000}, {"blue", 1, 0x0000ff}};
  CHECK(card_teams(two));
}

TEST(card_draws_the_board_over_the_still_or_the_hue) {
  const CardLook look = solid_look();
  CardPicture still;
  still.width = 32;
  still.height = 18;
  still.rgba.assign(size_t{32} * 18 * 4, 200);
  const CardPicture a = compose_card(spec(), match("1on1", 2, false), look, still, 960, 270);
  CHECK(a.width == 960 && a.height == 270 && a.rgba.size() == size_t{960} * 270 * 4);
  // The still darkened to three fifths where nothing is drawn (the bottom-left corner).
  const uint8_t* corner = &a.rgba[(size_t(269) * 960 + 1) * 4];
  CHECK(corner[0] == 120 && corner[1] == 120 && corner[2] == 120);
  // The letters white, in whole squares of the scale.
  const int lit = bright(a);
  CHECK(lit > 0 && lit % 9 == 0);  // two rows fit at three times
  // More players, more letters (six rows fit only at twice the pixels); the team column in a team
  // match adds more.
  const CardPicture b = compose_card(spec(), match("ffa", 6, false), look, still, 960, 270);
  CHECK(bright(b) % 4 == 0 && bright(b) / 4 > lit / 9);
  const CardPicture t = compose_card(spec(), match("4on4", 6, true), look, still, 960, 270);
  CHECK(bright(t) > bright(b));
  // No still: the map's hue, dark.
  const CardPicture h = compose_card(spec(), match("1on1", 2, false), look, {}, 480, 270);
  const uint8_t* g = &h.rgba[(size_t(269) * 480 + 1) * 4];
  CHECK(g[0] < 60 && g[1] < 60 && g[2] < 60);
  // No charset: the ground alone.
  CHECK(bright(compose_card(spec(), match("1on1", 2, false), CardLook{}, still, 960, 270)) == 0);
  // The JPEG reads back at the size.
  std::string error;
  const std::vector<uint8_t> jpg = card_jpeg(a);
  const auto back = ge::capture::decode_jpeg(jpg, error);
  CHECK(back && back->width == 960 && back->height == 270);
}

TEST(frag_boxes_take_the_players_colour_rows_in_the_games_shades) {
  CardLook look = solid_look();
  Status s = match("1on1", 2, false);
  s.players[0].top = 1;
  s.players[0].bottom = 2;
  auto green = [](const CardPicture& c) {
    int n = 0;
    for (size_t i = 0; i < c.rgba.size(); i += 4)
      if (c.rgba[i] == 0 && c.rgba[i + 1] == 255 && c.rgba[i + 2] == 0) n++;
    return n;
  };
  CHECK(green(compose_card(spec(), s, look, {}, 960, 270)) == 0);  // no shades: the brown box
  look.rows.assign(14, 0x00ff00);
  CHECK(green(compose_card(spec(), s, look, {}, 960, 270)) > 0);
  s.players[0].top = s.players[0].bottom = kNoColourRow;  // none picked: the brown box again
  CHECK(green(compose_card(spec(), s, look, {}, 960, 270)) == 0);
}

TEST(names_from_the_wire_are_plain_file_names) {
  CHECK(still_name_ok("dm2") && still_name_ok("q2dm1") && still_name_ok("ztn_dm3+x.v2"));
  CHECK(!still_name_ok("") && !still_name_ok("../etc") && !still_name_ok("a/b"));
  CHECK(!still_name_ok(".hidden") && !still_name_ok("a\\b") &&
        !still_name_ok(std::string(65, 'a')));
}

TEST(stills_are_kept_by_key_and_drawn_again_when_it_changes) {
  const fs::path dir = fs::temp_directory_path() / "hub_card_test_stills";
  fs::remove_all(dir);
  int drawn = 0;
  std::string key = "k0001";
  HubStills game;
  game.game_of = [](const Status&) { return std::string("q1"); };
  game.key = [&](std::string_view, std::string_view, std::string_view map, int, int) {
    return map == "dm2" ? key : std::string();
  };
  game.render = [&](std::string_view, std::string_view, std::string_view, int w, int h,
                    CardPicture& out, std::string&) {
    drawn++;
    out.width = w;
    out.height = h;
    out.rgba.assign(size_t(w) * size_t(h) * 4, 90);
    return true;
  };
  game.look = [](std::string_view, std::string_view, CardLook& out) {
    out = solid_look();
    return true;
  };
  StillsConfig c;
  c.cache = dir.string();
  c.content = "unused";
  c.width = 64;
  c.height = 36;
  c.thread = false;
  {
    Stills s(game, c);
    CHECK(s.still("q1", "dm2") == nullptr);  // asked
    CHECK(s.still("q1", "e1m9") == nullptr);
    CHECK(s.still("q1", "../dm2") == nullptr);
    CHECK(s.make_waiting() == 2 && drawn == 1);
    CHECK(s.made() == 1 && s.refused() == 1);
    const auto pic = s.still("q1", "dm2");
    CHECK(pic && pic->width == 64 && pic->height == 36);
    CHECK(s.still("q1", "e1m9") == nullptr && s.make_waiting() == 0);  // refused once, not again
    CHECK(fs::exists(dir / "q1" / "dm2-k0001.jpg"));
    // The routes: the still's file, and the card of a node on that map.
    Poller p;
    HubNode n;
    n.address = sim_address(10, 27500);
    n.have = n.listed = true;
    n.status = match("1on1", 2, false);
    p.nodes.push_back(n);
    HubSpec hs = spec();
    hs.stills = game;
    Answer a;
    CHECK(stills_answer(p, hs, &s, "/still/q1/dm2.jpg", a) && a.type == "image/jpeg");
    CHECK(!stills_answer(p, hs, &s, "/still/q1/e1m9.jpg", a));
    CHECK(stills_answer(p, hs, &s, "/card/" + to_string(n.address) + ".jpg", a));
    CHECK(a.type == "image/jpeg" && a.body.size() > 100);
    CHECK(!stills_answer(p, hs, nullptr, "/still/q1/dm2.jpg", a));
    // The front page shows the card's picture in place of the HTML board.
    const std::string page = servers_page(p, hs, "", 0, &s);
    CHECK(page.find("<img class=\"shot\" src=\"/card/") != std::string::npos);
    CHECK(page.find("table class=\"score\"") == std::string::npos);
    CHECK(servers_page(p, hs, "", 0).find("table class=\"score\"") != std::string::npos);
  }
  {
    // A new hub reads the kept still without drawing it.
    Stills s(game, c);
    (void)s.still("q1", "dm2");
    CHECK(s.make_waiting() == 1 && drawn == 1 && s.still("q1", "dm2") != nullptr);
  }
  {
    // A new key (another map file, a new renderer): drawn again, the old file gone.
    key = "k0002";
    Stills s(game, c);
    (void)s.still("q1", "dm2");
    CHECK(s.make_waiting() == 1 && drawn == 2);
    CHECK(fs::exists(dir / "q1" / "dm2-k0002.jpg") && !fs::exists(dir / "q1" / "dm2-k0001.jpg"));
  }
  fs::remove_all(dir);
}
