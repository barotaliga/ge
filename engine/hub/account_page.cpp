// The hub's Account page (hub/account_page.hpp): the forms and the answers to their posts.
//
// ge:summary The hub's Account page defined: the forms, the posts, the HTTPS rule
// ge:keywords hub account page form signup code recover https html
// ge:skill net
// ge:test tests/hub_account_page_test.cpp
#include "hub/account_page.hpp"

#include "hub/page_parts.hpp"
#include "hub/pages.hpp"

namespace ge::hub {

namespace ga = ge::account;

namespace {

int hex_digit(char c) {
  if (c >= '0' && c <= '9') return c - '0';
  if (c >= 'a' && c <= 'f') return c - 'a' + 10;
  if (c >= 'A' && c <= 'F') return c - 'A' + 10;
  return -1;
}

std::string form_decode(std::string_view s) {
  std::string out;
  for (size_t i = 0; i < s.size(); i++) {
    if (s[i] == '+') {
      out += ' ';
    } else if (s[i] == '%' && i + 2 < s.size() && hex_digit(s[i + 1]) >= 0 &&
               hex_digit(s[i + 2]) >= 0) {
      out += static_cast<char>(hex_digit(s[i + 1]) * 16 + hex_digit(s[i + 2]));
      i += 2;
    } else {
      out += s[i];
    }
  }
  return out;
}

std::string page(const HubSpec& spec, const std::string& title, const std::string& main) {
  return head(spec, title, false) + nav(spec, "") + "<main class=\"acct\">" + main +
         "</main><footer>A password typed here goes to this hub's account service only, and only "
         "over HTTPS. The hub keeps no cookie and no session.</footer>\n</body></html>\n";
}

Answer done(int code, std::string body) {
  return html(code, std::move(body), 0);  // no-store: a code or a recovery code is never cached
}

std::string field(const char* label, const char* name, const char* type, bool on,
                  const char* extra = "") {
  return std::string("<label>") + label + " <input name=\"" + name + "\" type=\"" + type + "\"" +
         (on ? "" : " disabled") + " required " + extra + "></label>";
}

std::string forms(const HubSpec& spec, bool secure) {
  std::string m = "<h2>Your account</h2>";
  m += "<p>One account is your name on every server of " + html_escape(spec.game) +
       " that uses this hub, in every game it hosts, and your stats on its pages.</p>";
  if (!secure) {
    m += "<p class=\"warn\">This page takes passwords only over HTTPS, and this connection is "
         "not. Ask the hub's owner to serve it through HTTPS, or use the game's command line: "
         "<code>account signup</code>.</p>";
  }
  const char* name_extra = "minlength=\"2\" maxlength=\"15\" autocomplete=\"username\"";
  const char* pass_extra = "minlength=\"8\" maxlength=\"128\" autocomplete=\"current-password\"";
  const char* new_extra = "minlength=\"8\" maxlength=\"128\" autocomplete=\"new-password\"";
  const char* off = secure ? "" : " disabled";
  m += "<section><h3>Sign up</h3><form method=\"post\" action=\"/account/signup\">" +
       field("Name", "name", "text", secure, name_extra) +
       field("Password", "password", "password", secure, new_extra) +
       field("Again", "again", "password", secure, new_extra) + "<button" + off +
       ">Sign up</button></form><p>2 to 15 letters, digits, dashes, dots or underscores. A name "
       "spelled alike (capitals, a digit for a letter) is the same name.</p></section>";
  m += "<section><h3>A login code</h3><form method=\"post\" action=\"/account/code\">" +
       field("Name", "name", "text", secure, name_extra) +
       field("Password", "password", "password", secure, pass_extra) + "<button" + off +
       ">Get a code</button></form><p>For a classic client: type <code>login &lt;name&gt; "
       "&lt;code&gt;</code> in its console. A code works once, within 15 minutes.</p></section>";
  m += "<section><h3>Forgot your password?</h3><form method=\"post\" "
       "action=\"/account/recover\">" +
       field("Name", "name", "text", secure, name_extra) +
       field("Recovery code", "recovery", "text", secure, "autocomplete=\"off\"") +
       field("New password", "password", "password", secure, new_extra) + "<button" + off +
       ">Set it</button></form><p>The recovery code was shown when you signed up.</p></section>";
  m += "<section><h3>A duo for the two-on-two ladder</h3><form method=\"post\" "
       "action=\"/account/duo\">" +
       field("Name", "name", "text", secure, name_extra) +
       field("Password", "password", "password", secure, pass_extra) +
       field("Partner's name", "partner", "text", secure, "maxlength=\"15\"") +
       field("Duo's name", "duo", "text", secure, "maxlength=\"24\"") + "<button" + off +
       ">Ask</button></form><p>Your partner asks for you under the same duo name; then your "
       "two-on-two matches together count on the <a href=\"/ladder\">ladder</a>.</p></section>";
  return m;
}

std::string refused(const HubSpec& spec, const std::string& words) {
  return page(spec, "Account",
              "<h2>Your account</h2><p class=\"warn\">" + html_escape(words) +
                  "</p><p><a href=\"/account\">Back</a></p>");
}

}  // namespace

std::map<std::string, std::string> form_fields(std::string_view body) {
  std::map<std::string, std::string> out;
  while (!body.empty()) {
    const size_t amp = body.find('&');
    const std::string_view pair = body.substr(0, amp);
    const size_t eq = pair.find('=');
    const std::string key = form_decode(pair.substr(0, eq));
    if (!key.empty() && out.find(key) == out.end()) {
      out[key] = eq == std::string_view::npos ? "" : form_decode(pair.substr(eq + 1));
    }
    if (amp == std::string_view::npos) break;
    body.remove_prefix(amp + 1);
  }
  return out;
}

bool account_answer(const HubSpec& spec, account::Service* svc, const AccountAsk& ask,
                    Answer& out) {
  std::string path = ask.path;
  while (path.size() > 1 && path.back() == '/') path.pop_back();
  if (path != "/account" && path.rfind("/account/", 0) != 0) return false;
  if (!svc) {
    out = done(404, page(spec, "Account",
                         "<h2>Your account</h2><p>This hub keeps no accounts. Accounts are kept "
                         "by the hub or server its owner named for them.</p>"));
    return true;
  }
  if (ask.method != "POST") {
    if (path != "/account") {
      out = done(404, refused(spec, "Nothing is at " + path + "."));
      return true;
    }
    out = done(200, page(spec, "Account", forms(spec, ask.secure)));
    return true;
  }
  if (!ask.secure) {
    out = done(403, refused(spec,
                            "This page takes passwords only over HTTPS, and this connection "
                            "is not. Nothing was sent on."));
    return true;
  }
  std::map<std::string, std::string> f = form_fields(ask.body);
  ga::Fresh fresh{};
  struct Wipe {  // the form's secrets and the fresh bytes overwritten however the answer ends
    std::map<std::string, std::string>& f;
    ga::Fresh& fresh;
    ~Wipe() {
      for (auto& [k, v] : f) ge::net::wipe(v.data(), v.size());
      ge::net::wipe(fresh.data(), fresh.size());
    }
  } wipe{f, fresh};
  const std::string name = f["name"];
  ga::Store& s = svc->store;
  if (path == "/account/signup") {
    if (f["password"] != f["again"]) {
      out = done(400, refused(spec, "The two passwords are not the same."));
      return true;
    }
    svc->fresh(svc->fresh_ctx, fresh.data(), fresh.size());
    const ga::Answer a = ga::create(s, name, f["password"], ask.unix_us, fresh);
    if (a.refusal != ga::Refusal::none) {
      out = done(400, refused(spec, a.words));
      return true;
    }
    out = done(200, page(spec, "Account",
                         "<h2>Welcome, " + html_escape(name) +
                             "</h2><p>Your account is made. Your recovery code, shown this "
                             "once:</p><p class=\"code\">" +
                             html_escape(a.code) +
                             "</p><p>Keep it somewhere safe: it sets a new password if you "
                             "forget yours. Next, <a href=\"/account\">get a login code</a>.</p>"));
  } else if (path == "/account/code") {
    ga::Answer a = ga::sign_in(s, name, f["password"], ask.from, ask.unix_us);
    if (a.refusal == ga::Refusal::none) {
      svc->fresh(svc->fresh_ctx, fresh.data(), fresh.size());
      a = ga::make_code(s, a.id, ask.unix_us, fresh);
    }
    if (a.refusal != ga::Refusal::none) {
      out = done(400, refused(spec, a.words));
      return true;
    }
    const ga::Account* acc = ga::find(s, a.id);
    const std::string shown = acc ? acc->name : name;
    out = done(200, page(spec, "Account",
                         "<h2>Your login code</h2><p class=\"code\">" + html_escape(a.code) +
                             "</p><p>In a classic client's console type:</p><p><code>login " +
                             html_escape(shown) + " " + html_escape(a.code) +
                             "</code></p><p>It works once, within 15 minutes.</p>"));
  } else if (path == "/account/recover") {
    svc->fresh(svc->fresh_ctx, fresh.data(), fresh.size());
    const ga::Answer a =
        ga::recover(s, name, f["recovery"], f["password"], ask.from, ask.unix_us, fresh);
    if (a.refusal != ga::Refusal::none) {
      out = done(400, refused(spec, a.words));
      return true;
    }
    out = done(200, page(spec, "Account",
                         "<h2>Password set</h2><p>Your new recovery code, shown this once:</p>"
                         "<p class=\"code\">" +
                             html_escape(a.code) + "</p>"));
  } else if (path == "/account/duo") {
    const ga::Answer a = ga::sign_in(s, name, f["password"], ask.from, ask.unix_us);
    if (a.refusal != ga::Refusal::none) {
      out = done(400, refused(spec, a.words));
      return true;
    }
    const ga::Account* partner = ga::find_by_name(s, f["partner"]);
    if (!partner || partner->removed_s != 0) {
      out = done(400, refused(spec, "No player called " + f["partner"] + " has an account here."));
      return true;
    }
    const ga::DuoAnswer d = ga::matches_duo_ask(svc->matches, a.id, partner->id, f["duo"]);
    const bool ok =
        d == ga::DuoAnswer::asked || d == ga::DuoAnswer::made || d == ga::DuoAnswer::already;
    out = done(ok ? 200 : 400, ok ? page(spec, "Account",
                                         "<h2>" + html_escape(f["duo"]) + "</h2><p>" +
                                             html_escape(ga::duo_answer_words(d)) +
                                             "</p><p><a href=\"/ladder\">The ladder</a></p>")
                                  : refused(spec, ga::duo_answer_words(d)));
  } else {
    out = done(404, refused(spec, "Nothing is at " + path + "."));
  }
  return true;
}

}  // namespace ge::hub
