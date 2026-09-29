#pragma once

#include <cstdint>
#include <optional>
#include <string>

namespace krkr2 {

/** Runtime-independent description of a Kirikiri game directory/archive. */
struct GamePath {
  std::string input;
  std::string root;
  bool is_xp3 = false;
  bool is_directory = false;
};

/**
 * Resolve a command line path and perform the inexpensive XP3 signature check.
 * This function does not extract or modify the game.
 */
bool resolve_game(const std::string &path, GamePath &result, std::string &error);

/** Linux host options.  The engine itself remains optional and can be loaded
 * from a shared object by applications embedding this host. */
struct HostOptions {
  int width = 960;
  int height = 640;
  bool fullscreen = false;
  bool probe_only = false;
  bool list_archive = false;
  bool run_startup = false;
  bool no_window = false;
  std::string engine_library;
  std::string game;
  std::optional<std::string> expression;
  std::optional<std::string> script;
  std::optional<std::string> cat_storage;
};

/** Run the native event loop. Returns a process exit status. */
int run_host(const HostOptions &options);

} // namespace krkr2
