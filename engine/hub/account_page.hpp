// THE HUB'S ACCOUNT PAGE (docs/accounts.md § The hub's pages): a player signs up, makes a one-time
// login code for a classic client's console, or sets a new password with the recovery code, from
// a browser. It is served only by a hub that runs the account service (`--accounts=<dir>`), and it
// works the service's store directly, in the hub's own process.
//
//   GET  /account            the three forms (or why the page cannot take a password here)
//   POST /account/signup     name, password, again   -> the recovery code, shown once
//   POST /account/code       name, password          -> the code and the line to type
//   POST /account/recover    name, recovery, password -> a new recovery code
//
// A PASSWORD ONLY OVER HTTPS. The hub speaks plain HTTP; an operator's reverse proxy adds TLS and
// says so (`X-Forwarded-Proto: https`). Without that, and not from this machine, the forms are
// shown disabled and a post is refused, so the page never asks for a password in the clear. The
// throttle's address is the proxy's `X-Forwarded-For` when the request came from this machine,
// else the connection's. Every answer is `no-store`; the page keeps no session and sets no
// cookie, so another site's form can make it do nothing a player's own browser could not.
//
// ge:summary The hub's Account page: sign up, a login code, recovery, by form posts over HTTPS
// ge:keywords hub account account-page signup login-code recover form post https
// ge:keywords x-forwarded-proto x-forwarded-for urlencoded account_answer form_fields
// ge:skill net
// ge:test tests/hub_account_page_test.cpp
#pragma once

#include <map>
#include <string>
#include <string_view>

#include "account/service.hpp"
#include "hub/api.hpp"
#include "hub/spec.hpp"

namespace ge::hub {

constexpr size_t kMaxForm = 2048;  // a form post's body: more is refused (413)

struct AccountAsk {
  std::string method;   // GET, HEAD or POST
  std::string path;     // without its query
  std::string body;     // a POST's form, urlencoded
  bool secure = false;  // over HTTPS (the proxy said so) or from this machine
  std::string from;     // the address the throttle counts by
  int64_t unix_us = 0;
};

// True when `path` is the Account page's (/account, /account/...), with `out` its answer.
// `svc` null: this hub keeps no accounts, and the page says so.
bool account_answer(const HubSpec& spec, account::Service* svc, const AccountAsk& ask, Answer& out);

// An urlencoded form's fields ('+' a space, %XX a byte); a field named twice keeps the first.
std::map<std::string, std::string> form_fields(std::string_view body);

}  // namespace ge::hub
