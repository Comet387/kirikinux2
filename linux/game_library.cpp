#include "game_library.h"

#include <algorithm>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <utility>

namespace krkr2 {
namespace {

constexpr std::size_t kMaximumRecentGames = 50;

} // namespace

GameLibrary::GameLibrary(std::filesystem::path storage_path)
    : storage_path_(std::move(storage_path)) {}

std::filesystem::path GameLibrary::default_storage_path() {
  if (const char *override_path = std::getenv("KRKR2_CONFIG_HOME")) {
    if (*override_path)
      return std::filesystem::path(override_path) / "recent-games.txt";
  }
  if (const char *xdg_path = std::getenv("XDG_CONFIG_HOME")) {
    if (*xdg_path)
      return std::filesystem::path(xdg_path) / "kirikiroid2" /
             "recent-games.txt";
  }
  if (const char *home_path = std::getenv("HOME")) {
    if (*home_path)
      return std::filesystem::path(home_path) / ".config" / "kirikiroid2" /
             "recent-games.txt";
  }
  std::error_code ec;
  std::filesystem::path fallback = std::filesystem::temp_directory_path(ec);
  if (ec) fallback = std::filesystem::current_path(ec);
  if (ec) fallback = ".";
  return fallback / "kirikiroid2-recent-games.txt";
}

std::string GameLibrary::normalize_path(const std::string &path) {
  if (path.empty()) return {};
  std::error_code ec;
  std::filesystem::path normalized = std::filesystem::absolute(path, ec);
  if (ec) normalized = path;
  const std::filesystem::path canonical =
      std::filesystem::weakly_canonical(normalized, ec);
  if (!ec) normalized = canonical;
  return normalized.lexically_normal().string();
}

bool GameLibrary::load(std::string &error) {
  error.clear();
  games_.clear();
  std::ifstream input(storage_path_);
  if (!input) {
    std::error_code ec;
    if (!std::filesystem::exists(storage_path_, ec)) return true;
    error = "cannot open game library: " + storage_path_.string();
    return false;
  }

  bool removed_stale_entry = false;
  std::string line;
  while (std::getline(input, line)) {
    if (line.empty() || line.front() == '#') continue;
    std::istringstream parser(line);
    std::string path;
    if (!(parser >> std::quoted(path)) || path.empty()) continue;
    path = normalize_path(path);
    std::error_code ec;
    if (!std::filesystem::exists(path, ec)) {
      removed_stale_entry = true;
      continue;
    }
    if (std::find(games_.begin(), games_.end(), path) == games_.end())
      games_.push_back(std::move(path));
    if (games_.size() >= kMaximumRecentGames) break;
  }
  if (!input.eof()) {
    error = "cannot read game library: " + storage_path_.string();
    return false;
  }
  return !removed_stale_entry || save(error);
}

bool GameLibrary::remember(const std::string &path, std::string &error) {
  error.clear();
  const std::string normalized = normalize_path(path);
  if (normalized.empty()) {
    error = "game path is empty";
    return false;
  }
  std::error_code ec;
  if (!std::filesystem::exists(normalized, ec)) {
    error = "game path no longer exists: " + normalized;
    return false;
  }
  games_.erase(std::remove(games_.begin(), games_.end(), normalized),
               games_.end());
  games_.insert(games_.begin(), normalized);
  if (games_.size() > kMaximumRecentGames) games_.resize(kMaximumRecentGames);
  return save(error);
}

bool GameLibrary::forget(const std::string &path, std::string &error) {
  error.clear();
  const std::string normalized = normalize_path(path);
  const auto old_size = games_.size();
  games_.erase(std::remove(games_.begin(), games_.end(), normalized),
               games_.end());
  return old_size == games_.size() || save(error);
}

bool GameLibrary::save(std::string &error) const {
  error.clear();
  std::error_code ec;
  const std::filesystem::path parent = storage_path_.parent_path();
  if (!parent.empty()) {
    std::filesystem::create_directories(parent, ec);
    if (ec) {
      error = "cannot create configuration directory: " + parent.string();
      return false;
    }
  }

  std::filesystem::path temporary = storage_path_;
  temporary += ".tmp";
  {
    std::ofstream output(temporary, std::ios::trunc);
    if (!output) {
      error = "cannot write game library: " + temporary.string();
      return false;
    }
    output << "# Kirikiroid2 recent games v1\n";
    for (const std::string &game : games_) output << std::quoted(game) << '\n';
    output.flush();
    if (!output) {
      error = "cannot finish writing game library: " + temporary.string();
      return false;
    }
  }
  std::filesystem::rename(temporary, storage_path_, ec);
  if (ec) {
    error = "cannot replace game library: " + ec.message();
    std::filesystem::remove(temporary, ec);
    return false;
  }
  return true;
}

} // namespace krkr2
