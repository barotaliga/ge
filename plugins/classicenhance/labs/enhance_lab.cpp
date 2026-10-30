// LAB `enhance`: what the texture enhancer makes of a texture. Each texture is a captioned sheet:
// the source as the classic renderer draws it (nearest texels), the same upscaled by Catmull-Rom
// for comparison, the enhanced albedo, the albedo lit from the upper left through the generated
// normal map with the glow added, then the normal, height, roughness and (when anything glows)
// emissive maps; and an overview of every enhanced albedo tiled 2 × 2, where a seam would show.
// The numbers: each texture's quarter-size copy upscaled 4× back by nearest, Catmull-Rom and the
// enhancer, as PSNR against the full-size picture.
//
// `--weights=<file>` adds the trained network (upscaler.hpp) beside the kernel: a NETWORK column
// on every sheet, its PSNR in the table and its time; the weights are the classicenhance tool's
// fetched copy, or any file of that network.
//
// By default it runs on the lab's own painted fixtures (fixtures.hpp), so its pictures and digest
// are the same everywhere and may be published. `--source=<path>` runs it on the classic pack
// found there (source.hpp: any pack that reads); those pictures are derived from the
// pack and stay on this machine: never attach them to a PR or publish them.
//
// ge:summary Lab: textures beside their enhanced albedo, lit view and maps; PSNR against cubic
// ge:keywords lab enhance texture upscale normal height roughness emissive psnr catmull-rom sheet
// ge:skill classicenhance labs
// ge:test tests/lab_runner_test.cpp
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

#include "classicenhance/cache.hpp"
#include "classicenhance/labs/fixtures.hpp"
#include "classicenhance/pipeline.hpp"
#include "classicenhance/planes.hpp"
#include "classicenhance/source.hpp"
#include "classicenhance/texture.hpp"
#include "classicenhance/upscaler.hpp"
#include "core/libm/libm.hpp"
#include "lab/lab.hpp"

namespace {

namespace ce = ge::classicenhance;
using ge::lab::Picture;

struct Tex {
  std::string name;
  int w, h;
  std::vector<uint8_t> indexed;
};

Picture pic(const ge::assets::Image& img) { return {img.width, img.height, img.rgba}; }

// Catmull-Rom in linear light, wrapping, `scale` times larger.
Picture cubic(const Tex& t, const ce::Palette& p, int scale) {
  const int ow = t.w * scale, oh = t.h * scale;
  Picture out{ow, oh, std::vector<uint8_t>(size_t(ow) * size_t(oh) * 4, 255)};
  auto w = [](double x) {
    x = std::fabs(x);
    return x < 1   ? 1.5 * x * x * x - 2.5 * x * x + 1
           : x < 2 ? -0.5 * x * x * x + 2.5 * x * x - 4 * x + 2
                   : 0.0;
  };
  for (int y = 0; y < oh; y++) {
    const double sy = (y + 0.5) / scale - 0.5;
    const int iy = int(std::floor(sy));
    for (int x = 0; x < ow; x++) {
      const double sx = (x + 0.5) / scale - 0.5;
      const int ix = int(std::floor(sx));
      for (int c = 0; c < 3; c++) {
        double acc = 0;
        for (int j = -1; j <= 2; j++) {
          for (int i = -1; i <= 2; i++) {
            const int xx = ((ix + i) % t.w + t.w) % t.w, yy = ((iy + j) % t.h + t.h) % t.h;
            const uint8_t k = t.indexed[size_t(yy) * size_t(t.w) + size_t(xx)];
            acc += w(sx - (ix + i)) * w(sy - (iy + j)) *
                   ce::srgb8_to_linear(p.rgb[size_t(k) * 3 + size_t(c)]);
          }
        }
        out.rgba[(size_t(y) * size_t(ow) + size_t(x)) * 4 + size_t(c)] =
            ce::linear_to_srgb8(float(acc));
      }
    }
  }
  return out;
}

Picture nearest(const Tex& t, const ce::Palette& p, int scale) {
  const auto img = ce::to_rgba(t.indexed, t.w, t.h, p);
  return ge::lab::resized({img.width, img.height, img.rgba}, t.w * scale, t.h * scale);
}

// The albedo lit by a light from the upper left and in front, through the normal map, plus glow.
Picture lit(const ce::Enhanced& e) {
  Picture out = pic(e.albedo);
  if (e.normal.width == 0) return out;
  const double lx = -0.5, ly = 0.5, lz = 0.7071;
  for (size_t i = 0; i < size_t(out.width) * size_t(out.height); i++) {
    const double nx = e.normal.rgba[4 * i] / 127.5 - 1, ny = e.normal.rgba[4 * i + 1] / 127.5 - 1,
                 nz = e.normal.rgba[4 * i + 2] / 127.5 - 1;
    const double d = std::max(0.0, nx * lx + ny * ly + nz * lz);
    const double k = 0.25 + 0.95 * d;
    for (int c = 0; c < 3; c++) {
      const double base = ce::srgb8_to_linear(e.albedo.rgba[4 * i + size_t(c)]) * k;
      const double glow = e.glows ? ce::srgb8_to_linear(e.emissive.rgba[4 * i + size_t(c)]) : 0.0;
      out.rgba[4 * i + size_t(c)] = ce::linear_to_srgb8(float(base + glow));
    }
  }
  return out;
}

// The texture at a quarter of its size, each texel the palette colour nearest the mean of the
// 4 × 4 it covers (in linear light): the picture the enhancer gets back to full size from.
Tex quarter(const Tex& t, const ce::Palette& p) {
  Tex q{t.name, t.w / 4, t.h / 4, {}};
  q.indexed.resize(size_t(q.w) * size_t(q.h));
  for (int y = 0; y < q.h; y++) {
    for (int x = 0; x < q.w; x++) {
      double rgb[3] = {};
      for (int j = 0; j < 4; j++) {
        for (int i = 0; i < 4; i++) {
          const uint8_t k = t.indexed[size_t(y * 4 + j) * size_t(t.w) + size_t(x * 4 + i)];
          for (int c = 0; c < 3; c++)
            rgb[c] += ce::srgb8_to_linear(p.rgb[size_t(k) * 3 + size_t(c)]) / 16;
        }
      }
      q.indexed[size_t(y) * size_t(q.w) + size_t(x)] = ge::classicenhance::labs::nearest_index(
          p, ce::linear_to_srgb8(float(rgb[0])), ce::linear_to_srgb8(float(rgb[1])),
          ce::linear_to_srgb8(float(rgb[2])));
    }
  }
  return q;
}

double psnr(const Picture& a, const Tex& t, const ce::Palette& p) {
  double se = 0;
  for (size_t i = 0; i < size_t(t.w) * size_t(t.h); i++) {
    const uint8_t k = t.indexed[i];
    for (size_t c = 0; c < 3; c++) {
      const double d = double(a.rgba[4 * i + c]) - p.rgb[size_t(k) * 3 + c];
      se += d * d;
    }
  }
  const double mse = std::max(1e-9, se / (double(t.w) * t.h * 3));
  return 10 * ge::core::libm::log(255.0 * 255.0 / mse) / ge::core::libm::log(10.0);
}

std::string f2(double v) {
  char b[32];
  std::snprintf(b, sizeof b, "%.2f", v);
  return b;
}

void run(ge::lab::Lab& lab) {
  const int scale = std::clamp(int(lab.arg_num("scale")), 1, 8);
  const std::string path = lab.arg("source");
  ce::Palette palette;
  std::vector<Tex> textures;
  if (path.empty()) {
    palette = ce::labs::fixture_palette();
    for (auto& f : ce::labs::fixture_textures(palette)) {
      textures.push_back({f.name, f.width, f.height, f.indexed});
    }
  } else {
    ce::Source src;
    std::string error;
    if (!ce::open_source(path, &src, &error)) {
      lab.fail(error, "next: give --source a release zip, an installed folder or its pack.");
      return;
    }
    std::vector<ce::PackTexture> all;
    if (!ce::pack_textures(src.content(), &palette, &all, &error)) {
      lab.fail(
          error,
          "next: check the pack with `ge tool classicenhance -- verify`; skill: classicenhance.");
      return;
    }
    const std::string names = "," + lab.arg("names") + ",";
    for (auto& t : all) {
      if (names != ",," && names.find("," + t.name + ",") == std::string::npos) continue;
      textures.push_back({t.name, t.width, t.height, t.indexed});
      if (names == ",," && textures.size() >= size_t(lab.arg_num("limit"))) break;
    }
    lab.note(
        "local run on the classic pack: these pictures are derived from it; keep them on "
        "this machine");
  }

  ce::EnhanceOptions opts;
  opts.scale = scale;
  ce::Upscaler upscaler;
  const std::string weights = lab.arg("weights");
  if (!weights.empty()) {
    std::string error;
    if (!ce::open_upscaler(weights, upscaler, nullptr, error)) {
      lab.fail(error,
               "next: give --weights the upscaler's weights file (the classicenhance "
               "tool fetches it into the cache's weights/ folder).");
      return;
    }
  }
  ce::EnhanceOptions net = opts;
  net.network = weights.empty() ? nullptr : &upscaler;
  ge::lab::LabTable table{"each texture's quarter-size copy upscaled 4x back: PSNR, dB",
                          {"texture", "size", "material", "nearest", "catmull-rom", "enhanced"},
                          {}};
  if (net.network) table.columns.push_back("network");
  double sum_cubic = 0, sum_ours = 0, sum_net = 0;
  int scored = 0;
  double seconds = 0, net_seconds = 0;
  size_t texels = 0;
  std::vector<Picture> tiled;
  for (const Tex& t : textures) {
    const auto t0 = std::chrono::steady_clock::now();
    const ce::Enhanced e = ce::enhance_texture(t.name, t.indexed, t.w, t.h, palette, opts);
    seconds += std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
    texels += size_t(t.w) * size_t(t.h);
    ce::Enhanced n;
    if (net.network) {
      const auto t1 = std::chrono::steady_clock::now();
      n = ce::enhance_texture(t.name, t.indexed, t.w, t.h, palette, net);
      net_seconds += std::chrono::duration<double>(std::chrono::steady_clock::now() - t1).count();
    }

    std::vector<std::pair<std::string, Picture>> cols = {{"SOURCE", nearest(t, palette, scale)},
                                                         {"CATMULL-ROM", cubic(t, palette, scale)},
                                                         {"ENHANCED", pic(e.albedo)},
                                                         {"LIT", lit(e)}};
    if (net.network) {
      cols.push_back({"NETWORK", pic(n.albedo)});
      cols.push_back({"NETWORK LIT", lit(n)});
    }
    if (e.normal.width > 0) {
      cols.push_back({"NORMAL", pic(e.normal)});
      cols.push_back({"HEIGHT", pic(e.height)});
      cols.push_back({"ROUGHNESS", pic(e.roughness)});
    }
    if (e.glows) cols.push_back({"EMISSIVE", pic(e.emissive)});
    const int cw = t.w * scale, ch = t.h * scale, gap = 6, label = 14;
    Picture sheet =
        ge::lab::create_picture(int(cols.size()) * (cw + gap) - gap, ch + label, {24, 24, 28});
    for (size_t i = 0; i < cols.size(); i++) {
      const int x = int(i) * (cw + gap);
      ge::lab::draw_text(sheet, x + 2, 3, cols[i].first, {230, 230, 230});
      ge::lab::blit(sheet, cols[i].second, x, label);
    }
    const std::string id = ce::safe_name(t.name);
    lab.shot(
        {id,
         t.name + " ENHANCED " + std::to_string(scale) + "X",
         "Texture '" + t.name + "' (" + std::to_string(t.w) + " x " + std::to_string(t.h) + ", " +
             ce::material_name(e.material) + ") at " + std::to_string(scale) +
             "x: the source's texels, Catmull-Rom, the enhancer's albedo, that albedo lit "
             "from the upper left through the generated normal map (with any glow), and "
             "the generated maps.",
         "edges as crisp as the source's without stair steps or halos; the lit view's "
         "relief reading the right way (seams and mortar sunk, panels and bricks raised); "
         "glow only where the source's fullbright texels were",
         {{"scale", std::to_string(scale)}, {"source", path.empty() ? "fixtures" : "local pack"}}},
        sheet);

    Picture tile = ge::lab::create_picture(2 * e.albedo.width, 2 * e.albedo.height, {0, 0, 0});
    for (int k = 0; k < 4; k++) {
      ge::lab::blit(tile, pic(e.albedo), (k % 2) * e.albedo.width, (k / 2) * e.albedo.height);
    }
    tiled.push_back(ge::lab::resized(tile, 256, 256 * tile.height / std::max(1, tile.width)));

    if (t.w >= 16 && t.h >= 16 && t.w % 4 == 0 && t.h % 4 == 0) {
      const Tex q = quarter(t, palette);
      ce::EnhanceOptions four = opts;
      four.scale = 4;
      four.grain = 0;
      const double pn = psnr(nearest(q, palette, 4), t, palette);
      const double pc = psnr(cubic(q, palette, 4), t, palette);
      const double pe = psnr(
          pic(ce::enhance_texture(q.name, q.indexed, q.w, q.h, palette, four).albedo), t, palette);
      table.rows.push_back({t.name, std::to_string(t.w) + "x" + std::to_string(t.h),
                            ce::material_name(e.material), f2(pn), f2(pc), f2(pe)});
      if (net.network && net.network->scale == 4) {
        ce::EnhanceOptions nf = net;
        nf.scale = 4;
        const double pq = psnr(
            pic(ce::enhance_texture(q.name, q.indexed, q.w, q.h, palette, nf).albedo), t, palette);
        table.rows.back().push_back(f2(pq));
        sum_net += pq;
      }
      sum_cubic += pc;
      sum_ours += pe;
      scored++;
    }
  }
  if (scored > 0) {
    lab.metric("psnr-catmull-rom", sum_cubic / scored, "dB");
    lab.metric("psnr-enhanced", sum_ours / scored, "dB");
    if (net.network) lab.metric("psnr-network", sum_net / scored, "dB");
  }
  if (net.network) {
    lab.metric("network-seconds", net_seconds, "s", ge::lab::Det::measured);
    lab.metric("network-seconds-per-kilotexel", net_seconds / std::max(1.0, double(texels) / 1000),
               "s", ge::lab::Det::measured);
  }
  lab.metric("textures", double(textures.size()), "count");
  lab.metric("enhance-seconds", seconds, "s", ge::lab::Det::measured);
  lab.table(std::move(table));

  if (!tiled.empty()) {
    const int cols = std::min<int>(6, int(tiled.size()));
    const int rows = (int(tiled.size()) + cols - 1) / cols;
    int cell_h = 0;
    for (const Picture& p : tiled) cell_h = std::max(cell_h, p.height);
    Picture over = ge::lab::create_picture(cols * 262 - 6, rows * (cell_h + 6) - 6, {24, 24, 28});
    for (size_t i = 0; i < tiled.size(); i++) {
      ge::lab::blit(over, tiled[i], int(i % size_t(cols)) * 262,
                    int(i / size_t(cols)) * (cell_h + 6));
    }
    lab.shot({"tiled",
              "EVERY ALBEDO TILED 2 X 2",
              "Each enhanced albedo repeated twice across and twice down, as a wall repeats it.",
              "no seam where the copies meet: the enhancer wraps at the edges",
              {{"scale", std::to_string(scale)}}},
             over);
  }
}

const ge::lab::LabSpec kSpec{
    "enhance",
    ge::lab::LabKind::picture,
    "plugin:classicenhance",
    "plugins/classicenhance/texture",
    "classicenhance",
    "What does the enhancer make of a texture: is it sharper than a cubic upscale, does its relief "
    "read, does it tile?",
    {
        {"scale", "4", "output texels a source texel"},
        {"source", "", "the classic pack (zip, folder or pack); empty: the lab's fixtures"},
        {"limit", "8", "with --source, how many textures (in the pack's order)"},
        {"names", "", "with --source, texture names, comma-separated, in place of --limit"},
        {"weights", "", "the trained upscaler's weights file: adds its column; empty: kernel only"},
    },
    "--scale=2",
    run,
};
GE_LAB(kSpec);

}  // namespace
