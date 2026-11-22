// Tests: Quake II's layout language (games/shooters_edge/hud/q2_layout.cpp): the cursor's
// six words, the big numbers (field, inset, cut, flashing, the hit's background), pictures by stat
// and by name, the text words and centring, `if` without nesting, the score blocks, and the words
// and values that draw nothing.
//
// ge:summary Tests: Quake II's layout language: cursor, numbers, pictures, text, conditions
// ge:keywords test q2 layout status-bar hnum anum rnum cstring client-block
// ge:skill ui testing
// ge:test tests/shooters_edge_q2_layout_test.cpp
#include <array>
#include <string>
#include <vector>

#include "check.hpp"
#include "shooters_edge/hud/q2_layout.hpp"

namespace {

namespace hud = shooters_edge::hud;
using Op = hud::LayoutOp;

struct Fixture {
  std::array<int16_t, 32> stats{};
  std::vector<std::string> strings = std::vector<std::string>(hud::kLayoutStrings);
  int width = 640, height = 480;
  int64_t frame = 0;
  int self = -1;

  std::vector<Op> run(std::string_view program) const {
    hud::LayoutInput in;
    in.program = program;
    in.stats = stats;
    in.width = width;
    in.height = height;
    in.frame = frame;
    in.self = self;
    in.ctx = this;
    in.string = [](const void* ctx, int i) -> std::string_view {
      return static_cast<const Fixture*>(ctx)->strings[static_cast<size_t>(i)];
    };
    std::vector<Op> out;
    hud::run_layout(in, out);
    return out;
  }
};

std::string names(const std::vector<Op>& ops) {
  std::string s;
  for (const Op& o : ops) {
    s += o.kind == Op::Kind::picture ? o.name : "'" + o.text + "'";
    s += "@" + std::to_string(o.x) + "," + std::to_string(o.y) + " ";
  }
  return s;
}

}  // namespace

TEST(the_cursor_words_place_from_each_edge_and_the_centred_field) {
  Fixture f;
  const auto ops = f.run("xl 4 yt 6 picn a xr -10 yb -20 picn b xv 0 yv 0 picn c");
  CHECK_MSG(names(ops) == "a@4,6 b@630,460 c@160,120 ", names(ops));
}

TEST(a_number_is_right_aligned_in_its_field_with_an_inset_and_cut_when_too_wide) {
  Fixture f;
  f.stats[7] = 42;
  CHECK_MSG(names(f.run("num 3 7")) == "num_4@18,0 num_2@34,0 ", names(f.run("num 3 7")));
  f.stats[7] = -150;
  CHECK_MSG(names(f.run("num 3 7")) == "num_minus@2,0 num_1@18,0 num_5@34,0 ",
            names(f.run("num 3 7")));
  f.stats[7] = 1234;
  CHECK(names(f.run("num 3 7")) == "num_1@2,0 num_2@18,0 num_3@34,0 ");
  CHECK(f.run("num 0 7").empty());
  f.stats[7] = 1;
  CHECK(names(f.run("num 9 7")) == "num_1@66,0 ");  // a width over 5 is 5
}

TEST(health_flashes_low_and_shows_the_hit_and_ammo_and_armour_hide) {
  Fixture f;
  f.stats[1] = 100;
  CHECK(names(f.run("hnum")) == "num_1@2,0 num_0@18,0 num_0@34,0 ");
  f.stats[1] = 20;
  f.frame = 0;
  CHECK(f.run("hnum")[0].name == "num_2");
  f.frame = 4;
  CHECK(f.run("hnum")[0].name == "anum_2");
  f.stats[1] = -5;
  f.frame = 0;
  CHECK(f.run("hnum")[0].name == "anum_minus");
  f.stats[15] = 1 | 2;
  f.stats[1] = 50;
  f.stats[5] = 30;
  CHECK(f.run("hnum")[0].name == "field_3");
  CHECK(f.run("rnum")[0].name == "field_3");
  f.stats[5] = 0;
  CHECK(f.run("rnum").empty());
  f.stats[3] = -1;
  CHECK(f.run("anum").empty());
  f.stats[3] = 3;
  f.frame = 4;
  CHECK(f.run("anum")[0].name == "anum_3");
}

TEST(a_picture_by_stat_reads_the_image_strings_and_zero_draws_nothing) {
  Fixture f;
  f.strings[hud::kLayoutImages + 2] = "i_health";
  f.stats[0] = 2;
  CHECK(names(f.run("xv 50 yb -24 pic 0")) == "i_health@210,456 ");
  f.stats[0] = 0;
  CHECK(f.run("pic 0").empty());
  f.stats[0] = 300;
  CHECK(f.run("pic 0").empty());
  CHECK(f.run("pic 40").empty());  // past the 32 stats
  CHECK(hud::layout_picture_path("i_health") == "pics/i_health.pcx");
  CHECK(hud::layout_picture_path("/players/male/grunt_i.pcx") == "players/male/grunt_i.pcx");
}

TEST(if_skips_to_the_first_endif_without_nesting) {
  Fixture f;
  f.stats[2] = 0;
  f.stats[3] = 1;
  CHECK(names(f.run("if 2 picn a if 3 picn b endif picn c endif picn d")) == "c@0,0 d@0,0 ");
  CHECK(names(f.run("xl 5 if 2 xl 9 endif picn e")) == "e@5,0 ");  // the cursor kept
  CHECK(f.run("if 2 picn a").empty());
  CHECK(names(f.run("if 3 picn a endif")) == "a@0,0 ");
}

TEST(text_words_draw_lines_and_centre_them_in_320) {
  Fixture f;
  auto ops = f.run("xv 0 yv 0 string \"Hello there\" string2 alt");
  CHECK(ops.size() == 2 && ops[0].text == "Hello there" && !ops[0].alt && ops[1].alt);
  ops = f.run("xv 0 yv 24 cstring2 \"abcd\nxy\"");
  CHECK(ops.size() == 2);
  CHECK(ops[0].x == 160 + (320 - 32) / 2 && ops[0].y == 144 && ops[0].alt);
  CHECK(ops[1].x == 160 + (320 - 16) / 2 && ops[1].y == 152);
  f.strings[1056 + 4] = "Body Armor";
  f.stats[8] = 1056 + 4;
  ops = f.run("xv 26 yb -42 stat_string 8");
  CHECK(ops.size() == 1 && ops[0].text == "Body Armor" && ops[0].x == 186 && ops[0].y == 438);
  CHECK(f.run("// only a comment\n").empty());
}

TEST(a_client_block_and_a_ctf_row) {
  Fixture f;
  f.strings[hud::kLayoutSkins + 3] = "Ana\\female/athena";
  auto ops = f.run("client 0 32 3 12 50 7");
  CHECK_MSG(names(ops) ==
                "'Ana'@192,152 'Score: '@192,160 '12'@248,160 'Ping:  50'@192,168 "
                "'Time:  7'@192,176 /players/female/athena_i.pcx@160,152 ",
            names(ops));
  CHECK(ops.back().fallback == "/players/male/grunt_i.pcx");
  f.self = 3;
  ops = f.run("ctf 0 0 3 5 1500");
  CHECK(ops.size() == 1 && ops[0].text == "  5 999 Ana         " && ops[0].alt);
  CHECK(f.run("client 0 0 300 1 1 1").empty());
}

TEST(unknown_words_are_skipped_and_long_words_read_empty) {
  Fixture f;
  CHECK(names(f.run("face 3 picn a endif")) == "a@0,0 ");
  CHECK(f.run("picn " + std::string(130, 'x')).empty());
}

// `rstring` ends its line at the cursor; `rstring2` flips each letter's top bit (plain text into
// the coloured set); the stat_ forms draw the string a stat names in each of the six forms.
TEST(right_aligned_text_and_every_stat_form) {
  Fixture f;
  f.stats[7] = 40;
  f.strings[40] = "ab";
  const auto ops = f.run(
      "xl 100 yt 10 rstring \"hello\" rstring2 hi stat_rstring 7 "
      "stat_cstring2 7 stat_string2 7");
  CHECK_MSG(ops.size() == 5, names(ops));
  CHECK(ops[0].x == 60 && ops[0].text == "hello" && !ops[0].alt);
  CHECK(ops[1].x == 84 && ops[1].text == std::string("\xe8\xe9"));
  CHECK(ops[2].x == 84 && ops[2].text == "ab");
  CHECK(ops[3].x == 100 + (320 - 16) / 2 && ops[3].alt);
  CHECK(ops[4].x == 100 && ops[4].alt && ops[4].text == "ab");
}

// `color` tints everything after it in that program, in each of its forms; a colour it does not
// name leaves the last one; each program starts white.
TEST(color_tints_what_follows_until_the_next_one) {
  Fixture f;
  const auto ops = f.run(
      "string a color red string b color #f80 picn c color #10203040 string d "
      "color alt string e color 4 string f color #12 string g");
  CHECK_MSG(ops.size() == 7, names(ops));
  const auto is = [](const Op& o, int r, int g, int b, int a) {
    return o.rgba[0] == r && o.rgba[1] == g && o.rgba[2] == b && o.rgba[3] == a;
  };
  CHECK(is(ops[0], 255, 255, 255, 255));
  CHECK(is(ops[1], 255, 0, 0, 255));
  CHECK(is(ops[2], 255, 136, 0, 255));
  CHECK(is(ops[3], 16, 32, 48, 64));
  CHECK(is(ops[4], 16, 32, 48, 64));  // `alt` is refused
  CHECK(is(ops[5], 0, 0, 255, 255));  // 4: blue
  CHECK(is(ops[6], 0, 0, 255, 255));  // two digits refused
  CHECK(is(f.run("string z")[0], 255, 255, 255, 255));
  uint8_t c[4] = {1, 2, 3, 4};
  CHECK(hud::layout_colour("YELLOW", c) == false && c[0] == 1);
  CHECK(hud::layout_colour("#ABCDEF", c) && c[0] == 0xab && c[2] == 0xef && c[3] == 255);
}

TEST(health_bars_draws_a_boss_s_name_and_its_two_bars_from_a_stat_s_bytes) {
  Fixture f;  // 640 x 480: the bars a third of it, 213 px, centred on the cursor's x + 160
  f.strings[700] = "Makron";
  f.stats[20] = static_cast<int16_t>((0 << 8) | 128);  // the first bar half full, the second hidden
  const auto ops = f.run("xv 0 yv 0 health_bars 20 700");
  CHECK(ops.size() == 3);
  CHECK(ops[0].kind == Op::Kind::text && ops[0].text == "Makron" && ops[0].x == 160 + 136);
  const int left = 160 + 160 - 213 / 2;
  CHECK(ops[1].kind == Op::Kind::fill && ops[1].x == left && ops[1].y == 120 + 12);
  CHECK(ops[1].w == 107 && ops[1].h == 4 && ops[1].rgba[0] == 239 && ops[1].rgba[1] == 0);
  CHECK(ops[2].x == left + 107 && ops[2].w == 213 - 107 && ops[2].rgba[0] == 63);
  f.stats[20] = static_cast<int16_t>((255 << 8) | 1);  // the first empty, the second full
  const auto both = f.run("xv 0 yv 0 health_bars 20 700");
  CHECK(both.size() == 3);  // an empty bar's red and a full bar's grey are not drawn
  CHECK(both[1].w == 213 && both[1].rgba[0] == 63 && both[1].y == 132);
  CHECK(both[2].w == 213 && both[2].rgba[0] == 239 && both[2].y == 140);
}

TEST(a_scope_picture_at_the_centred_box_fills_the_screen_s_middle_scaled_by_ch_scale) {
  Fixture f;
  f.strings[hud::kLayoutImages + 3] = "scope4x";
  f.stats[16] = 3;
  auto ops = f.run("xv 0 yv 0 pic 16");
  CHECK(ops.size() == 1 && ops[0].w == 320 && ops[0].h == 240 && ops[0].x == 160 &&
        ops[0].y == 120);
  ops = f.run("xv 8 yv 0 pic 16");  // off the box: drawn as it is
  CHECK(ops.size() == 1 && ops[0].w == 0 && ops[0].x == 168);
  f.strings[hud::kLayoutImages + 3] = "scope10x";  // not one letter between: as it is
  ops = f.run("xv 0 yv 0 pic 16");
  CHECK(ops.size() == 1 && ops[0].w == 0);
}
