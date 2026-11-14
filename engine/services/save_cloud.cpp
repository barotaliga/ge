// The store's cloud files as a save store's cloud (save_cloud.hpp).
//
// ge:summary The store's cloud files behind save::Cloud: each call and its refusal
// ge:keywords cloud save-cloud sync adapter
// ge:skill services
// ge:test tests/save_cloud_test.cpp
#include "services/save_cloud.hpp"

namespace ge::services {

bool SaveCloud::answer(Result r, const char* call, const std::string& name) {
  if (r == Result::ok) {
    problem_.clear();
    return true;
  }
  problem_ = std::string(call) + " " + name + " answered " + std::string(result_name(r));
  return false;
}

bool SaveCloud::list(std::vector<save::CloudFileInfo>& out) {
  out.clear();
  problem_.clear();
  backend_->cloud_files(files_);
  for (const CloudFile& f : files_) out.push_back({f.name, f.size});
  return true;
}

bool SaveCloud::read(const std::string& name, std::vector<uint8_t>& out) {
  return answer(backend_->cloud_read(name, out), "cloud_read", name);
}

bool SaveCloud::write(const std::string& name, std::span<const uint8_t> bytes) {
  return answer(backend_->cloud_write(name, bytes), "cloud_write", name);
}

bool SaveCloud::remove(const std::string& name) {
  return answer(backend_->cloud_delete(name), "cloud_delete", name);
}

}  // namespace ge::services
