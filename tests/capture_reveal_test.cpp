// A file shown in the system's file manager (engine/capture/reveal.hpp): each desktop's commands in
// the order they are tried, Explorer's raw command line with the quote after the comma, the
// freedesktop bus call with a percent-encoded URI and the folder opened when no file manager
// answers, and file URIs.
//
// ge:summary Tests: the reveal steps for macOS, Windows and Linux, and file URIs
// ge:keywords test reveal show-in-finder explorer dbus xdg-open file-uri
// ge:skill replay platform testing
#include <string>

#include "capture/reveal.hpp"
#include "check.hpp"

namespace gc = ge::capture;

TEST(finder_selects_the_file_and_opens_a_folder) {
  const auto file = gc::reveal_steps(gc::Desktop::mac, "/Users/a/shots/se-1.jpg", false);
  CHECK(file.size() == 1 && file[0].argv.size() == 3);
  CHECK(file[0].argv[0] == "open" && file[0].argv[1] == "-R" &&
        file[0].argv[2] == "/Users/a/shots/se-1.jpg" && !file[0].judged);
  const auto folder = gc::reveal_steps(gc::Desktop::mac, "/Users/a/shots", true);
  CHECK(folder.size() == 1 && folder[0].argv.size() == 2 && folder[0].argv[1] == "/Users/a/shots");
  CHECK(gc::file_manager_name(gc::Desktop::mac) == "FINDER");
}

TEST(explorer_reads_its_select_with_the_quote_after_the_comma) {
  const auto file = gc::reveal_steps(gc::Desktop::windows, "C:/Users/a b/shots/se-1.jpg", false);
  CHECK(file.size() == 1 && file[0].argv[0] == "explorer.exe" && !file[0].judged);
  CHECK_MSG(file[0].line == "explorer.exe /select,\"C:\\Users\\a b\\shots\\se-1.jpg\"",
            file[0].line);
  const auto folder = gc::reveal_steps(gc::Desktop::windows, "C:\\shots", true);
  CHECK(folder[0].line == "explorer.exe \"C:\\shots\"");
  CHECK(gc::file_manager_name(gc::Desktop::windows) == "EXPLORER");
}

TEST(a_linux_desktop_asks_the_file_manager_then_opens_the_folder) {
  const auto steps = gc::reveal_steps(gc::Desktop::freedesktop, "/home/a/my shots/se,1.jpg", false);
  CHECK(steps.size() == 2);
  CHECK(steps[0].argv[0] == "dbus-send" && steps[0].judged);
  CHECK(steps[0].argv[6] == "org.freedesktop.FileManager1.ShowItems");
  CHECK_MSG(steps[0].argv[7] == "array:string:file:///home/a/my%20shots/se%2C1.jpg",
            steps[0].argv[7]);  // a comma would split dbus-send's array
  CHECK(steps[1].argv.size() == 2 && steps[1].argv[0] == "xdg-open" &&
        steps[1].argv[1] == "/home/a/my shots" && !steps[1].judged);
  const auto folder = gc::reveal_steps(gc::Desktop::freedesktop, "/home/a/shots", true);
  CHECK(folder[0].argv[6] == "org.freedesktop.FileManager1.ShowFolders");
  CHECK(folder[1].argv[1] == "/home/a/shots");
  CHECK(gc::reveal_steps(gc::Desktop::freedesktop, "", false).empty());
  CHECK(gc::file_manager_name(gc::Desktop::freedesktop) == "FILES");
}

TEST(a_file_uri_encodes_what_is_not_plain) {
  CHECK(gc::file_uri("/a b/\xc3\xbc.png") == "file:///a%20b/%C3%BC.png");
  CHECK(gc::file_uri("C:\\x\\y.jpg") == "file:///C%3A/x/y.jpg");
}
