#include "game_library.h"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

int main() {
  std::error_code ec;
  const auto root = std::filesystem::temp_directory_path(ec) /
      "kirikiroid2-game-library-test";
  std::filesystem::remove_all(root, ec);
  std::filesystem::create_directories(root / "one", ec);
  std::filesystem::create_directories(root / "two", ec);
  if (ec) return 1;

  const auto database = root / "config" / "recent-games.txt";
  krkr2::GameLibrary library(database);
  std::string error;
  if (!library.load(error) ||
      !library.remember((root / "one").string(), error) ||
      !library.remember((root / "two").string(), error) ||
      !library.remember((root / "one").string(), error)) {
    std::cerr << error << '\n';
    return 2;
  }
  if (library.games().size() != 2 ||
      library.games().front() !=
          krkr2::GameLibrary::normalize_path((root / "one").string()))
    return 3;

  krkr2::GameLibrary loaded(database);
  if (!loaded.load(error) || loaded.games() != library.games()) return 4;
  if (!loaded.forget((root / "one").string(), error) ||
      loaded.games().size() != 1)
    return 5;

  std::filesystem::remove_all(root, ec);
  krkr2::GameLibrary cleaned(database);
  if (!cleaned.load(error) || !cleaned.games().empty()) return 6;
  return 0;
}
