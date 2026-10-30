// THE QC COMPILER'S WORKING STATE, shared by its three halves and by nothing else: the types, the
// symbols, the constants pool, the function being emitted and its block of locals, and the emit
// helpers. `qc_compile.cpp` reads declarations, `qc_statements.cpp` a function's body,
// `qc_expressions.cpp` an expression. Internal to the compiler: include `qc_compile.hpp`.
//
// ge:summary The QC compiler's internal state: types, symbols, constants, the function's block
// ge:keywords qc compiler-state symbol type-table local-block temporaries relocation val
// ge:keywords emit store_op load_op immediate
// ge:skill progs
// ge:test tests/progs_qc_compile_test.cpp
#pragma once

#include <map>
#include <string>
#include <string_view>
#include <vector>

#include "progs/layout.hpp"
#include "progs/opcodes.hpp"
#include "progs/qc_compile.hpp"
#include "progs/qc_lexer.hpp"

namespace ge::progs::qc {

// An operand at or above this names a word of the function's own block, relocated when the
// block is placed (`Compiler::finish_function`). Jumps are relative and far below it.
constexpr int kLocalBase = 1 << 20;

// A type: a value type, a field of a type, or a function of parameters returning a type.
struct Type {
  uint16_t base = kTypeVoid;  // layout.hpp's kType*
  int inner = -1;             // a field's value type; a function's return type
  std::vector<int> params;    // a function's parameter types
  bool varargs = false;       // a function ending in `...`
  bool operator==(const Type&) const = default;
};

struct Symbol {
  std::string name;
  int type = 0;
  int ofs = 0;            // the global (or the local, from kLocalBase)
  bool constant = false;  // an initialised global or a function: never assigned
  int function = -1;      // a function symbol's index in the functions
  bool defined = false;   // a function with a body or a builtin number
  int line = 0;
};

// An expression's value: a word (or three) of globals, or an entity's field.
struct Val {
  enum class Kind : uint8_t { global, field } kind = Kind::global;
  int type = 0;
  int ofs = 0;        // global: the value; field: the global holding the entity
  int field_ofs = 0;  // field: the global holding the field's offset
  bool assignable = false;
};

struct CompileError {
  int line;
  std::string message;
};

// A statement as emitted while its function is read: operands as ints, so a local's operand can
// carry kLocalBase until the block is placed.
struct Pending {
  uint16_t op;
  int a, b, c;
  int line;
};

struct Compiler {
  Progs prog;
  std::vector<QcDiagnostic> diagnostics;
  std::vector<Type> types;
  int t_void = 0, t_float = 0, t_vector = 0, t_string = 0, t_entity = 0, t_void_fn = 0;

  std::vector<Symbol> globals;
  std::map<std::string, int, std::less<>> global_by_name;
  std::map<std::string, int, std::less<>> strings;  // contents -> offset
  std::map<std::vector<uint32_t>, int> immediates;  // {type, words...} -> global
  int next_field = 0;
  bool claims_layout = false;  // `end_sys_fields` was declared: a layout mismatch warns

  // The source being read.
  std::string file;
  int file_name = 0;  // its string offset
  const std::vector<Token>* toks = nullptr;
  size_t pos = 0;

  // The function being emitted.
  int fn_return = 0;
  std::vector<Symbol> locals;
  std::map<std::string, int, std::less<>> local_by_name;
  std::vector<Pending> code;
  int block_words = 0;                              // words of the block so far
  std::vector<int> free1, free3;                    // released temporaries (block offsets)
  std::vector<int> temps1, temps3;                  // temporaries in use in this statement
  std::vector<std::vector<int>> breaks, continues;  // pending jumps, a list per open loop

  Compiler();

  // --- tokens (qc_compile.cpp) ---
  const Token& peek(size_t ahead = 0) const;
  const Token& next();
  bool is(std::string_view word, size_t ahead = 0) const;
  bool accept(std::string_view word);
  void expect(std::string_view word, std::string_view what);
  std::string expect_name(std::string_view what);
  [[noreturn]] void fail(std::string message) const;
  [[noreturn]] void fail_at(int line, std::string message) const;

  // --- types ---
  int intern(const Type& t);
  int field_of(int t);
  int function_of(int ret, std::vector<int> params, bool varargs);
  const Type& type(int t) const { return types[size_t(t)]; }
  uint16_t base(int t) const { return types[size_t(t)].base; }
  int size_of(int t) const { return base(t) == kTypeVector ? 3 : base(t) == kTypeVoid ? 0 : 1; }
  std::string type_name(int t) const;
  bool is_type_word(size_t ahead = 0) const;
  // A type; `names` gets a function type's parameter names (the last parameter list read).
  int parse_type(std::vector<std::string>* names);
  int parse_params(int ret, std::vector<std::string>* names);

  // --- layout ---
  int alloc_global(int words);
  int intern_string(const std::string& s);
  int immediate(uint16_t t, std::vector<uint32_t> words);
  int float_const(float f);
  void add_global_def(const std::string& name, int t, int ofs, bool saved);

  // --- declarations ---
  void declaration();
  int declare_global(const std::string& name, int t, int line);
  int declare_function(const std::string& name, int t, int line);
  void constant_value(int sym, int t);
  void function_body(int sym, const std::vector<std::string>& parm_names);
  void finish();

  // --- a function's body (qc_statements.cpp) ---
  int alloc_local(int words);
  int declare_local(const std::string& name, int t);
  int temp(int words);
  void release_temps();
  int emit(uint16_t op, int a, int b, int c);
  void patch_jump(int at, int target);  // the jump operand of statement `at` to `target`
  int here() const { return static_cast<int>(code.size()); }
  void block();
  void statement();
  void local_declaration();
  void jump_if_false(const Val& v, std::vector<int>& jumps);
  void jump_if_true(const Val& v, int target);
  void finish_function(int sym);

  // --- expressions (qc_expressions.cpp) ---
  const Symbol* lookup(std::string_view name) const;
  Val expression();
  Val assignment();
  Val binary(int min_prec);
  Val unary();
  Val postfix();
  Val primary();
  Val call(const Val& callee);
  Val member(const Val& left);
  int rv(const Val& v);  // the value's global (a field's loaded into a temporary)
  void store(const Val& target, int value_ofs, int value_type);
  Val binop(std::string_view op, const Val& l, const Val& r);
  int truth(int ofs, int t);  // a float 0 or 1: the value's truth
  Val temp_val(int t);
  void check_assign(int to, int from, std::string_view what) const;
};

uint16_t store_op(uint16_t t);
uint16_t storep_op(uint16_t t);
uint16_t load_op(uint16_t t);
uint16_t not_op(uint16_t t);

}  // namespace ge::progs::qc
