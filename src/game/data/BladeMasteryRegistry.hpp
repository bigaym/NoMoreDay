#pragma once

#include "game/data/BladeMasteryData.hpp"

#include <string>
#include <unordered_map>
#include <vector>

namespace NoMoreDay::data {

class BladeMasteryRegistry {
public:
  static BladeMasteryRegistry &Get();

  bool LoadFromJson(const std::string &path);
  bool Load();

  [[nodiscard]] const BladeMasteryProfile *GetProfile(BladeMasteryId id) const;
  [[nodiscard]] const std::vector<BladeMasteryProfile> &GetAllProfiles() const;

private:
  std::vector<BladeMasteryProfile> profiles_;
  std::unordered_map<BladeMasteryId, std::size_t> profile_index_;
};

} // namespace NoMoreDay::data
