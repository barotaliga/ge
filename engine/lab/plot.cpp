// The plot's two renderers (plot.hpp): the raster's painters, and SVG text.
//
// ge:summary The plot's renderers: to a raster Picture and to SVG text
// ge:keywords plot svg render raster to_svg to_picture
// ge:skill debug labs
// ge:test tests/lab_plot_test.cpp
#include "lab/plot.hpp"

#include "core/num_text.hpp"

namespace ge::lab {

namespace {

std::string n(double v) { return core::num::to_fixed(v, 1); }

std::string colour(Rgba c) {
  static const char* kHex = "0123456789abcdef";
  std::string s = "#";
  for (const uint8_t b : {c.r, c.g, c.b}) {
    s.push_back(kHex[b >> 4]);
    s.push_back(kHex[b & 15]);
  }
  return s;
}

std::string opacity(Rgba c) {
  if (c.a == 255) return "";
  return " opacity=\"" + core::num::to_fixed(c.a / 255.0, 3) + "\"";
}

std::string escape(const std::string& s) {
  std::string out;
  out.reserve(s.size());
  for (const char ch : s) {
    switch (ch) {
      case '&':
        out += "&amp;";
        break;
      case '<':
        out += "&lt;";
        break;
      case '>':
        out += "&gt;";
        break;
      case '"':
        out += "&quot;";
        break;
      default:
        out.push_back(ch);
    }
  }
  return out;
}

// An element closed with its tooltip, or self-closed without one.
std::string close(const std::string& open, const std::string& tag, const std::string& tip) {
  if (tip.empty()) return open + "/>\n";
  return open + "><title>" + escape(tip) + "</title></" + tag + ">\n";
}

PlotOp make_op(PlotOp::Kind kind, Rgba c) {
  PlotOp op;
  op.kind = kind;
  op.colour = c;
  return op;
}

}  // namespace

void Plot::line(double x0, double y0, double x1, double y1, Rgba c, double w) {
  PlotOp op = make_op(PlotOp::Kind::line, c);
  op.x = x0;
  op.y = y0;
  op.x1 = x1;
  op.y1 = y1;
  op.size = w;
  ops.push_back(std::move(op));
}

void Plot::polyline(std::vector<double> points, Rgba c, double w) {
  PlotOp op = make_op(PlotOp::Kind::polyline, c);
  op.points = std::move(points);
  op.size = w;
  ops.push_back(std::move(op));
}

void Plot::rect(double x, double y, double w, double h, Rgba c, std::string tip) {
  PlotOp op = make_op(PlotOp::Kind::rect, c);
  op.x = x;
  op.y = y;
  op.x1 = w;
  op.y1 = h;
  op.tip = std::move(tip);
  ops.push_back(std::move(op));
}

void Plot::disc(double x, double y, double r, Rgba c, std::string tip) {
  PlotOp op = make_op(PlotOp::Kind::disc, c);
  op.x = x;
  op.y = y;
  op.size = r;
  op.tip = std::move(tip);
  ops.push_back(std::move(op));
}

void Plot::ring(double x, double y, double r, Rgba c, std::string tip) {
  PlotOp op = make_op(PlotOp::Kind::ring, c);
  op.x = x;
  op.y = y;
  op.size = r;
  op.tip = std::move(tip);
  ops.push_back(std::move(op));
}

void Plot::text(double x, double y, std::string s, Rgba c, int scale) {
  PlotOp op = make_op(PlotOp::Kind::text, c);
  op.x = x;
  op.y = y;
  op.size = scale;
  op.text = std::move(s);
  ops.push_back(std::move(op));
}

Picture to_picture(const Plot& plot) {
  Picture p = create_picture(plot.width, plot.height, plot.background);
  for (const PlotOp& op : plot.ops) {
    switch (op.kind) {
      case PlotOp::Kind::line:
        draw_line(p, op.x, op.y, op.x1, op.y1, op.colour, op.size);
        break;
      case PlotOp::Kind::polyline:
        for (size_t i = 2; i + 1 < op.points.size(); i += 2) {
          draw_line(p, op.points[i - 2], op.points[i - 1], op.points[i], op.points[i + 1],
                    op.colour, op.size);
        }
        break;
      case PlotOp::Kind::rect:
        fill_rect(p, op.x, op.y, op.x1, op.y1, op.colour);
        break;
      case PlotOp::Kind::disc:
        fill_disc(p, op.x, op.y, op.size, op.colour);
        break;
      case PlotOp::Kind::ring:
        draw_ring(p, op.x, op.y, op.size, op.colour);
        draw_ring(p, op.x, op.y, op.size + 1, op.colour);
        break;
      case PlotOp::Kind::text:
        draw_text(p, op.x, op.y, op.text, op.colour, static_cast<int>(op.size));
        break;
    }
  }
  return p;
}

std::string to_svg(const Plot& plot) {
  std::string s = "<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"" +
                  std::to_string(plot.width) + "\" height=\"" + std::to_string(plot.height) +
                  "\" viewBox=\"0 0 " + std::to_string(plot.width) + " " +
                  std::to_string(plot.height) +
                  "\" font-family=\"ui-monospace, Menlo, Consolas, monospace\">\n";
  s += "<rect width=\"100%\" height=\"100%\" fill=\"" + colour(plot.background) + "\"/>\n";
  for (const PlotOp& op : plot.ops) {
    const std::string fill = "fill=\"" + colour(op.colour) + "\"" + opacity(op.colour);
    const std::string stroke = "stroke=\"" + colour(op.colour) + "\"" + opacity(op.colour);
    switch (op.kind) {
      case PlotOp::Kind::line:
        s += "<line x1=\"" + n(op.x) + "\" y1=\"" + n(op.y) + "\" x2=\"" + n(op.x1) + "\" y2=\"" +
             n(op.y1) + "\" " + stroke + " stroke-width=\"" + n(op.size) + "\"/>\n";
        break;
      case PlotOp::Kind::polyline: {
        std::string pts;
        for (size_t i = 0; i + 1 < op.points.size(); i += 2) {
          if (i) pts.push_back(' ');
          pts += n(op.points[i]) + "," + n(op.points[i + 1]);
        }
        s += "<polyline points=\"";
        s += pts;
        s += "\" fill=\"none\" " + stroke + " stroke-width=\"" + n(op.size) +
             "\" stroke-linejoin=\"round\"/>\n";
        break;
      }
      case PlotOp::Kind::rect:
        s += close("<rect x=\"" + n(op.x) + "\" y=\"" + n(op.y) + "\" width=\"" + n(op.x1) +
                       "\" height=\"" + n(op.y1) + "\" " + fill,
                   "rect", op.tip);
        break;
      case PlotOp::Kind::disc:
        s += close("<circle cx=\"" + n(op.x) + "\" cy=\"" + n(op.y) + "\" r=\"" + n(op.size) +
                       "\" " + fill,
                   "circle", op.tip);
        break;
      case PlotOp::Kind::ring:
        s += close("<circle cx=\"" + n(op.x) + "\" cy=\"" + n(op.y) + "\" r=\"" + n(op.size) +
                       "\" fill=\"none\" " + stroke + " stroke-width=\"2\"",
                   "circle", op.tip);
        break;
      case PlotOp::Kind::text:
        // The raster's glyph cell is 6x8 a scale step; the baseline sits at 7 of the 8.
        s += "<text x=\"" + n(op.x) + "\" y=\"" + n(op.y + 7 * op.size) + "\" font-size=\"" +
             n(9 * op.size) + "\" " + fill + ">" + escape(op.text) + "</text>\n";
        break;
    }
  }
  s += "</svg>\n";
  return s;
}

}  // namespace ge::lab
