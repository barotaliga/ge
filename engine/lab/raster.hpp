// A SMALL RASTER for the labs: an RGBA8 picture, the few painters every lab picture needs (a
// box, a line, a disc, a polyline, a label in a 5x7 bitmap font), and alpha blending. It is how a
// lab draws BEFORE there is a renderer, and it stays the picture type after: a GPU frame read back
// (M7, the `render` skill) is handed to `Lab::shot` as a `Picture` and gets the same caption band,
// sidecar and contact sheet.
//
// Colours are 0..255; an alpha under 255 blends over what is there. Coordinates are pixels,
// y down; off-picture pixels are dropped (a lab that draws past its edge framed badly, which
// is a thing to see, not a thing to crash over). Pure integer output from double input, with
// no transcendental, so a picture's pixels are part of the lab's digest.
//
// ge:summary The labs' CPU raster: an RGBA8 Picture, boxes, lines, discs, a 5x7 font
// ge:keywords raster draw picture canvas rgba pixel font glyph text label line disc box
// ge:keywords blend
// ge:skill labs
// ge:test tests/lab_raster_test.cpp
#pragma once

#include <cstdint>
#include <string_view>
#include <vector>

namespace ge::lab {

struct Rgba {
  uint8_t r, g, b, a = 255;
};

struct Picture {
  int width = 0;
  int height = 0;
  std::vector<uint8_t> rgba;  // width * height * 4, row-major, y down
};

// Glyph cell: five columns and one of space, seven rows and one of lead.
inline constexpr int kGlyphW = 6;
inline constexpr int kGlyphH = 8;

Picture create_picture(int width, int height, Rgba background);

void set_pixel(Picture& p, int x, int y, Rgba c);
void fill_rect(Picture& p, double x, double y, double w, double h, Rgba c);
// A line `thickness` pixels wide (a square brush), endpoints included.
void draw_line(Picture& p, double x0, double y0, double x1, double y1, Rgba c,
               double thickness = 1);
void fill_disc(Picture& p, double cx, double cy, double radius, Rgba c);
void draw_ring(Picture& p, double cx, double cy, double radius, Rgba c);
// `text` at top-left (x, y), each glyph pixel a `scale`-square. Lower case is folded to upper;
// a character the font lacks draws blank.
void draw_text(Picture& p, double x, double y, std::string_view text, Rgba c, int scale = 1);
int text_width(std::string_view text, int scale = 1);

// `src` pasted with its top-left at (x, y), clipped to `dst`.
void blit(Picture& dst, const Picture& src, int x, int y);

// `src` drawn at `width` x `height`: each pixel the mean of the source pixels under it (the
// nearest one when it grows), in integers, so the bytes are the same on every row.
Picture resized(const Picture& src, int width, int height);

}  // namespace ge::lab
