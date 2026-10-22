// engine/lab/plot: a recorded drawing renders to a picture of its size and to SVG whose text,
// tooltips and escapes are there to read.
//
// ge:summary Test: the plot's picture and SVG renderers
// ge:keywords test plot svg picture tooltip escape
// ge:skill testing debug
#include <string>

#include "check.hpp"
#include "lab/plot.hpp"

namespace gl = ge::lab;

TEST(a_plot_renders_both_ways) {
  gl::Plot p;
  p.width = 120;
  p.height = 80;
  p.line(0, 0, 119, 79, {255, 0, 0});
  p.polyline({0, 10, 50, 20, 100, 10}, {0, 0, 255});
  p.rect(10, 10, 20, 20, {0, 255, 0}, "a <box> & more");
  p.ring(60, 40, 6, {208, 40, 40}, "spike");
  p.text(4, 4, "Speed (km/h)", {0, 0, 0});
  const gl::Picture pic = gl::to_picture(p);
  CHECK(pic.width == 120 && pic.height == 80);
  CHECK(pic.rgba[(15 * 120 + 15) * 4 + 1] == 255);  // inside the green box
  const std::string svg = gl::to_svg(p);
  CHECK(svg.find("<svg") == 0);
  CHECK(svg.find(">Speed (km/h)</text>") != std::string::npos);
  CHECK(svg.find("<title>a &lt;box&gt; &amp; more</title>") != std::string::npos);
  CHECK(svg.find("<polyline") != std::string::npos);
  CHECK(svg.find("<title>spike</title>") != std::string::npos);
}
