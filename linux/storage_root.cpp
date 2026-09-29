#include "storage_root.h"

#include "xp3_archive.h"

#include <algorithm>
#include <fstream>
#include <iterator>
#include <system_error>

namespace krkr2 {
namespace {

constexpr std::uintmax_t kMaxScriptStorageSize = 64ULL * 1024 * 1024;

char ascii_lower(char ch) {
  return ch >= 'A' && ch <= 'Z' ? static_cast<char>(ch - 'A' + 'a') : ch;
}

bool ascii_case_equal(const std::string &left, const std::string &right) {
  if (left.size() != right.size()) return false;
  for (std::size_t i = 0; i < left.size(); ++i) {
    if (ascii_lower(left[i]) != ascii_lower(right[i])) return false;
  }
  return true;
}

bool is_within(const std::filesystem::path &root,
               const std::filesystem::path &candidate) {
  auto root_part = root.begin();
  auto candidate_part = candidate.begin();
  for (; root_part != root.end(); ++root_part, ++candidate_part) {
    if (candidate_part == candidate.end() || *root_part != *candidate_part)
      return false;
  }
  return true;
}

bool normalize_relative(const std::string &name, std::filesystem::path &path,
                        std::string *error) {
  std::string portable = name;
  std::replace(portable.begin(), portable.end(), '\\', '/');
  while (!portable.empty() && portable.front() == '/') portable.erase(portable.begin());
  if (portable.empty() || portable.find('>') != std::string::npos ||
      portable.find(':') != std::string::npos) {
    if (error) *error = "invalid storage name: " + name;
    return false;
  }
  path.clear();
  for (const std::filesystem::path &component : std::filesystem::path(portable)) {
    if (component == "." || component.empty()) continue;
    if (component == "..") {
      if (error) *error = "storage path escapes the game root: " + name;
      return false;
    }
    path /= component;
  }
  if (path.empty()) {
    if (error) *error = "invalid storage name: " + name;
    return false;
  }
  return true;
}

} // namespace

StorageRoot::StorageRoot() = default;
StorageRoot::~StorageRoot() = default;
StorageRoot::StorageRoot(StorageRoot &&) noexcept = default;
StorageRoot &StorageRoot::operator=(StorageRoot &&) noexcept = default;

bool StorageRoot::open(const std::string &path, std::string &error) {
  source_path_.clear();
  directory_.clear();
  archive_.reset();

  std::error_code ec;
  const std::filesystem::path input(path);
  if (std::filesystem::is_directory(input, ec)) {
    directory_ = std::filesystem::canonical(input, ec);
    if (ec) {
      error = "cannot resolve game directory: " + path + ": " + ec.message();
      return false;
    }
  } else {
    auto archive = std::make_unique<Xp3Archive>();
    if (!archive->open(path, error)) return false;
    archive_ = std::move(archive);
  }
  source_path_ = path;
  return true;
}

bool StorageRoot::locate_file(const std::string &name, std::filesystem::path &path,
                              std::string *error) const {
  if (directory_.empty()) {
    if (error) *error = "storage root is not a directory";
    return false;
  }
  std::filesystem::path relative;
  if (!normalize_relative(name, relative, error)) return false;

  std::filesystem::path current = directory_;
  std::error_code ec;
  for (const std::filesystem::path &component : relative) {
    const std::filesystem::path exact = current / component;
    if (std::filesystem::exists(exact, ec) && !ec) {
      current = exact;
      continue;
    }
    ec.clear();
    bool found = false;
    if (!std::filesystem::is_directory(current, ec) || ec) break;
    for (std::filesystem::directory_iterator item(current, ec), end;
         !ec && item != end; item.increment(ec)) {
      if (ascii_case_equal(item->path().filename().string(), component.string())) {
        current = item->path();
        found = true;
        break;
      }
    }
    if (!found) {
      if (error) *error = "storage not found: " + name;
      return false;
    }
  }

  const std::filesystem::path resolved = std::filesystem::canonical(current, ec);
  if (ec || !is_within(directory_, resolved) ||
      !std::filesystem::is_regular_file(resolved, ec) || ec) {
    if (error) *error = "storage is not a readable file: " + name;
    return false;
  }
  path = resolved;
  return true;
}

bool StorageRoot::exists(const std::string &name) const {
  if (archive_) return archive_->contains(name);
  std::filesystem::path path;
  return locate_file(name, path, nullptr);
}

bool StorageRoot::read(const std::string &name, std::vector<std::uint8_t> &data,
                       std::string &error) const {
  if (archive_) return archive_->read(name, data, error);

  std::filesystem::path path;
  if (!locate_file(name, path, &error)) return false;
  std::error_code ec;
  const std::uintmax_t size = std::filesystem::file_size(path, ec);
  if (ec) {
    error = "cannot determine storage size: " + path.string();
    return false;
  }
  if (size > kMaxScriptStorageSize) {
    error = "storage exceeds the 64 MiB script limit: " + name;
    return false;
  }
  std::ifstream input(path, std::ios::binary);
  if (!input) {
    error = "cannot open storage: " + path.string();
    return false;
  }
  data.assign(std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>());
  if (!input.good() && !input.eof()) {
    error = "cannot read storage: " + path.string();
    return false;
  }
  return true;
}

std::string StorageRoot::placed_path(const std::string &name) const {
  if (!exists(name)) return {};
  if (archive_) return source_path_ + ">" + name;
  std::filesystem::path path;
  return locate_file(name, path, nullptr) ? path.string() : std::string();
}

} // namespace krkr2
