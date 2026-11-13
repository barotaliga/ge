// engine/hub/account_page.hpp and its posts through engine/hub/http.hpp: the forms are shown,
// disabled where the connection is not HTTPS; a sign-up shows its recovery code once, a code post
// shows the line a classic client types, recovery sets a new password; a post over plain HTTP,
// two passwords that differ and a wrong password are refused in words; a hub with no accounts
// says so; a form is read with '+' and %XX; the request head's length and proxy headers parse.
//
// ge:summary Test: the hub's Account page: forms, sign-up, code, recovery, the HTTPS rule
// ge:keywords test hub account account-page form post signup code recover https proxy
// ge:skill net testing
#include "hub/account_page.hpp"

#include <string>

#include "check.hpp"
#include "hub/http.hpp"

namespace ga = ge::account;
using namespace ge::hub;

namespace {

constexpr int64_t kNow = 1794500000ll * 1000000;

HubSpec spec() {
  HubSpec s;
  s.game = "Test Game";
  s.id = "test_game";
  return s;
}

void counter_fresh(void* ctx, uint8_t* out, size_t size) {
  auto* n = static_cast<uint64_t*>(ctx);
  for (size_t i = 0; i < size; i++) out[i] = static_cast<uint8_t>((*n * 131 + i * 17) >> 1);
  (*n)++;
}

struct Svc {
  ge::net::Transport port;  // never opened: the page works the store directly
  ga::Service svc;
  uint64_t n = 1;
  Svc() {
    ga::Store store;
    ga::store_init(store, ga::HashCost{8, 1});
    ga::ServiceConfig c;
    c.seed.fill(3);
    ga::service_init(svc, c, std::move(store), port, counter_fresh, &n);
  }
};

Answer post(Svc& s, const std::string& path, const std::string& body, bool secure = true,
            const std::string& from = "198.51.100.7:51000") {
  AccountAsk ask{"POST", path, body, secure, from, kNow};
  Answer a;
  CHECK(account_answer(spec(), &s.svc, ask, a));
  return a;
}

bool has(const Answer& a, const std::string& text) {
  return a.body.find(text) != std::string::npos;
}

}  // namespace

TEST(account_page_shows_forms_and_disables_them_without_https) {
  Svc s;
  Answer a;
  CHECK(account_answer(spec(), &s.svc, {"GET", "/account", "", true, "x", kNow}, a));
  CHECK(a.code == 200 && has(a, "action=\"/account/signup\"") && !has(a, " disabled"));
  CHECK(a.max_age_s == 0);
  CHECK(account_answer(spec(), &s.svc, {"GET", "/account", "", false, "x", kNow}, a));
  CHECK(has(a, " disabled") && has(a, "only over HTTPS"));
  CHECK(!account_answer(spec(), &s.svc, {"GET", "/accounts-of-others", "", true, "x", kNow}, a));
  CHECK(account_answer(spec(), nullptr, {"GET", "/account", "", true, "x", kNow}, a));
  CHECK(a.code == 404 && has(a, "keeps no accounts"));
}

TEST(account_page_signs_up_makes_a_code_and_recovers) {
  Svc s;
  Answer a = post(s, "/account/signup", "name=Ranger&password=long+enough&again=long+enough");
  CHECK_MSG(a.code == 200 && has(a, "Welcome, Ranger"), a.body);
  const size_t at = a.body.find("class=\"code\">") + 13;
  const std::string recovery = a.body.substr(at, 19);
  CHECK(ga::find_by_name(s.svc.store, "ranger"));

  a = post(s, "/account/code", "name=ranger&password=long%20enough");
  CHECK_MSG(a.code == 200 && has(a, "<code>login Ranger "), a.body);

  a = post(s, "/account/code", "name=ranger&password=not+the+one");
  CHECK(a.code == 400 && has(a, "class=\"warn\""));

  // The wrong password held the address and the account for a second: another player's address,
  // a second later.
  AccountAsk later{
      "POST", "/account/recover",   "name=Ranger&recovery=" + recovery + "&password=brand+new+one",
      true,   "198.51.100.8:51000", kNow + 2000000};
  CHECK(account_answer(spec(), &s.svc, later, a));
  CHECK_MSG(a.code == 200 && has(a, "Password set"), a.body);
  CHECK(ga::sign_in(s.svc.store, "Ranger", "brand new one", "y", kNow + 3600ll * 1000000).refusal ==
        ga::Refusal::none);
}

TEST(account_page_refuses_plain_http_and_unlike_passwords) {
  Svc s;
  Answer a =
      post(s, "/account/signup", "name=Ranger&password=long+enough&again=long+enough", false);
  CHECK(a.code == 403 && s.svc.store.accounts.empty());
  a = post(s, "/account/signup", "name=Ranger&password=long+enough&again=other+one");
  CHECK(a.code == 400 && has(a, "not the same") && s.svc.store.accounts.empty());
}

TEST(account_form_fields_decode) {
  const auto f = form_fields("a=b+c&d=%41%2b&a=second&e");
  CHECK(f.at("a") == "b c" && f.at("d") == "A+" && f.at("e").empty());
}

TEST(http_parse_reads_a_post_head) {
  HttpRequest r;
  const std::string head =
      "POST /account/code HTTP/1.1\r\nHost: hub\r\ncontent-length: 27\r\n"
      "X-Forwarded-Proto: https\r\nX-Forwarded-For: 203.0.113.9, 10.0.0.1\r\n\r\n";
  CHECK(http_parse(head, r) == HttpParse::ok);
  CHECK(r.method == "POST" && r.content_length == 27 && r.forwarded_https);
  CHECK(r.forwarded_for == "203.0.113.9" && r.head_size == head.size());
  CHECK(http_parse("POST / HTTP/1.1\r\nContent-Length: 1x\r\n\r\n", r) == HttpParse::bad);
}
