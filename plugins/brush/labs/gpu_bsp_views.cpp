// The `gpu-bsp` lab's hand-picked views read (gpu_bsp_views.hpp).
//
// ge:summary Reads the gpu-bsp lab's --view specs: an eye, or aimed at the sun or moon
// ge:keywords lab bsp view parse_view spec sscanf sun moon lens
// ge:skill brush labs
#include "brush/labs/gpu_bsp_views.hpp"

#include <cstdio>

#include "lab/labs/liquid_scene.hpp"

namespace ge::brush::labs {

namespace {

namespace ls = ge::lab::liquid_scene;
namespace gr = ge::render;
constexpr double kDeg = 3.14159265358979323846 / 180;

}  // namespace

bool parse_view(const std::string& spec, const std::vector<std::string>& names, View* out) {
  const size_t colon = spec.find(':');
  if (colon == std::string::npos) return false;
  const std::string map = spec.substr(0, colon);
  double x, y, z, hd = 0, pt = 0;
  char word[8] = {};
  int used = 0;
  // `x,y,z,heading,pitch`, or `x,y,z,sun[,lens]` / `x,y,z,moon[,lens]`: aimed at the body.
  if (std::sscanf(spec.c_str() + colon + 1, "%lf,%lf,%lf,%7[a-z]%n", &x, &y, &z, word, &used) ==
      4) {
    const std::string w = word;
    if (w != "sun" && w != "moon") return false;
    const char* rest = spec.c_str() + colon + 1 + used;
    double lens = 0;
    if (*rest == ',' && std::sscanf(rest + 1, "%lf", &lens) != 1) return false;
    if (*rest != ',' && *rest != '\0') return false;
    out->aimed = true;
    out->body = w == "sun" ? gr::SkyBody::sun : gr::SkyBody::moon;
    out->lens = lens * kDeg;
  } else if (std::sscanf(spec.c_str() + colon + 1, "%lf,%lf,%lf,%lf,%lf", &x, &y, &z, &hd, &pt) !=
             5) {
    return false;
  }
  for (size_t i = 0; i < names.size(); ++i) {
    if (names[i] != map) continue;
    out->map = i;
    out->eye[0] = x;
    out->eye[1] = y;
    out->eye[2] = z;
    out->heading = hd * kDeg;
    out->pitch = pt * kDeg;
    out->id = map + "-at-" + ls::fixed(x, 0) + "-" + ls::fixed(z, 0) +
              (!out->aimed                     ? ""
               : out->body == gr::SkyBody::sun ? "-sun"
                                               : "-moon");
    out->title = map + " at " + ls::fixed(x, 1) + ", " + ls::fixed(y, 1) + ", " + ls::fixed(z, 1);
    out->caption = out->aimed ? std::string("A view picked by hand, aimed at the ") +
                                    (out->body == gr::SkyBody::sun ? "sun." : "moon.")
                              : "A view picked by hand.";
    return true;
  }
  return false;
}

}  // namespace ge::brush::labs
