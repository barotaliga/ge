// Tests: QC's text rules (plugins/progs/text.hpp): ftos's whole numbers and its five-wide one
// decimal, the same text for NaN and the infinities everywhere, the largest float, vtos, stof's
// decimal, hexadecimal and character readings, and the words of tokenize with the arguments line,
// quotes, comments and the limits.
//
// ge:summary Tests: ftos, vtos, stof and the words of tokenize
// ge:keywords test qc text ftos vtos stof tokenize words arguments-line format_ftos parse_stof
// ge:skill progs testing
#include <limits>
#include <string>

#include "check.hpp"
#include "progs/text.hpp"

namespace pg = ge::progs;

namespace {

std::string ftos(float v) {
  char buf[pg::kFtosBytes];
  pg::format_ftos(v, buf);
  return buf;
}

std::string vtos(float x, float y, float z) {
  char buf[pg::kVtosBytes];
  pg::format_vtos(x, y, z, buf);
  return buf;
}

}  // namespace

TEST(ftos_prints_whole_numbers_as_integers) {
  CHECK(ftos(0) == "0");
  CHECK(ftos(-0.0f) == "0");
  CHECK(ftos(42) == "42");
  CHECK(ftos(-7) == "-7");
  CHECK(ftos(16777216) == "16777216");
  // At or past 1e21: the shortest exponent form, the same on every platform.
  CHECK(ftos(std::numeric_limits<float>::max()) == "3.4028234663852886e+38");
}

TEST(ftos_prints_anything_else_with_one_decimal_five_wide) {
  CHECK(ftos(1.5f) == "  1.5");
  CHECK(ftos(-0.25f) == " -0.3");  // the exact decimal rounded half away from zero
  CHECK(ftos(0.04f) == "  0.0");
  CHECK(ftos(123456.5f) == "123456.5");
  const float inf = std::numeric_limits<float>::infinity();
  CHECK(ftos(inf) == "  inf");
  CHECK(ftos(-inf) == " -inf");
  CHECK(ftos(std::numeric_limits<float>::quiet_NaN()) == "  nan");
}

TEST(vtos_quotes_three_one_decimal_components) {
  CHECK(vtos(0, 1, -2) == "'  0.0   1.0  -2.0'");
  CHECK(vtos(1.25f, 300, 0.5f) == "'  1.3 300.0   0.5'");
}

TEST(stof_reads_decimal_hex_and_characters) {
  CHECK(pg::parse_stof("12.5") == 12.5f);
  CHECK(pg::parse_stof("  -3") == -3.0f);
  CHECK(pg::parse_stof("+4.25xyz") == 4.25f);
  CHECK(pg::parse_stof("0x1f") == 31.0f);
  CHECK(pg::parse_stof("-0X10") == -16.0f);
  CHECK(pg::parse_stof("'A") == 65.0f);
  CHECK(pg::parse_stof("") == 0.0f);
  CHECK(pg::parse_stof("abc") == 0.0f);
  CHECK(pg::parse_stof("1.2.3") == 1.2f);
  CHECK(pg::parse_stof("0.1") == 0.1f);
}

TEST(tokenize_splits_words_and_keeps_the_arguments_line) {
  pg::Words w;
  CHECK(pg::tokenize_words("say \"hello there\" all\n next line", w) == 3);
  CHECK(std::string(pg::word(w, 0)) == "say");
  CHECK(std::string(pg::word(w, 1)) == "hello there");
  CHECK(std::string(pg::word(w, 2)) == "all");
  CHECK(std::string(pg::word(w, 3)).empty());
  CHECK(std::string(pg::word(w, -1)).empty());
  CHECK(std::string(w.args) == "\"hello there\" all");
}

TEST(tokenize_skips_blanks_and_stops_at_a_comment) {
  pg::Words w;
  CHECK(pg::tokenize_words(" \t\r kill  // the rest", w) == 1);
  CHECK(std::string(pg::word(w, 0)) == "kill");
  CHECK(std::string(w.args).empty());
  CHECK(pg::tokenize_words("", w) == 0);
  CHECK(pg::tokenize_words("\"unclosed quote", w) == 1);
  CHECK(std::string(pg::word(w, 0)) == "unclosed quote");
}

TEST(tokenize_keeps_to_its_limits) {
  pg::Words w;
  std::string many;
  for (int i = 0; i < 100; i++) many += "w ";
  CHECK(pg::tokenize_words(many.c_str(), w) == pg::kMaxWords);
  // 1024 bytes of words, a terminator each: 14-letter words take 15 bytes, so 68 fit.
  std::string big;
  for (int i = 0; i < 70; i++) big += "abcdefghijklmn ";
  CHECK(pg::tokenize_words(big.c_str(), w) == 68);
  const std::string long_word(2000, 'x');
  CHECK(pg::tokenize_words(long_word.c_str(), w) == 1);
  CHECK(std::string(pg::word(w, 0)).size() == static_cast<size_t>(pg::kMaxWordBytes));
}
