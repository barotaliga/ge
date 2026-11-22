// The maps' stills, cached on disk by the game's key, drawn on a worker (stills.hpp).
//
// ge:summary The maps' stills: the cache by key, the worker, the card and still routes
// ge:keywords hub stills cache key worker still_name_ok stills_answer card-route still-route
// ge:skill net
// ge:test tests/hub_card_test.cpp
#include "hub/stills.hpp"

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <system_error>

#include "capture/jpeg.hpp"
#include "hub/api.hpp"
#include "net/address.hpp"

namespace ge::hub {

namespace fs = std::filesystem;
namespace gn = ge::net;

constexpr int kStillQuality = 92;

struct Stills::Entry {
  enum class State : uint8_t { asked, ready, none } state = State::asked;
  std::string key;
  std::shared_ptr<const CardPicture> picture;
  std::vector<uint8_t> file;
};

bool still_name_ok(std::string_view name) {
  if (name.empty() || name.size() > 64 || name.find("..") != std::string_view::npos) return false;
  if (name.front() == '.') return false;
  for (const char c : name) {
    const bool ok = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') ||
                    c == '_' || c == '-' || c == '+' || c == '.';
    if (!ok) return false;
  }
  return true;
}

Stills::Stills(const HubStills& game, StillsConfig config)
    : game_(game), config_(std::move(config)) {}

Stills::~Stills() {
  {
    std::lock_guard<std::mutex> l(mu_);
    stop_ = true;
  }
  wake_.notify_all();
  if (worker_.joinable()) worker_.join();
}

std::shared_ptr<const CardPicture> Stills::still(std::string_view game,
                                                 std::string_view map) const {
  if (!game_.render || !still_name_ok(game) || !still_name_ok(map)) return nullptr;
  const std::string id = std::string(game) + "/" + std::string(map);
  std::lock_guard<std::mutex> l(mu_);
  auto it = entries_.find(id);
  if (it == entries_.end()) {
    entries_.emplace(id, std::make_shared<Entry>());
    queue_.push_back(id);
    if (config_.thread && !worker_.joinable())
      worker_ = std::thread([this] { const_cast<Stills*>(this)->work(); });
    wake_.notify_one();
    return nullptr;
  }
  return it->second->state == Entry::State::ready ? it->second->picture : nullptr;
}

std::vector<uint8_t> Stills::still_file(std::string_view game, std::string_view map) const {
  if (!still(game, map)) return {};
  std::lock_guard<std::mutex> l(mu_);
  auto it = entries_.find(std::string(game) + "/" + std::string(map));
  return it == entries_.end() ? std::vector<uint8_t>() : it->second->file;
}

const CardLook* Stills::look(std::string_view game) const {
  if (!game_.look || !still_name_ok(game)) return nullptr;
  std::lock_guard<std::mutex> l(mu_);
  auto it = looks_.find(std::string(game));
  if (it == looks_.end()) {
    auto look = std::make_unique<CardLook>();
    if (!game_.look(config_.content, game, *look) || look->charset.width < 16) look.reset();
    it = looks_.emplace(std::string(game), std::move(look)).first;
  }
  return it->second.get();
}

int Stills::made() const {
  std::lock_guard<std::mutex> l(mu_);
  return made_;
}

int Stills::refused() const {
  std::lock_guard<std::mutex> l(mu_);
  return refused_;
}

// One still: the kept file when its key matches, else drawn and kept (the old keys deleted).
bool Stills::make(Entry& e, const std::string& game, const std::string& map) {
  const std::string key = game_.key
                              ? game_.key(config_.content, game, map, config_.width, config_.height)
                              : std::string();
  if (key.empty() || !still_name_ok(key)) return false;
  std::error_code ec;
  const fs::path dir = fs::path(config_.cache) / game;
  const fs::path file = dir / (map + "-" + key + ".jpg");
  std::vector<uint8_t> bytes;
  if (std::ifstream in(file, std::ios::binary); in) {
    bytes.assign(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
  }
  std::string error;
  if (!bytes.empty()) {
    if (auto pic = capture::decode_jpeg(bytes, error)) {
      auto out = std::make_shared<CardPicture>();
      out->width = pic->width;
      out->height = pic->height;
      out->rgba = std::move(pic->rgba);
      e.key = key;
      e.picture = std::move(out);
      e.file = std::move(bytes);
      return true;
    }
  }
  CardPicture pic;
  if (!game_.render(config_.content, game, map, config_.width, config_.height, pic, error) ||
      pic.width <= 0 || pic.rgba.size() != size_t(pic.width) * size_t(pic.height) * 4) {
    std::fprintf(stderr, "hub: the still of %s (%s) was not drawn: %s; next: check --content=%s\n",
                 map.c_str(), game.c_str(), error.empty() ? "no picture" : error.c_str(),
                 config_.content.c_str());
    return false;
  }
  bytes = capture::encode_jpeg(pic.width, pic.height, pic.rgba, kStillQuality, false);
  fs::create_directories(dir, ec);
  // The map's older stills (another key: another map file or another renderer) go.
  for (fs::directory_iterator it(dir, ec), end; !ec && it != end; it.increment(ec)) {
    const std::string name = it->path().filename().string();
    if (name.rfind(map + "-", 0) == 0 && name.size() == map.size() + 1 + key.size() + 4 &&
        it->path() != file)
      fs::remove(it->path(), ec);
  }
  const fs::path part = dir / (map + "-" + key + ".part");
  if (std::ofstream out(part, std::ios::binary); out) {
    out.write(reinterpret_cast<const char*>(bytes.data()), std::streamsize(bytes.size()));
    out.close();
    fs::rename(part, file, ec);
  }
  if (ec)
    std::fprintf(stderr, "hub: the still %s was not kept: %s; next: check --stills=%s\n",
                 file.string().c_str(), ec.message().c_str(), config_.cache.c_str());
  e.key = key;
  e.picture = std::make_shared<CardPicture>(std::move(pic));
  e.file = std::move(bytes);
  return true;
}

int Stills::make_waiting() {
  int n = 0;
  for (;;) {
    std::string id;
    std::shared_ptr<Entry> e;
    {
      std::lock_guard<std::mutex> l(mu_);
      if (queue_.empty()) return n;
      id = queue_.front();
      queue_.erase(queue_.begin());
      e = entries_[id];
    }
    const size_t slash = id.find('/');
    Entry made;
    const bool ok = make(made, id.substr(0, slash), id.substr(slash + 1));
    std::lock_guard<std::mutex> l(mu_);
    if (ok) {
      made.state = Entry::State::ready;
      *e = std::move(made);
      made_++;
    } else {
      e->state = Entry::State::none;
      refused_++;
    }
    n++;
  }
}

void Stills::work() {
  for (;;) {
    {
      std::unique_lock<std::mutex> l(mu_);
      wake_.wait(l, [&] { return stop_ || !queue_.empty(); });
      if (stop_) return;
    }
    make_waiting();
  }
}

bool stills_answer(const Poller& p, const HubSpec& spec, const Stills* stills,
                   std::string_view path, Answer& out) {
  const std::string_view kCard = "/card/", kStill = "/still/";
  if (stills == nullptr) return false;
  if (path.rfind(kCard, 0) == 0 && path.size() > kCard.size() + 4 &&
      path.substr(path.size() - 4) == ".jpg") {
    gn::Address a;
    const std::string addr(path.substr(kCard.size(), path.size() - kCard.size() - 4));
    const HubNode* n = gn::parse_address(addr, a) ? poller_find(p, a) : nullptr;
    if (n == nullptr || !n->have) return false;
    const std::string game = spec.stills.game_of ? spec.stills.game_of(n->status) : std::string();
    const CardLook* look = stills->look(game);
    if (look == nullptr) return false;
    const auto still = stills->still(game, n->status.map);
    const CardPicture none;
    const CardPicture card = compose_card(spec, n->status, *look, still ? *still : none,
                                          stills->config().width, stills->config().height);
    out = Answer{};
    out.type = "image/jpeg";
    const std::vector<uint8_t> jpg = card_jpeg(card);
    out.body.assign(jpg.begin(), jpg.end());
    out.max_age_s = 2;  // the board is live; the page asks again by the record's change count
    return true;
  }
  if (path.rfind(kStill, 0) == 0 && path.size() > kStill.size() + 4 &&
      path.substr(path.size() - 4) == ".jpg") {
    const std::string_view rest = path.substr(kStill.size(), path.size() - kStill.size() - 4);
    const size_t slash = rest.find('/');
    if (slash == std::string_view::npos) return false;
    const std::vector<uint8_t> file =
        stills->still_file(rest.substr(0, slash), rest.substr(slash + 1));
    if (file.empty()) return false;
    out = Answer{};
    out.type = "image/jpeg";
    out.body.assign(file.begin(), file.end());
    out.max_age_s = 3600;
    return true;
  }
  return false;
}

}  // namespace ge::hub
