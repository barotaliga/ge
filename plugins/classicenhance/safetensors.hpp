// A NETWORK'S WEIGHTS FILE READ: the safetensors layout, which a trained upscaler is published in.
// The file is an 8-byte little-endian header length, a JSON header naming every tensor (its
// dtype, its shape and its byte range in the data that follows) and an optional `__metadata__`
// object of strings, then the tensors' bytes back to back. Only 32-bit float tensors are kept,
// copied out so they are aligned; a tensor of any other dtype (a network's integer index buffers,
// which the reader of the weights recomputes) is listed with its shape and no data. A file whose
// header does not parse, or whose ranges run outside the file, is refused with what was wrong.
//
// ge:summary The safetensors weights file read: the header, the float tensors, the metadata
// ge:keywords safetensors weights tensors network model file header metadata float32 dtype
// ge:skill classicenhance
// ge:test tests/classicenhance_upscaler_test.cpp
#pragma once

#include <cstdint>
#include <map>
#include <span>
#include <string>
#include <vector>

namespace ge::classicenhance {

struct Tensor {
  std::string dtype;           // as the header spells it: "F32", "I64", ...
  std::vector<int64_t> shape;  // outermost first; empty for a scalar
  std::vector<float> data;     // the values when dtype is F32, row-major; empty otherwise

  int64_t count() const;  // the product of the shape (1 for a scalar)
};

struct Weights {
  std::map<std::string, Tensor> tensors;
  std::map<std::string, std::string> metadata;  // `__metadata__`'s strings
};

// The weights in `file`; false with `error` (what, and where) when it is not a safetensors file.
bool read_safetensors(std::span<const uint8_t> file, Weights& out, std::string& error);

// A safetensors file of `weights`' float tensors and metadata (the tests' and labs' small
// networks are written with it).
std::vector<uint8_t> write_safetensors(const Weights& weights);

}  // namespace ge::classicenhance
