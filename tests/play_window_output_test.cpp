// A game window's output (engine/play/window_output.hpp): every line on the terminal and, with a
// log file named, appended to it as a dump writes it and flushed, so a run is read from the file.
//
// ge:summary Test: a window's output lines reach the log file named, flushed as written
// ge:keywords test log-file logfile window output sink
// ge:skill agent-interface testing
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

#include "check.hpp"
#include "core/output.hpp"
#include "play/window_output.hpp"

namespace {

std::string read_all(const std::filesystem::path& p) {
  std::ifstream in(p);
  std::ostringstream ss;
  ss << in.rdbuf();
  return ss.str();
}

}  // namespace

TEST(a_window_log_file_takes_every_line_as_it_is_written) {
  const std::filesystem::path dir = std::filesystem::temp_directory_path() / "ge_window_output";
  std::error_code ec;
  std::filesystem::remove_all(dir, ec);
  const std::filesystem::path file = dir / "logs" / "run.log";
  std::string error;
  CHECK_MSG(ge::play::attach_window_output(file.string(), error), error);
  CHECK(ge::play::window_log_file() == file.string());
  ge::core::output::status("console: framestats: 59.2 fps");
  ge::core::output::warn("a warning a run left");
  const std::string text = read_all(file);  // read before detaching: flushed line by line
  CHECK_MSG(text.find("console: framestats: 59.2 fps") != std::string::npos, text);
  CHECK_MSG(text.find("a warning a run left") != std::string::npos, text);
  CHECK(ge::play::attach_window_output("", error));  // the terminal alone: the file closed
  CHECK(ge::play::window_log_file().empty());
  ge::core::output::status("after the file closed");
  CHECK(read_all(file).find("after the file closed") == std::string::npos);
  CHECK(!ge::play::attach_window_output((dir / "logs").string(), error));  // a folder
  CHECK_MSG(error.find("cannot be opened") != std::string::npos, error);
  ge::core::output::set_output_sink(nullptr);
  std::filesystem::remove_all(dir, ec);
}
