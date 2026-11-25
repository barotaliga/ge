// plugins/classic/protocol/addr_list.hpp: a classic server's address list. Entries are added with a
// mask and a comment, refused in the server's words when malformed or already there, matched by
// their mask with each hit counted, removed by address, row or all, and listed with their hits.
//
// ge:summary Test: the classic address list's words, masks, hits and listing
// ge:keywords test address-list mask hits addgtvhost addgtvban ipv6-mask
// ge:skill classic net testing
#include "classic/protocol/addr_list.hpp"

#include <string>

#include "check.hpp"

namespace {

namespace cl = ge::classic;
namespace gn = ge::net;

TEST(entries_are_added_and_refused_in_the_servers_words) {
  cl::AddrList l;
  std::string out;
  cl::addr_list_add(l, "addgtvhost", "", out);
  CHECK(out == "Usage: addgtvhost <address[/mask]> [comment]\n");
  cl::addr_list_add(l, "addgtvhost", "10.0.4.7/22 the office", out);
  CHECK(out.empty());
  CHECK(l.entries.size() == 1 && l.entries[0].comment == "the office" && l.entries[0].bits == 22);
  cl::addr_list_add(l, "addgtvhost", "10.0.5.1/22", out);
  CHECK(out == "Entry 10.0.4.0/22 already exists.\n");
  cl::addr_list_add(l, "addgtvhost", "10.0.0.1/", out);
  CHECK(out == "Please specify a mask after '/'.\n");
  cl::addr_list_add(l, "addgtvhost", "10.0.0.1/33", out);
  CHECK(out == "Bad mask: 33 bits\n");
  cl::addr_list_add(l, "addgtvhost", "nowhere", out);
  CHECK(out == "Bad address: nowhere\n");
  cl::addr_list_add(l, "addgtvhost", "2001:db8::1/48", out);
  CHECK(out.empty());
  CHECK(l.entries.size() == 2);
}

TEST(a_match_counts_a_hit_by_the_mask) {
  cl::AddrList l;
  std::string out;
  cl::addr_list_add(l, "addgtvban", "192.168.1.0/24", out);
  CHECK(cl::addr_list_hit(l, gn::ipv4(192, 168, 1, 99, 5000), 1000) != nullptr);
  CHECK(cl::addr_list_hit(l, gn::ipv4(192, 168, 2, 1, 5000), 1000) == nullptr);
  CHECK(l.entries[0].hits == 1 && l.entries[0].last_hit == 1000);
  const std::string t = cl::addr_list_text(l);
  CHECK(t.rfind("id address/mask       hits last hit     comment\n", 0) == 0);
  CHECK(t.find("1  192.168.1.0/24     1    ") != std::string::npos);
}

TEST(entries_are_removed_by_address_row_or_all) {
  cl::AddrList l;
  std::string out;
  cl::addr_list_del(l, "delgtvban", "1", out);
  CHECK(out == "Address list is empty.\n");
  CHECK(cl::addr_list_text(l) == "Address list is empty.\n");
  for (const char* a : {"1.2.3.4", "5.6.7.8", "9.9.9.9"}) cl::addr_list_add(l, "addgtvban", a, out);
  cl::addr_list_del(l, "delgtvban", "4", out);
  CHECK(out == "No such index: 4\n");
  cl::addr_list_del(l, "delgtvban", "2", out);
  CHECK(out.empty() && l.entries.size() == 2);
  cl::addr_list_del(l, "delgtvban", "5.6.7.8", out);
  CHECK(out == "No such entry: 5.6.7.8\n");
  cl::addr_list_del(l, "delgtvban", "9.9.9.9", out);
  CHECK(out.empty() && l.entries.size() == 1);
  cl::addr_list_del(l, "delgtvban", "all", out);
  CHECK(l.entries.empty());
  cl::addr_list_del(l, "delgtvban", "", out);
  CHECK(out == "Usage: delgtvban <address[/mask]|id|all>\n");
}

}  // namespace
