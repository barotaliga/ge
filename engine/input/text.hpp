// THE TEXT FORMATS' SHARED READING: a line split into words (a "quoted phrase" one word, `#` a
// comment to the end of the line), a number read and written exactly (core/num_text's shortest
// round trip), and `key=value` options. The bindings file, the device database and the event
// log are all read with it, so they quote and comment alike.
//
// ge:summary Line words, quoted phrases, comments and exact numbers for the input text formats
// ge:keywords input text parse words quote comment number option key-value split_words
// ge:skill input
// ge:test tests/input_actions_test.cpp
#pragma once

#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace ge::input::text {

// The words of one line; empty for a blank or comment line.
std::vector<std::string> split_words(std::string_view line);
// Each line of `text` with its 1-based number.
std::vector<std::pair<int, std::string_view>> lines(std::string_view text);
std::optional<double> number(std::string_view word);
// Shortest text that reads back to the same double.
std::string num(double v);
// Shortest text that reads back to the same float (0.1f is "0.1", not its double's digits).
std::string num(float v);
// "name=value" split at the first '='; value empty when there is none.
std::pair<std::string_view, std::string_view> option(std::string_view word);
// A word that needs quotes gets them.
std::string quote(std::string_view s);

}  // namespace ge::input::text
