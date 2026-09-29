#pragma once

#include <filesystem>
#include <string>
#include <vector>

namespace krkr2 {

class GameLibrary {
 public:
  explicit GameLibrary(std::filesystem::path storage_path);

  static std::filesystem::path default_storage_path();
  static std::string normalize_path(const std::string &path);

  bool load(std::string &error);
  bool remember(const std::string &path, std::string &error);
  bool forget(const std::string &path, std::string &error);

  const std::vector<std::string> &games() const { return games_; }
  const std::filesystem::path &storage_path() const { return storage_path_; }

 private:
  bool save(std::string &error) const;

  std::filesystem::path storage_path_;
  std::vector<std::string> games_;
};

} // namespace krkr2
