// engine/cli/source.hpp's scraping, held on short pages written in each source's shape (no
// network):
//
//   * KENNEY: a listing's pack links without its category, series, tag and page links; the zip a
//     pack's page offers.
//   * OPENGAMEART: a search's results in order with their titles' entities decoded; a pack's
//     title, author, licences and files, and no file outside the files field.
//   * POLY HAVEN: every word matched across id, name and tags; the 1k glTF and its includes.
//
// ge:summary Tests for ge source: Kenney, OpenGameArt and Poly Haven pages and answers parsed
// ge:keywords test cli source kenney opengameart polyhaven scrape search fetch
// ge:skill agent-interface assets testing
#include <string>

#include "check.hpp"
#include "cli/source.hpp"

namespace c = ge::cli;

TEST(kenneys_listing_and_pack_page) {
  const std::string listing =
      "<a href='https://kenney.nl/assets/impact-sounds'>Impact</a>"
      "<a href='https://kenney.nl/assets/category:Audio'>Audio</a>"
      "<a href='https://kenney.nl/assets/series:VFX'>VFX</a>"
      "<a href='https://kenney.nl/assets/page:2'>2</a>"
      "<a href='https://kenney.nl/assets/blaster-kit'>x</a>"
      "<a href='https://kenney.nl/assets/impact-sounds'>again</a>";
  const auto slugs = c::kenney_slugs(listing);
  CHECK(slugs.size() == 2 && slugs[0] == "impact-sounds" && slugs[1] == "blaster-kit");
  const std::string page =
      "<img src=\"https://kenney.nl/media/pages/assets/blaster-kit/9f/preview.png\">"
      "<a href=\"https://kenney.nl/media/pages/assets/blaster-kit/1a2b/kenney_blaster-kit.zip\">"
      "Download</a>";
  CHECK(c::kenney_zip(page, "blaster-kit") ==
        "https://kenney.nl/media/pages/assets/blaster-kit/1a2b/kenney_blaster-kit.zip");
  CHECK(c::kenney_zip(page, "space-kit").empty());
}

TEST(opengameart_search_and_pack_page) {
  const std::string search =
      "<a href=\"/content/faq\">FAQ</a>"
      "<span class=\"art-preview-title\"><a href=\"/content/door-set\">Door &amp; Lock Set</a>"
      "</span><span class=\"art-preview-title\"><a href=\"/content/iron-door\">Iron Door</a>";
  const auto hits = c::oga_results(search);
  CHECK(hits.size() == 2);
  CHECK(hits[0].id == "oga:door-set" && hits[0].title == "Door & Lock Set");
  CHECK(hits[1].id == "oga:iron-door");

  const std::string page =
      "<title>Iron Door | OpenGameArt.org</title>"
      "<div class=\"field field-name-author-submitter\"><a href=\"/users/someone\">someone</a>"
      "</div><div class=\"field field-name-field-art-licenses\"><div class='license-name'>CC0"
      "</div><div class='license-name'>CC-BY 4.0</div></div>"
      "<div class=\"field field-name-field-art-files\"><a href=\"https://opengameart.org/sites/"
      "default/files/iron_door.ogg\" type=\"audio/ogg\">iron_door.ogg</a>"
      "<a href=\"https://opengameart.org/sites/default/files/more.zip\">more.zip</a></div>"
      "<div class=\"field field-name-field-art-tags\"><a href=\"https://opengameart.org/sites/"
      "default/files/css/x.css\">no</a></div>";
  const c::OgaPage p = c::oga_page(page);
  CHECK(p.title == "Iron Door" && p.author == "someone");
  CHECK(p.licences.size() == 2 && p.licences[0] == "CC0" && p.licences[1] == "CC-BY 4.0");
  CHECK_MSG(p.files.size() == 2 && p.files[1].ends_with("/more.zip"),
            std::to_string(p.files.size()));
}

TEST(poly_havens_index_and_files) {
  const std::string index = R"({"barrel_01":{"name":"Barrel 01","tags":["wood","container"]},)"
                            R"("crate_02":{"name":"Wooden Crate 02","tags":["box","wood"]},)"
                            R"("lamp":{"name":"Lamp","tags":["metal"]}})";
  const auto wood = c::polyhaven_hits(index, {"wood"});
  CHECK(wood.size() == 2 && wood[0].id == "polyhaven:barrel_01");
  const auto crate = c::polyhaven_hits(index, {"Wood", "crate"});
  CHECK(crate.size() == 1 && crate[0].title == "Wooden Crate 02");
  const std::string files =
      R"({"gltf":{"1k":{"gltf":{"url":"https://dl.example/1k/crate_02.gltf","include":{)"
      R"("crate_02.bin":{"url":"https://dl.example/crate_02.bin"},)"
      R"("textures/crate_02_diff_1k.jpg":{"url":"https://dl.example/diff.jpg"}}}}}})";
  const auto f = c::polyhaven_files(files);
  CHECK(f.size() == 3 && f[0].first == "crate_02.gltf" &&
        f[2].first == "textures/crate_02_diff_1k.jpg");
  CHECK(c::polyhaven_files(R"({"gltf":{"2k":{}}})").empty());
}
