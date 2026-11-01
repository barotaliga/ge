// A demo heard and read in the window (demo_heard.hpp).
//
// ge:summary DemoHearing's run: the sounds made per folder, the frame's sounds, voices and lines
// ge:keywords demo-heard print_lines playdemo sound voice print
// ge:skill shooter audio
// ge:test tests/shooters_edge_demo_test.cpp
#include "shooters_edge/client/demo_heard.hpp"

namespace shooters_edge::client {

namespace {

// The entity the listener's own sounds come from: the view entity, or the player the view rides.
int listener_of(const DemoState& s) {
  return s.format == DemoFormat::single ? s.view_entity : s.player + 1;
}

}  // namespace

std::vector<std::string> print_lines(const std::string& text) {
  std::vector<std::string> out;
  std::string line;
  const auto flush = [&] {
    if (line.find_first_not_of(' ') != std::string::npos) out.push_back(line);
    line.clear();
  };
  for (const char raw : text) {
    const int c = static_cast<unsigned char>(raw) & 0x7f;
    if (c == '\n') {
      flush();
    } else if (c == 16 || c == 17) {
      line += c == 16 ? '[' : ']';
    } else if (c >= 18 && c <= 27) {
      line += static_cast<char>('0' + (c - 18));
    } else if (c >= 32 && c < 127) {
      line += static_cast<char>(c);
    }
  }
  flush();
  // The colour marks players' own clients write into chat: `&c` and three hex digits a colour,
  // `&r` back to plain, braces round a word in the second colour. Shown as the plain words.
  const auto hex = [](char c) {
    return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
  };
  for (std::string& l : out) {
    std::string plain;
    for (size_t i = 0; i < l.size(); i++) {
      if (l[i] == '&' && i + 4 < l.size() && l[i + 1] == 'c' && hex(l[i + 2]) && hex(l[i + 3]) &&
          hex(l[i + 4])) {
        i += 4;
      } else if (l[i] == '&' && i + 1 < l.size() && l[i + 1] == 'r') {
        i += 1;
      } else if (l[i] != '{' && l[i] != '}') {
        plain += l[i];
      }
    }
    l = plain;
  }
  std::erase_if(out,
                [](const std::string& l) { return l.find_first_not_of(' ') == std::string::npos; });
  return out;
}

DemoHearing::DemoHearing(ge::audio::Synth& synth) : synth_(synth) {}

void DemoHearing::level(const DemoState& s, const SearchPath& files, const ge::brush::Map* map) {
  if (!sounds_ || files_ != &files) {
    if (sounds_) sounds_->silence();
    sounds_ = std::make_unique<DemoSounds>(synth_, files);
    files_ = &files;
  }
  if (!voices_) voices_ = std::make_unique<DemoVoices>(synth_);
  sounds_->level(s, map);
}

bool DemoHearing::frame(const DemoState& s, DemoHeard& h, float dt, std::vector<std::string>& lines,
                        std::string& centre) {
  if (sounds_) sounds_->frame(s, h.started, h.stopped, listener_of(s), dt);
  if (voices_)
    for (const DemoVoice& v : h.voice) voices_->play(v);
  for (const DemoPrint& p : h.prints)
    for (std::string& line : print_lines(p.text)) lines.push_back(std::move(line));
  const bool centred = !h.centre.empty();
  if (centred) {  // the centre is one line: a message's lines side by side
    centre.clear();
    for (const std::string& line : print_lines(h.centre))
      centre += (centre.empty() ? "" : "  ") + line;
  }
  h.clear();
  return centred;
}

void DemoHearing::stop() {
  if (sounds_) sounds_->silence();
  sounds_.reset();
  voices_.reset();
  files_ = nullptr;
}

}  // namespace shooters_edge::client
