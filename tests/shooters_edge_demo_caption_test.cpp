// The demo video's upper-left words and video.cfg: the lines a single-player, a team and a
// free-for-all recording get, the clock and the size, the packs' glyph sheet read with index 0
// see-through, the words drawn in it and in the charset, and video.cfg's order (the newer look's
// defaults over the player's config, video.cfg over those, the command line over all).
//
// ge:summary Tests: the demo video's caption lines and glyph sheet, and video.cfg's order
// ge:keywords test demo-video caption conchars glyph-sheet team-score video-cfg reset
// ge:skill shooter testing
#include <filesystem>
#include <fstream>
#include <map>
#include <string>
#include <vector>

#include "check.hpp"
#include "classicenhance/switches.hpp"
#include "console/console.hpp"
#include "shooters_edge/client/demo_caption.hpp"
#include "shooters_edge/client/video_cfg.hpp"
#include "shooters_edge/menu/classic_art.hpp"

namespace {

namespace cl = shooters_edge::client;

std::vector<std::string> texts(const std::vector<cl::CaptionLine>& lines) {
  std::vector<std::string> out;
  for (const cl::CaptionLine& l : lines) out.push_back(l.text);
  return out;
}

TEST(the_clock_and_the_size) {
  CHECK(cl::caption_clock(0) == "0:00");
  CHECK(cl::caption_clock(75.9) == "1:15");
  CHECK(cl::caption_clock(3725) == "1:02:05");
  CHECK(cl::caption_clock(-3) == "0:00");
  CHECK(cl::caption_size(360) == 8);
  CHECK(cl::caption_size(720) == 16);
  CHECK(cl::caption_size(1080) == 24);
  CHECK(cl::caption_size(100) == 8);
}

TEST(a_team_game_shows_the_map_the_clock_whose_eyes_and_the_teams) {
  cl::DemoState s;
  s.format = cl::DemoFormat::multiview;
  s.map = "hall";
  s.player = 2;
  const char* names[] = {"red1", "red2", "blue1", "blue2"};
  const char* teams[] = {"red", "red", "blue", "blue"};
  const int frags[] = {3, 4, 9, 1};
  for (int k = 0; k < 4; k++) {
    s.names[static_cast<size_t>(k)] = names[k];
    s.teams[static_cast<size_t>(k)] = teams[k];
    s.frags[static_cast<size_t>(k)] = frags[k];
  }
  const std::vector<cl::CaptionLine> lines = cl::demo_caption(s, "The\nGreat  Hall\n", 61);
  CHECK((texts(lines) == std::vector<std::string>{"hall  The Great Hall", "1:01",
                                                  "watching blue1, 9 frags", "blue 10  red 7"}));
  CHECK(lines[0].gold);
  CHECK(!lines[1].gold);
}

TEST(a_free_for_all_names_the_leader_unless_it_is_the_watched_player) {
  cl::DemoState s;
  s.format = cl::DemoFormat::client;
  s.map = "hall";
  s.names[0] = "me";
  s.frags[0] = 1;
  s.names[1] = "ace";
  s.frags[1] = 12;
  std::vector<std::string> got = texts(cl::demo_caption(s, "hall", 5));
  CHECK((got ==
         std::vector<std::string>{"hall", "0:05", "watching me, 1 frag", "leader ace, 12 frags"}));
  s.frags[0] = 20;
  got = texts(cl::demo_caption(s, "", 5));
  CHECK(got.size() == 3);
}

TEST(a_single_player_recording_shows_kills_and_secrets_when_the_level_has_them) {
  cl::DemoState s;
  s.map = "start";
  CHECK((texts(cl::demo_caption(s, "", 0)) == std::vector<std::string>{"start", "0:00"}));
  s.stats[cl::kStatTotalMonsters] = 40;
  s.stats[cl::kStatMonsters] = 12;
  s.stats[cl::kStatTotalSecrets] = 5;
  s.stats[cl::kStatSecrets] = 1;
  CHECK((texts(cl::demo_caption(s, "", 0)) ==
         std::vector<std::string>{"start", "0:00", "kills 12/40  secrets 1/5"}));
}

TEST(the_packs_glyph_sheet_is_read_with_index_0_see_through_and_drawn) {
  const auto files = shooters_edge::menu::fixture_files();
  const cl::CaptionFind find = [&files](std::string_view p, std::vector<uint8_t>& store) {
    const auto it = files.find(p);
    if (it == files.end()) return std::span<const uint8_t>{};
    store = it->second;
    return std::span<const uint8_t>(store);
  };
  ge::ui::Image sheet;
  std::string why;
  CHECK(cl::caption_glyphs_from_packs(find, &sheet, &why));
  CHECK(sheet.width == 128 && sheet.height == 128);
  bool clear = false, solid = false;
  for (size_t i = 3; i < sheet.rgba.size(); i += 4) (sheet.rgba[i] == 0 ? clear : solid) = true;
  CHECK(clear && solid);

  const cl::CaptionFind none = [](std::string_view, std::vector<uint8_t>&) {
    return std::span<const uint8_t>{};
  };
  ge::ui::Image missing;
  CHECK(!cl::caption_glyphs_from_packs(none, &missing, &why));
  CHECK(why.find("palette") != std::string::npos);

  const std::vector<cl::CaptionLine> lines = {{"hall", true}, {"0:12", false}};
  ge::ui::DrawList in_sheet, in_charset;
  in_sheet.begin(1280, 720);
  cl::draw_caption(in_sheet, sheet, 1280, 720, lines);
  in_charset.begin(1280, 720);
  cl::draw_caption(in_charset, ge::ui::Image{}, 1280, 720, lines);
  CHECK(in_sheet.size() > 0);
  CHECK(in_charset.size() > 0);
  CHECK(!cl::caption_glyphs_from_png("no/such/conchars.png", &missing));
}

TEST(the_prints_become_notify_lines_and_the_chat_lasts_longer) {
  cl::DemoState s;
  s.printed = {
      {0, "You got the "}, {0, "shells\n"}, {3, "(red1): \x90quad\x91 low\n"}, {1, "  \n"}};
  cl::DemoNotices n;
  n.take(s, 10);
  std::vector<cl::CaptionLine> got = n.at(10);
  CHECK(got.size() == 2);
  CHECK(got[0].text == "You got the shells" && !got[0].gold);
  CHECK(got[1].gold);
  CHECK(cl::plain_text(got[1].text) == "(red1): [quad] low");
  CHECK(n.at(10 + cl::kNoticeSeconds + 0.1).size() == 1);  // the pickup gone, the chat kept
  CHECK(n.at(10 + cl::kChatSeconds + 0.1).empty());

  s.printed.clear();
  for (int k = 0; k < 6; k++) s.printed.push_back({2, "line " + std::to_string(k) + "\n"});
  s.centred = true;
  s.centre = "The Slipgate Complex\n";
  n.take(s, 20);
  got = n.at(20);
  CHECK(got.size() == cl::kNoticeLines);
  CHECK(got.back().text == "line 5");
  CHECK(n.centre_until >= 20 + cl::kCentreSeconds);

  ge::ui::DrawList dl;
  dl.begin(640, 360);
  cl::draw_centre(dl, ge::ui::Image{}, 640, 360, n.centre);
  CHECK(dl.size() > 0);

  // A line printed in pieces is one line once its end comes.
  cl::PrintJoin join;
  CHECK(join.feed({0, "You got the "}).empty());
  CHECK(join.feed({0, "Nailgun"}).empty());
  const std::vector<cl::DemoPrint> done = join.feed({0, "\n\nnext"});
  CHECK(done.size() == 1 && done[0].text == "You got the Nailgun");
  CHECK(join.pending == "next");
}

TEST(video_cfg_puts_back_the_newer_look_then_reads_the_players_file_then_the_command_line) {
  ge::console::Console con;
  std::string look = "modern";
  con.cvars().bind({.name = "r_look", .fallback = "modern", .kind = ge::console::CvarKind::text},
                   &look);
  ge::classicenhance::Switches ce;
  ge::classicenhance::bind_switches(con.cvars(), &ce);
  cl::VideoSettings video;
  cl::add_video_cvars(con, &video);
  CHECK(video.size == "1080p" && video.fps == 60 && video.text && video.hud);

  // The player's config for play: the classic look, the decals and the flames off.
  con.execute("r_look classic\nce_decals 0\nce_flames 0\n");
  CHECK(look == "classic" && !ce.decals && !ce.flames);

  const std::string defaults = cl::video_defaults(con.cvars());
  CHECK(defaults.find("reset r_look\n") != std::string::npos);
  CHECK(defaults.find("reset ce_decals\n") != std::string::npos);
  CHECK(defaults.find("reset r_bloom") == std::string::npos);  // not registered here

  const std::filesystem::path dir = std::filesystem::temp_directory_path() / "ge_video_cfg_test";
  std::filesystem::create_directories(dir);
  {
    std::ofstream f(dir / "video.cfg");
    f << "demovideo_fps 30\ndemovideo_size 720p\nce_flames 0\n";
  }
  const std::filesystem::path read =
      cl::apply_video_cfg(con, {std::filesystem::path(), dir}, {"+demovideo_text", "0"});
  CHECK(read == dir / "video.cfg");
  CHECK(look == "modern");  // the built-in defaults
  CHECK(ce.decals);         // back to the catalog's default
  CHECK(!ce.flames);        // video.cfg's own choice
  CHECK(video.fps == 30 && video.size == "720p");
  CHECK(!video.text);  // the command line
  std::filesystem::remove_all(dir);

  // With no video.cfg anywhere the defaults alone.
  con.execute("r_look classic\n");
  CHECK(cl::apply_video_cfg(con, {dir}, {}).empty());
  CHECK(look == "modern");
}

}  // namespace
