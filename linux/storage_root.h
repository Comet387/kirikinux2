#pragma once

#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

namespace krkr2 {

class Xp3Archive;

/** Read-only game storage rooted at a directory or one XP3 archive. */
class StorageRoot {
 public:
  StorageRoot();
  ~StorageRoot();
  StorageRoot(StorageRoot &&) noexcept;
  StorageRoot &operator=(StorageRoot &&) noexcept;
  StorageRoot(const StorageRoot &) = delete;
  StorageRoot &operator=(const StorageRoot &) = delete;

  bool open(const std::string &path, std::string &error);
  bool exists(const std::string &name) const;
  bool read(const std::string &name, std::vector<std::uint8_t> &data,
            std::string &error) const;
  std::string placed_path(const std::string &name) const;

 private:
  bool locate_file(const std::string &name, std::filesystem::path &path,
                   std::string *error) const;

  std::string source_path_;
  std::filesystem::path directory_;
  std::unique_ptr<Xp3Archive> archive_;
};

} // namespace krkr2
