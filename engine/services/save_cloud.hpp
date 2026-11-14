// A PLAYER'S CLOUD FILES AS A SAVE STORE'S CLOUD (docs/saves.md § Saves on every device): the
// store's cloud files (`Backend::cloud_write` and the rest) behind `save::Cloud`, so `save::sync`
// keeps a game's slots in step on every device the player plays on, whichever store it runs in.
//
// ge:summary The store's cloud files as a save::Cloud, for save::sync
// ge:keywords cloud save-cloud sync cloud-save adapter backend
// ge:skill services
// ge:test tests/save_cloud_test.cpp
#pragma once

#include <string>
#include <vector>

#include "save/cloud.hpp"
#include "services/backend.hpp"

namespace ge::services {

class SaveCloud final : public save::Cloud {
 public:
  // `backend` must outlive this.
  explicit SaveCloud(Backend& backend) : backend_(&backend) {}
  bool list(std::vector<save::CloudFileInfo>& out) override;
  bool read(const std::string& name, std::vector<uint8_t>& out) override;
  bool write(const std::string& name, std::span<const uint8_t> bytes) override;
  bool remove(const std::string& name) override;
  const std::string& problem() const override { return problem_; }

 private:
  bool answer(Result r, const char* call, const std::string& name);
  Backend* backend_;
  std::vector<CloudFile> files_;
  std::string problem_;
};

}  // namespace ge::services
