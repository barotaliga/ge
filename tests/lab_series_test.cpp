// engine/lab/series: the text format written and read back to the same doubles, numeric and text
// columns told apart, meta lines, the x column, a spans file recognised, and errors that name the
// line.
//
// ge:summary Test: the series file's round trip, columns, meta, x, spans and errors
// ge:keywords test series tsv read_series write_series spans
// ge:skill testing debug
#include <cmath>
#include <string>

#include "check.hpp"
#include "lab/series.hpp"

namespace gl = ge::lab;

TEST(a_series_reads_back_to_the_same_bits) {
  gl::Series s = gl::make_series({"t_s", "speed"});
  s.set("unit.speed", "km/h");
  s.set("x", "t_s");
  gl::add_row(s, {0, 0.1});
  gl::add_row(s, {1.0 / 120, 54.123456789012345});
  gl::add_row(s, {2.0 / 120, std::nan("")});
  const std::string text = gl::write_series(s);
  gl::Series back;
  std::string error;
  CHECK(gl::read_series(text, back, error));
  CHECK(back.rows() == 3);
  CHECK(back.unit("speed") == "km/h");
  CHECK(back.x() && back.x()->name == "t_s");
  CHECK_BITS(back.find("t_s")->num[1], 1.0 / 120);
  CHECK_BITS(back.find("speed")->num[1], 54.123456789012345);
  CHECK(std::isnan(back.find("speed")->num[2]));
  CHECK(gl::write_series(back) == text);
}

TEST(a_spans_file_is_recognised) {
  gl::Series s;
  std::string error;
  CHECK(
      gl::read_series("# title: a frame\nname\tdepth\tstart_ns\tdur_ns\nframe\t0\t0\t1000\n"
                      "step\t1\t0\t600\n",
                      s, error));
  CHECK(s.is_spans());
  CHECK(s.find("name")->text);
  CHECK(s.find("name")->cell[1] == "step");
  CHECK(s.get("title") == "a frame");
  CHECK(s.x() && s.x()->name == "depth");  // the first numeric column
}

TEST(a_bad_file_names_its_line) {
  gl::Series s;
  std::string error;
  CHECK(!gl::read_series("a\tb\n1\t2\n3\n", s, error));
  CHECK(error.find("line 3") != std::string::npos);
  CHECK(!gl::read_series("# only: meta\n", s, error));
  CHECK(error.find("no header") != std::string::npos);
}
