// Tests: the sequel's layout language (games/shooters_edge/hud/sequel_layout.cpp): the cursor's
// six words, the big numbers (field, inset, cut, flashing, the hit's background), pictures by stat
// and by name, the text words and centring, `if` without nesting, the score blocks, and the words
// and values that draw nothing.
//
// ge:summary Tests: the sequel's layout language: cursor, numbers, pictures, text, conditions
// ge:keywords test sequel layout status-bar hnum anum rnum cstring client-block
// ge:skill ui testing
// ge:test tests/shooters_edge_sequel_layout_test.cpp
#include <array>
#include <string>
#include <vector>

#include "check.hpp"
#include "shooters_edge/hud/sequel_layout.hpp"

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
