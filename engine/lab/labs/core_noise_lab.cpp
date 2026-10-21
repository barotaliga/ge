// LAB `noise`: the value noise every spatial thing in the world is built on
// (engine/core/noise.hpp), LOOKED AT. One captioned picture a scale, grey from 0 (black) to 1
// (white), with the lattice drawn over it, plus the field's range and mean; and the check that the
// cached field
// (`sample_noise`) reads the same bits as the plain function (`value_noise`) everywhere drawn.
//
// ge:summary Lab: value noise pictured at several scales, its range, cached-field agreement
// ge:keywords lab noise value-noise picture lattice scale sample_noise value_noise
// ge:skill labs determinism
// ge:test tests/lab_runner_test.cpp
#include <cstring>
#include <string>

#include "core/noise.hpp"
#include "lab/lab.hpp"

namespace {

void run(ge::lab::Lab& lab) {
  const int size = static_cast<int>(lab.arg_num("size"));
  const double metres = lab.arg_num("metres");
  const double seed = lab.arg_num("seed");
  for (const double scale : lab.arg_nums("scales")) {
    auto pic = ge::lab::create_picture(size, size, {0, 0, 0});
    auto field = ge::core::noise_field(scale, seed);
    double lo = 1, hi = 0, sum = 0;
    int mismatched = 0;
    for (int py = 0; py < size; py++) {
      for (int px = 0; px < size; px++) {
        const double x = px * metres / size, z = py * metres / size;
        const double v = ge::core::sample_noise(field, x, z);
        const double plain = ge::core::value_noise(x, z, scale, seed);
        if (!(v == plain)) mismatched++;  // no NaN here, so value equality is bit equality
        lo = v < lo ? v : lo;
        hi = v > hi ? v : hi;
        sum += v;
        const auto g = static_cast<uint8_t>(v * 255.0);
        ge::lab::set_pixel(pic, px, py, {g, g, g});
      }
    }
    // The lattice: where hash2 is sampled.
    for (int k = 0; k * scale <= metres; k++) {
      const double p = k * scale * size / metres;
      ge::lab::draw_line(pic, p, 0, p, size, {255, 80, 80, 90});
      ge::lab::draw_line(pic, 0, p, size, p, {255, 80, 80, 90});
    }
    const std::string id = "scale-" + std::to_string(static_cast<int>(scale));
    lab.metric(id + ".min", lo, "");
    lab.metric(id + ".max", hi, "");
    lab.metric(id + ".mean", sum / (double(size) * size), "");
    if (mismatched) {
      lab.fail(std::to_string(mismatched) +
                   " pixels where sample_noise and value_noise differ "
                   "at scale " +
                   std::to_string(scale),
               "The cached field must read the same bits as the function: run "
               "tests/core_noise_test.cpp. Skill: determinism.");
    }
    lab.shot({id,
              "VALUE NOISE, SCALE " + std::to_string(static_cast<int>(scale)) + " M",
              "value_noise over " + std::to_string(static_cast<int>(metres)) +
                  " m square, black 0 to white 1, the lattice (one hash2 a corner) in red.",
              "smooth bilinear cells with no seams at the red lines; no banding; the range "
              "spread over most of 0..1",
              {{"seed", lab.arg("seed")},
               {"scale", std::to_string(scale)},
               {"metres", lab.arg("metres")}}},
             pic);
  }
}

const ge::lab::LabSpec kSpec{
    "noise",
    ge::lab::LabKind::picture,
    "engine",
    "engine/core/noise.hpp",
    "determinism",
    "What does the value noise look like at each scale, and does the cached field agree?",
    {
        {"scales", "64,256", "noise periods, m, comma-separated"},
        {"metres", "1024", "the square drawn, m"},
        {"size", "512", "the picture's side, px"},
        {"seed", "38", "the noise seed"},
    },
    "--scales=64 --size=128 --metres=512",
    run,
};
GE_LAB(kSpec);

}  // namespace
