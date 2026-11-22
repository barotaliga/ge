// THE LISTING TAB (Quake II's `com_completion_mode` and `com_completion_treshold`), shared by the
// drop-down console (view.hpp) and a dedicated server's terminal (server/terminal.hpp): the
// command word completed to what every match shares; one match taken whole with a space after it;
// with more, the line echoed as `]<line>` and the matches listed: mode 0 one a line, 1 in columns
// (as many as fit `width` letters, at most 6, each as wide as the longest name and two spaces),
// 2 in columns under a heading for each kind ("<n> possible commands:", "variables", "aliases",
// singular for one). Past `threshold` matches the first Tab only asks ("Press TAB again to display
// all <n> possibilities."); the next on the same line lists them. A leading `/` or `\` is kept.
//
// ge:summary The listing Tab both consoles share: the common prefix, the matches in three forms
// ge:keywords tab completion com_completion_mode com_completion_treshold possibilities columns
// ge:skill console
// ge:test tests/console_view_test.cpp
#pragma once

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

#include "console/console.hpp"

namespace ge::console {

struct TabList {
  std::string input;               // the line after the Tab
  std::string echo;                // `]<line>` when the matches are listed or asked about; "" none
  std::vector<std::string> lines;  // what is printed after the echo
};

// `head` is the command word (the line's first word, its slash off), "" when the cursor is past
// it; `asked` keeps the line the last Tab asked about between calls.
TabList tab_list(const Console& con, std::string_view input, std::string_view head, int mode,
                 int threshold, std::string& asked, size_t width = 78);

}  // namespace ge::console
