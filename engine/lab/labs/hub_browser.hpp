// The hub labs' browser: a page the hub writes drawn by a headless Chromium into a lab picture.
// The browser is GE_CHROMIUM or the container's /opt/pw-browsers/chromium; where there is none
// the labs say so and draw nothing. A browser's pixels differ machine to machine, so a lab hands
// these pictures to `Lab::shot` as measured.
//
// ge:summary Hub labs' headless Chromium: a hub page drawn at a width into a lab picture
// ge:keywords lab hub browser chromium headless screenshot page width dark-mode stylesheet
// ge:skill net labs
#pragma once

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <optional>
#include <string>

#include "hub/pages.hpp"
#include "lab/png.hpp"
#include "lab/raster.hpp"

namespace ge::lab::hub_browser {

// The page with its stylesheet inlined, so a file:// load draws it as the hub serves it.
inline std::string hub_standalone(const std::string& page) {
  const std::string link = "<link rel=\"stylesheet\" href=\"/style.css\">";
  std::string out = page;
  const size_t at = out.find(link);
  if (at != std::string::npos) {
    out.replace(at, link.size(), "<style>" + ge::hub::hub_stylesheet() + "</style>");
  }
  return out;
}

// The browser's path, or "" when there is none.
inline std::string find_browser() {
  if (const char* env = std::getenv("GE_CHROMIUM"); env && *env) return env;
  std::error_code ec;
  if (std::filesystem::exists("/opt/pw-browsers/chromium", ec)) return "/opt/pw-browsers/chromium";
  return "";
}

// One page drawn at width x height (the dark scheme when `dark`) through files in `dir`; empty
// when the browser failed.
inline std::optional<ge::lab::Picture> hub_draw(const std::string& browser,
                                                const std::filesystem::path& dir,
                                                const std::string& name, const std::string& page,
                                                int width, int height, bool dark) {
  const std::filesystem::path html = dir / (name + ".html"), png = dir / (name + ".png");
  {
    std::ofstream f(html, std::ios::binary);
    f << hub_standalone(page);
  }
  const std::string cmd = "\"" + browser +
                          "\" --headless --no-sandbox --disable-gpu --hide-scrollbars " +
                          (dark ? "--force-dark-mode " : "") +
                          "--force-device-scale-factor=1 --window-size=" + std::to_string(width) +
                          "," + std::to_string(height) + " --screenshot=\"" + png.string() +
                          "\" \"file://" + html.string() + "\" > /dev/null 2>&1";
  if (std::system(cmd.c_str()) != 0) return std::nullopt;
  return ge::lab::read_png(png.string());
}

}  // namespace ge::lab::hub_browser
