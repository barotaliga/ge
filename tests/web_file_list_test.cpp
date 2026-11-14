// engine/web/file_list.hpp: a content pack's files by hash, the list both ways and the URLs.
//
// ge:summary Test: the file list the hub writes and the page reads, refusals, the URLs
// ge:keywords test web file-list read_file_list write_file_list pack_list_url map_list_url
// ge:skill net testing
#include "web/file_list.hpp"

#include <string>
#include <vector>

#include "check.hpp"

namespace gw = ge::web;

TEST(a_file_list_reads_back_what_was_written) {
  gw::FileList list;
  list.pack = std::string(64, 'f');
  list.files = {{std::string(64, 'a'), 1234, "maps/e1m1.bsp"},
                {std::string(64, '0'), 0, "sound/a b.wav"}};
  gw::FileList back;
  std::string error;
  CHECK(gw::read_file_list(gw::write_file_list(list), back, error));
  CHECK(back.pack == list.pack && back.files.size() == 2);
  const auto& f = back.files;
  CHECK(f[0].sha == list.files[0].sha && f[0].bytes == 1234 && f[0].name == "maps/e1m1.bsp");
  CHECK(f[1].bytes == 0 && f[1].name == "sound/a b.wav");  // a name keeps its spaces
  CHECK(gw::read_file_list("ge-files 1\r\n", back, error) && back.files.empty() &&
        back.pack.empty());
  CHECK(!gw::read_file_list("ge-files 1\npack 12\n", back, error));
}

TEST(a_damaged_file_list_is_refused_with_its_line) {
  gw::FileList out;
  std::string error;
  CHECK(!gw::read_file_list("", out, error));
  CHECK(!gw::read_file_list("ge-files 2\n", out, error) &&
        error.find("line 1") != std::string::npos);
  const std::string sha(64, 'b');
  CHECK(!gw::read_file_list("ge-files 1\n" + sha + " 12\n", out, error));
  CHECK(error.find("line 2") != std::string::npos);
  CHECK(!gw::read_file_list("ge-files 1\n" + sha + " x1 a\n", out, error));
  CHECK(!gw::read_file_list("ge-files 1\nABC 1 a\n", out, error));
  CHECK(!gw::read_file_list("ge-files 1\n" + sha + " 999999999999 a\n", out, error));
}

TEST(the_urls_a_page_asks_for) {
  const std::string sha(64, 'c');
  CHECK(gw::pack_list_url("/content/" + sha + ".pak.zst") == "/content/" + sha + ".files");
  CHECK(gw::pack_list_url("http://h:1/content/" + sha + ".pak.zst") ==
        "http://h:1/content/" + sha + ".files");
  CHECK(gw::pack_list_url("/viewer/pak0.pak").empty());
  CHECK(gw::pack_list_url("/other/" + sha + ".pak.zst").empty());
  CHECK(gw::pack_list_url(gw::pack_url(sha)) == "/content/" + sha + ".files");
  CHECK(gw::map_list_url("e1m2") == "/content/map/e1m2.files");
  CHECK(gw::file_url(sha) == "/content/f/" + sha + ".zst");
}
