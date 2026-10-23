// engine/render/stars.hpp: the table decodes to the catalogue's 9,096 stars, brightest first, the
// sky's own counts to each magnitude; the brightest is where the sky has it and the pole star
// stands at the pole; a catalogue line parses to its place and magnitude, and a table round-trips;
// a star's cube cell is where its direction falls; and the map keeps the brightest of a cell.
//
// ge:summary Tests for the real stars: the table, the counts, the parse, the cube map
// ge:keywords test render stars star-catalogue magnitude parse encode decode cube-cell star_map
// ge:skill render testing
#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

#include "check.hpp"
#include "render/stars.hpp"

namespace {

namespace gr = ge::render;
constexpr double kPi = 3.14159265358979323846;
constexpr double kDeg = kPi / 180;

}  // namespace

TEST(the_table_is_the_catalogues_stars_brightest_first) {
  const auto& stars = gr::star_catalogue();
  CHECK(stars.size() == 9096);
  for (size_t i = 1; i < stars.size(); ++i) CHECK(stars[i].mag >= stars[i - 1].mag);
  auto brighter = [&](double m) {
    return std::count_if(stars.begin(), stars.end(), [m](const gr::Star& s) { return s.mag < m; });
  };
  // The sky's own counts: some fifteen stars to the first magnitude, five thousand to the sixth.
  CHECK(brighter(1) >= 12 && brighter(1) <= 18);
  CHECK(brighter(6) >= 4600 && brighter(6) <= 5200);
}

TEST(the_brightest_star_and_the_pole_star_are_in_their_places) {
  const gr::Star& brightest = gr::star_catalogue().front();
  CHECK(std::fabs(brightest.mag + 1.46) < 0.06);
  CHECK(std::fabs(brightest.ra - (6 + 45.15 / 60) / 12 * kPi) < 0.01);
  CHECK(std::fabs(brightest.dec + 16.72 * kDeg) < 0.01);
  // The second-magnitude star within a degree of the north pole.
  int near_pole = 0;
  for (const gr::Star& s : gr::star_catalogue())
    if (s.dec > 89 * kDeg && s.mag < 2.2) ++near_pole;
  CHECK(near_pole == 1);
}

TEST(a_catalogue_line_parses_and_a_table_round_trips) {
  // A line in the catalogue's columns: right ascension 06 45 08.9, declination -16 42 58,
  // magnitude -1.46, B-V 0.00.
  std::string line(147, ' ');
  line.replace(75, 15, "064508.9-164258");
  line.replace(102, 5, "-1.46");
  line.replace(109, 5, " 0.00");
  gr::Star s;
  CHECK(gr::parse_catalogue_line(line, s));
  CHECK(std::fabs(s.ra - (6 + 45 / 60.0 + 8.9 / 3600) / 12 * kPi) < 1e-9);
  CHECK(std::fabs(s.dec + (16 + 42 / 60.0 + 58 / 3600.0) * kDeg) < 1e-9);
  CHECK(s.mag == -1.46 && s.bv == 0);
  std::string blank(147, ' ');
  CHECK(!gr::parse_catalogue_line(blank, s));

  std::vector<gr::Star> in = {{1.0, -0.4, 3.21, 0.48}, {6.0, 1.2, -0.7, 1.6}};
  std::vector<gr::Star> out;
  CHECK(gr::decode_stars(gr::encode_stars(in), out));
  CHECK(out.size() == 2);
  for (size_t i = 0; i < 2; ++i) {
    CHECK(std::fabs(out[i].ra - in[i].ra) < 1e-4);
    CHECK(std::fabs(out[i].dec - in[i].dec) < 1e-4);
    CHECK(std::fabs(out[i].mag - in[i].mag) <= 0.025 + 1e-9);
    CHECK(std::fabs(out[i].bv - in[i].bv) <= 0.0072);
  }
  CHECK(!gr::decode_stars("not base64!", out));
}

TEST(a_stars_cell_is_where_its_direction_falls) {
  for (const gr::Star& s : gr::star_catalogue()) {
    if (s.mag > 2) break;
    double d[3];
    gr::star_direction(s, d);
    CHECK(std::fabs(d[0] * d[0] + d[1] * d[1] + d[2] * d[2] - 1) < 1e-12);
    const gr::CubeCell c = gr::cube_cell(d);
    CHECK(c.face >= 0 && c.face < 6 && c.col >= 0 && c.col < gr::kStarCells && c.row >= 0 &&
          c.row < gr::kStarCells);
    CHECK(c.u >= 0 && c.u < 1 && c.v >= 0 && c.v < 1);
    // The axis the face names is the direction's largest.
    const int axis = c.face % 3;
    for (int k = 0; k < 3; ++k) CHECK(std::fabs(d[axis]) >= std::fabs(d[k]));
    CHECK((c.face < 3) == (d[axis] > 0));
  }
}

TEST(the_map_keeps_the_brightest_of_a_cell) {
  // Four stars in one cell: the three brightest are kept, brightest first.
  std::vector<gr::Star> four = {{0.001, 0.001, 5.0, 0.5},
                                {0.0012, 0.0011, 2.0, 0.5},
                                {0.0011, 0.0009, 4.0, 0.5},
                                {0.0009, 0.0012, 3.0, 0.5}};
  const gr::StarMap m = gr::star_map(four);
  CHECK(m.dropped == 1);
  double d[3];
  gr::star_direction(four[0], d);
  const gr::CubeCell c = gr::cube_cell(d);
  const size_t width = size_t{6} * gr::kStarCells, layer = width * gr::kStarCells;
  const size_t at = static_cast<size_t>(c.row) * width +
                    static_cast<size_t>(c.face) * gr::kStarCells + static_cast<size_t>(c.col);
  CHECK(m.cells[at].mag == 2.0f && m.cells[layer + at].mag == 3.0f &&
        m.cells[2 * layer + at].mag == 4.0f);
  // The whole sky drops few, none of them bright.
  const gr::StarMap sky = gr::star_map(gr::star_catalogue());
  CHECK(sky.dropped < 30);
}
