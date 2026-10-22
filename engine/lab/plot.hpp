// A PLOT is a recorded drawing (lines, polylines, boxes, discs, rings, text) that renders two
// ways: to a `Picture` through the labs' CPU raster (so it is a lab shot, captioned and hashed
// like any other) and to SVG text (so an agent can read every label and value as text, and a
// person can zoom it). The chart builders (chart.hpp) draw into one; nothing else needs to.
//
// Coordinates are pixels, y down, as the raster's. Text is the raster's 5x7 font in the PNG and
// a monospace <text> of the same cell size in the SVG; a `tip` on an op becomes the SVG
// element's <title>, a hover tooltip, so the exact value of a point is in the file.
//
// ge:summary A recorded drawing rendered to a Picture (the raster) or to SVG text
// ge:keywords plot canvas svg vector drawing display-list to_svg to_picture polyline tooltip
// ge:skill debug labs
// ge:test tests/lab_plot_test.cpp
#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "lab/raster.hpp"

namespace ge::lab {

struct PlotOp {
  enum class Kind : uint8_t { line, polyline, rect, disc, ring, text };
  Kind kind = Kind::line;
  Rgba colour{0, 0, 0};
  double x = 0, y = 0, x1 = 0, y1 = 0;  // line: ends; rect: x, y, x1 = w, y1 = h; text: top-left
  double size = 1;                      // line width, a disc's or ring's radius, text scale
  std::vector<double> points;           // polyline: x0, y0, x1, y1, ...
  std::string text;
  std::string tip;
};

struct Plot {
  int width = 0;
  int height = 0;
  Rgba background{252, 252, 251};
  std::vector<PlotOp> ops;

  void line(double x0, double y0, double x1, double y1, Rgba c, double w = 1);
  void polyline(std::vector<double> points, Rgba c, double w = 2);
  void rect(double x, double y, double w, double h, Rgba c, std::string tip = "");
  void disc(double x, double y, double r, Rgba c, std::string tip = "");
  void ring(double x, double y, double r, Rgba c, std::string tip = "");
  void text(double x, double y, std::string s, Rgba c, int scale = 1);
};

Picture to_picture(const Plot& plot);
std::string to_svg(const Plot& plot);

}  // namespace ge::lab
