// The opcode table (opcodes.hpp): each opcode's name and operand kinds, in number order.
//
// ge:summary The QC opcode table: names and operand kinds by number
// ge:keywords qc progs opcode table op_shape operand
// ge:skill progs
// ge:test tests/progs_vm_test.cpp
#include "progs/opcodes.hpp"

namespace ge::progs {

namespace {

constexpr Operand N = Operand::none;
constexpr Operand W = Operand::word;
constexpr Operand V = Operand::vector;
constexpr Operand J = Operand::jump;

constexpr OpShape kShapes[kOpCount] = {
    {"DONE", V, N, N},       {"MUL_F", W, W, W},      {"MUL_V", V, V, W},
    {"MUL_FV", W, V, V},     {"MUL_VF", V, W, V},     {"DIV_F", W, W, W},
    {"ADD_F", W, W, W},      {"ADD_V", V, V, V},      {"SUB_F", W, W, W},
    {"SUB_V", V, V, V},      {"EQ_F", W, W, W},       {"EQ_V", V, V, W},
    {"EQ_S", W, W, W},       {"EQ_E", W, W, W},       {"EQ_FNC", W, W, W},
    {"NE_F", W, W, W},       {"NE_V", V, V, W},       {"NE_S", W, W, W},
    {"NE_E", W, W, W},       {"NE_FNC", W, W, W},     {"LE", W, W, W},
    {"GE", W, W, W},         {"LT", W, W, W},         {"GT", W, W, W},
    {"LOAD_F", W, W, W},     {"LOAD_V", W, W, V},     {"LOAD_S", W, W, W},
    {"LOAD_ENT", W, W, W},   {"LOAD_FLD", W, W, W},   {"LOAD_FNC", W, W, W},
    {"ADDRESS", W, W, W},    {"STORE_F", W, W, N},    {"STORE_V", V, V, N},
    {"STORE_S", W, W, N},    {"STORE_ENT", W, W, N},  {"STORE_FLD", W, W, N},
    {"STORE_FNC", W, W, N},  {"STOREP_F", W, W, N},   {"STOREP_V", V, W, N},
    {"STOREP_S", W, W, N},   {"STOREP_ENT", W, W, N}, {"STOREP_FLD", W, W, N},
    {"STOREP_FNC", W, W, N}, {"RETURN", V, N, N},     {"NOT_F", W, N, W},
    {"NOT_V", V, N, W},      {"NOT_S", W, N, W},      {"NOT_ENT", W, N, W},
    {"NOT_FNC", W, N, W},    {"IF", W, J, N},         {"IFNOT", W, J, N},
    {"CALL0", W, N, N},      {"CALL1", W, N, N},      {"CALL2", W, N, N},
    {"CALL3", W, N, N},      {"CALL4", W, N, N},      {"CALL5", W, N, N},
    {"CALL6", W, N, N},      {"CALL7", W, N, N},      {"CALL8", W, N, N},
    {"STATE", W, W, N},      {"GOTO", J, N, N},       {"AND", W, W, W},
    {"OR", W, W, W},         {"BITAND", W, W, W},     {"BITOR", W, W, W},
};

}  // namespace

const OpShape& op_shape(uint16_t op) { return kShapes[op]; }

}  // namespace ge::progs
