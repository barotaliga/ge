// Tests: the engine-side layouts by program form (plugins/progs/layout.hpp): the single-player
// table at its offsets, both tables resolved by name, the form picked by the header's crc and
// refused when the game does not provide it, the compiler stamping the form a program declares,
// and the VM's STATE writing through the form's offsets.
//
// ge:summary Tests: the multiplayer and single-player layout tables, chosen by crc
// ge:keywords test qc progs layout single-player multiplayer crc form layout_for_crc every_form
// ge:keywords punchangle idealpitch deathmatch coop teamplay newmis lastruntime
// ge:skill progs testing
#include <string>
#include <vector>

#include "check.hpp"
#include "progs/assembler.hpp"
#include "progs/file.hpp"
#include "progs/layout.hpp"
#include "progs/qc_compile.hpp"
#include "progs/vm.hpp"

using namespace ge::progs;

namespace {

bool has(const std::string& text, const char* part) { return text.find(part) != std::string::npos; }

// The engine-side declarations of `form` as QC source, in its order (function globals with empty
// bodies), so a program built from it carries exactly that layout.
std::string declarations(const Layout& form) {
  auto type = [](uint16_t t) -> std::string {
    switch (t) {
      case kTypeString:
        return "string";
      case kTypeVector:
        return "vector";
      case kTypeEntity:
        return "entity";
      case kTypeFunction:
        return "void()";
      default:
        return "float";
    }
  };
  std::string out;
  for (int i = 0; i < form.num_globals; i++)
    out += type(form.globals[i].type) + " " + form.globals[i].name + ";\n";
  for (int i = 0; i < form.num_fields; i++)
    out += "." + type(form.fields[i].type) + " " + form.fields[i].name + ";\n";
  for (int i = 0; i < form.num_globals; i++)
    if (form.globals[i].type == kTypeFunction)
      out += std::string("void() ") + form.globals[i].name + " = {};\n";
  return out;
}

}  // namespace

TEST(the_single_player_table_sits_at_its_offsets) {
  const Layout& sp = kSinglePlayerLayout;
  CHECK(sp.crc == 5927 && sp.single_player);
  CHECK(sp.globals_end == 92 && sp.fields_end == 105);
  CHECK(sp.g.self == 28 && sp.g.time == 31 && sp.g.force_retouch == 33 && sp.g.mapname == 34);
  CHECK(sp.g.deathmatch == 35 && sp.g.coop == 36 && sp.g.teamplay == 37);
  CHECK(sp.g.serverflags == 38 && sp.g.killed_monsters == 42 && sp.g.parm1 == 43);
  CHECK(sp.g.v_forward == 59 && sp.g.trace_ent == 78 && sp.g.msg_entity == 81);
  CHECK(sp.g.main == 82 && sp.g.set_change_parms == 91);
  CHECK(sp.g.newmis == -1 && sp.f.lastruntime == -1);
  CHECK(sp.f.movetype == 8 && sp.f.origin == 10 && sp.f.avelocity == 22);
  CHECK(sp.f.punchangle == 25 && sp.f.classname == 28 && sp.f.nextthink == 46);
  CHECK(sp.f.v_angle == 70 && sp.f.idealpitch == 73 && sp.f.netname == 74);
  CHECK(sp.f.noise3 == 104);
  CHECK(layout_field(sp, "punchangle")->type == kTypeVector);
  CHECK(layout_global(sp, "newmis") == nullptr);
}

TEST(the_multiplayer_table_resolves_to_its_constants) {
  const Layout& mp = kMultiplayerLayout;
  CHECK(mp.crc == kClassicCrc && !mp.single_player);
  CHECK(mp.g.newmis == kGNewmis && mp.g.parm1 == kGParm1 &&
        mp.g.set_change_parms == kGSetChangeParms);
  CHECK(mp.g.deathmatch == -1 && mp.g.coop == -1 && mp.g.teamplay == -1);
  CHECK(mp.f.origin == kFOrigin && mp.f.classname == kFClassname && mp.f.owner == kFOwner);
  CHECK(mp.f.lastruntime == 8 && mp.f.punchangle == -1 && mp.f.idealpitch == -1);
  CHECK(layout_for_crc(54730) == &kMultiplayerLayout);
  CHECK(layout_for_crc(5927) == &kSinglePlayerLayout);
  CHECK(layout_for_crc(12345) == nullptr);
}

TEST(the_crc_picks_the_form_among_those_the_game_provides) {
  Assembler sp(kSinglePlayerLayout);
  const std::vector<uint8_t> b = sp.bytes();
  Progs p;
  std::string error;
  CHECK(!load_progs(b.data(), b.size(), p, error));
  CHECK_MSG(has(error, "crc is 5927 (the single-player form)") && has(error, "54730 (multiplayer)"),
            error);
  CHECK_MSG(load_progs(b.data(), b.size(), p, error, every_form()), error);
  CHECK(p.layout == &kSinglePlayerLayout && p.entityfields == 105);
  Assembler mp;
  const std::vector<uint8_t> m = mp.bytes();
  CHECK_MSG(load_progs(m.data(), m.size(), p, error, every_form()), error);
  CHECK(p.layout == &kMultiplayerLayout);
}

TEST(a_program_is_held_to_the_table_its_crc_names) {
  // A single-player header over the multiplayer definitions: 'newmis' has no place there and
  // 'force_retouch' is a word off.
  Assembler mp;
  std::vector<uint8_t> b = mp.bytes();
  const int32_t crc = kSinglePlayerCrc;
  for (int k = 0; k < 4; k++) b[4 + static_cast<size_t>(k)] = static_cast<uint8_t>(crc >> (8 * k));
  Progs p;
  std::string error;
  CHECK(!load_progs(b.data(), b.size(), p, error, every_form()));
  CHECK_MSG(has(error, "single-player layout"), error);
}

TEST(the_compiler_stamps_the_form_a_program_declares) {
  for (const Layout* form : kLayouts) {
    const std::vector<QcSource> src{{"defs.qc", declarations(*form)}};
    QcResult r = compile_qc(src);
    CHECK_MSG(r.ok, r.diagnostics.empty() ? "" : diagnostic_text(r.diagnostics.front()));
    CHECK_MSG(r.progs.crc == form->crc,
              std::string(form->name) + ": crc " + std::to_string(r.progs.crc));
    CHECK(r.progs.layout == form);
  }
}

TEST(state_writes_through_the_forms_offsets) {
  Assembler a(kSinglePlayerLayout);
  const int32_t fn = a.begin_function("tick", {}, 0);
  const int frame = a.constant_f(7);
  a.emit(kOpState, frame, a.constant_fn(fn));
  a.emit(kOpDone);
  a.end_function();
  Progs p;
  std::string error;
  CHECK_MSG(a.build(p, error), error);
  Vm vm;
  create_vm(vm, p);
  CHECK(vm.slots.nextthink == kSinglePlayerLayout.f.nextthink);
  CHECK(vm.slots.frame == kSinglePlayerLayout.f.frame);
  vm.num_entities = 2;
  set_global_int(vm, kSinglePlayerLayout.g.self, entity_value(vm, 1));
  set_global_float(vm, kSinglePlayerLayout.g.time, 2.0f);
  CHECK_MSG(call_function(vm, fn), fault_text(vm));
  CHECK(as_float(*entity_field(vm, 1, kSinglePlayerLayout.f.frame)) == 7.0f);
  CHECK(static_cast<int32_t>(*entity_field(vm, 1, kSinglePlayerLayout.f.think)) == fn);
  CHECK(as_float(*entity_field(vm, 1, kSinglePlayerLayout.f.nextthink)) == 2.1f);
}
