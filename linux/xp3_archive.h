#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace krkr2 {

struct Xp3Segment {
  std::uint64_t start = 0;
  std::uint64_t original_size = 0;
  std::uint64_t archive_size = 0;
  bool compressed = false;
};

struct Xp3Entry {
  std::string name;
  std::uint32_t flags = 0;
  std::uint32_t hash = 0;
  std::uint64_t original_size = 0;
  std::uint64_t archive_size = 0;
  std::vector<Xp3Segment> segments;
};

/** Minimal, read-only XP3 index/stream implementation for Linux bring-up. */
class Xp3Archive {
 public:
  bool open(const std::string &path, std::string &error);
  bool read(const std::string &name, std::vector<std::uint8_t> &data,
            std::string &error) const;
  bool contains(const std::string &name) const;

  const std::vector<Xp3Entry> &entries() const { return entries_; }

 private:
  std::string path_;
  std::uint64_t archive_offset_ = 0;
  std::uint64_t file_size_ = 0;
  std::vector<Xp3Entry> entries_;
};

} // namespace krkr2
